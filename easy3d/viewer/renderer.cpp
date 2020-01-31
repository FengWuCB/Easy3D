/**
 * Copyright (C) 2015 by Liangliang Nan (liangliang.nan@gmail.com)
 * https://3d.bk.tudelft.nl/liangliang/
 *
 * This file is part of Easy3D. If it is useful in your research/work,
 * I would be grateful if you show your appreciation by citing it:
 * ------------------------------------------------------------------
 *      Liangliang Nan.
 *      Easy3D: a lightweight, easy-to-use, and efficient C++
 *      library for processing and rendering 3D data. 2018.
 * ------------------------------------------------------------------
 * Easy3D is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License Version 3
 * as published by the Free Software Foundation.
 *
 * Easy3D is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


#include <easy3d/viewer/renderer.h>
#include <easy3d/viewer/drawable_points.h>
#include <easy3d/viewer/drawable_lines.h>
#include <easy3d/viewer/drawable_triangles.h>
#include <easy3d/viewer/setting.h>
#include <easy3d/viewer/tessellator.h>
#include <easy3d/core/random.h>

#include <cassert>


namespace easy3d {

    namespace renderer {


        void update_data(PointCloud* model, PointsDrawable* drawable) {
            assert(model);
            assert(drawable);

            // segmentation information has been stored as properties:
            //      - "v:primitive_type"  (one of PLANE, SPHERE, CYLINDER, CONE, TORUS, and UNKNOWN)
            //      - "v:primitive_index" (0, 1, 2...)
            auto primitive_type = model->get_vertex_property<int>("v:primitive_type");
            auto primitive_index = model->get_vertex_property<int>("v:primitive_index");
            if (primitive_type && primitive_index) { // model has segmentation information
                int num = 0;
                for (auto v : model->vertices())
                    num = std::max(num, primitive_index[v]);
                ++num;
                // assign each plane a unique color
                std::vector<vec3> color_table(num);
                for (auto& c : color_table)
                    c = random_color();

                std::vector<vec3> colors;
                for (auto v : model->vertices()) {
                    int idx = primitive_index[v];
                    if (primitive_type[v] == -1)
                        colors.push_back(vec3(0, 0, 0));
                    else
                        colors.push_back(color_table[idx]); // black for unkonwn type
                }
                drawable->update_color_buffer(colors);
                drawable->set_per_vertex_color(true);

                auto points = model->get_vertex_property<vec3>("v:point");
                drawable->update_vertex_buffer(points.vector());
                auto normals = model->get_vertex_property<vec3>("v:normal");
                if (normals)
                    drawable->update_normal_buffer(normals.vector());
            }

            else {
                auto points = model->get_vertex_property<vec3>("v:point");
                drawable->update_vertex_buffer(points.vector());
                auto normals = model->get_vertex_property<vec3>("v:normal");
                if (normals)
                    drawable->update_normal_buffer(normals.vector());
                auto colors = model->get_vertex_property<vec3>("v:color");
                if (colors) {
                    drawable->update_color_buffer(colors.vector());
                    drawable->set_per_vertex_color(true);
                }
                else {
                    drawable->set_default_color(setting::point_cloud_points_color);
                    drawable->set_per_vertex_color(false);
                }
            }
        }



        void update_data(SurfaceMesh* model, PointsDrawable* drawable) {
            auto points = model->get_vertex_property<vec3>("v:point");
            drawable->update_vertex_buffer(points.vector());
            drawable->set_default_color(setting::surface_mesh_vertices_color);
            drawable->set_per_vertex_color(false);
            drawable->set_point_size(setting::surface_mesh_vertices_point_size);
            drawable->set_impostor_type(PointsDrawable::SPHERE);
        }


        void update_data(SurfaceMesh* model, TrianglesDrawable* drawable) {
            assert(model);
            assert(drawable);

            /**
             * For non-triangular surface meshes, all polygonal faces are internally triangulated to allow a unified
             * rendering APIs. Thus for performance reasons, the selection of polygonal faces is also internally
             * implemented by selecting triangle primitives using program shaders. This allows data uploaded to the GPU
             * for the rendering purpose be shared for selection. Yeah, performance gain!
             */
            auto triangle_range = model->face_property< std::pair<int, int> >("f:triangle_range");
            int count_triangles = 0;

            // [Liangliang] How to achieve efficient switch between flat and smooth shading?
            //      Always transfer vertex normals to GPU and the normals for flat shading are
            //      computed on the fly in the fragment shader:
            //              normal = normalize(cross(dFdx(DataIn.position), dFdy(DataIn.position)));
            //     This way, we can use a uniform smooth_shading to switch between flat and smooth
            //     shading, avoiding transferring different data to the GPU.

            auto points = model->get_vertex_property<vec3>("v:point");
            model->update_vertex_normals();
            auto normals = model->get_vertex_property<vec3>("v:normal");

            Tessellator tessellator;

            auto face_colors = model->get_face_property<vec3>("f:color");
            if (face_colors) {  // rendering with per-face colors
                std::vector<vec3> d_points, d_normals, d_colors;
                for (auto face : model->faces()) {
                    tessellator.reset();
                    tessellator.begin_polygon(model->compute_face_normal(face));
                    tessellator.set_winding_rule(Tessellator::NONZERO);  // or POSITIVE
                    tessellator.begin_contour();
                    for (auto h : model->halfedges(face)) {
                        const vec3& v = points[model->to_vertex(h)];
                        const vec3& n = normals[model->to_vertex(h)];
                        const float data[6] = {v.x, v.y, v.z, n.x, n.y, n.z};
                        tessellator.add_vertex(data, 6);
                    }
                    tessellator.end_contour();
                    tessellator.end_polygon();

                    std::size_t num = tessellator.num_triangles();
                    const std::vector<const double *> &vts = tessellator.get_vertices();
                    for (std::size_t j = 0; j < num; ++j) {
                        std::size_t a, b, c;
                        tessellator.get_triangle(j, a, b, c);
                        d_points.emplace_back(vts[a]);   d_normals.emplace_back(vts[a] + 3);
                        d_points.emplace_back(vts[b]);   d_normals.emplace_back(vts[b] + 3);
                        d_points.emplace_back(vts[c]);   d_normals.emplace_back(vts[c] + 3);
                        d_colors.insert(d_colors.end(), 3, face_colors[face]);
                    }
                    triangle_range[face] = std::make_pair(count_triangles, count_triangles + num - 1);
                    count_triangles += num;
                }

                drawable->update_vertex_buffer(d_points);
                drawable->update_normal_buffer(d_normals);
                drawable->update_color_buffer(d_colors);
                drawable->set_per_vertex_color(true);
                drawable->release_element_buffer();
            }
            else { // rendering with per-vertex colors
                drawable->update_vertex_buffer(points.vector());
                auto colors = model->get_vertex_property<vec3>("v:color");
                if (colors) {
                    drawable->update_color_buffer(colors.vector());
                    drawable->set_per_vertex_color(true);
                }
                auto vertex_texcoords = model->get_vertex_property<vec2>("v:texcoord");
                if (vertex_texcoords)
                    drawable->update_texcoord_buffer(vertex_texcoords.vector());

                auto normals = model->get_vertex_property<vec3>("v:normal");
                if (!normals) {
                    model->update_vertex_normals();
                    normals = model->get_vertex_property<vec3>("v:normal");
                }

                drawable->update_normal_buffer(normals.vector());

                std::vector<unsigned int> indices;
                for (auto face : model->faces()) {
                    // we assume convex polygonal faces and we render in triangles
                    SurfaceMesh::Halfedge start = model->halfedge(face);
                    SurfaceMesh::Halfedge cur = model->next_halfedge(model->next_halfedge(start));
                    SurfaceMesh::Vertex va = model->to_vertex(start);
                    int num = 0;
                    while (cur != start) {
                        SurfaceMesh::Vertex vb = model->from_vertex(cur);
                        SurfaceMesh::Vertex vc = model->to_vertex(cur);
                        indices.push_back(static_cast<unsigned int>(va.idx()));
                        indices.push_back(static_cast<unsigned int>(vb.idx()));
                        indices.push_back(static_cast<unsigned int>(vc.idx()));
                        cur = model->next_halfedge(cur);
                        ++num;
                    }

                    triangle_range[face] = std::make_pair(count_triangles, count_triangles + num - 1);
                    count_triangles += num;
                }
                drawable->update_index_buffer(indices);
            }
        }


        void update_data(SurfaceMesh* model, LinesDrawable* drawable) {
            std::vector<unsigned int> indices;
            for (auto e : model->edges()) {
                SurfaceMesh::Vertex s = model->vertex(e, 0);
                SurfaceMesh::Vertex t = model->vertex(e, 1);
                indices.push_back(s.idx());
                indices.push_back(t.idx());
            }
            auto points = model->get_vertex_property<vec3>("v:point");
            drawable->update_vertex_buffer(points.vector());
            drawable->update_index_buffer(indices);
            drawable->set_default_color(setting::surface_mesh_edges_color);
            drawable->set_per_vertex_color(false);
            drawable->set_line_width(setting::surface_mesh_edges_line_width);
        }



        void update_data(Graph* model, PointsDrawable* drawable) {
            auto points = model->get_vertex_property<vec3>("v:point");
            drawable->update_vertex_buffer(points.vector());
            drawable->set_per_vertex_color(false);
            drawable->set_default_color(vec3(1.0f, 0.0f, 0.0f));
            drawable->set_point_size(15.0f);
            drawable->set_impostor_type(PointsDrawable::SPHERE);
        }


        void update_data(Graph* model, LinesDrawable* drawable) {
            auto points = model->get_vertex_property<vec3>("v:point");
            drawable->update_vertex_buffer(points.vector());

            std::vector<unsigned int> indices;
            for (auto e : model->edges()) {
                unsigned int s = model->from_vertex(e).idx();    indices.push_back(s);
                unsigned int t = model->to_vertex(e).idx();      indices.push_back(t);
            }
            drawable->update_index_buffer(indices);

            drawable->set_per_vertex_color(false);
            drawable->set_default_color(vec3(1.0f, 0.67f, 0.5f));
            drawable->set_line_width(3.0f);
            drawable->set_impostor_type(LinesDrawable::CYLINDER);
        }


    }

}