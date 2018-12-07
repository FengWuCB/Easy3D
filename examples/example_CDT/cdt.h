/*
*	Copyright (C) 2015 by Liangliang Nan (liangliang.nan@gmail.com)
*	https://3d.bk.tudelft.nl/liangliang/
*
*	This file is part of Easy3D: software for processing and rendering
*   meshes and point clouds.
*
*	Easy3D is free software; you can redistribute it and/or modify
*	it under the terms of the GNU General Public License Version 3
*	as published by the Free Software Foundation.
*
*	Easy3D is distributed in the hope that it will be useful,
*	but WITHOUT ANY WARRANTY; without even the implied warranty of
*	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*	GNU General Public License for more details.
*
*	You should have received a copy of the GNU General Public License
*	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/


#pragma once

#include <easy3d/window/main_window.h>

namespace easy3d {
	class LinesDrawable;
	class PointsDrawable;
	class ShaderProgram;
}

class Mapple : public easy3d::MainWindow
{
public:
	Mapple(int num_samples = 4, int gl_major = 3, int gl_minor = 2);

	void load_crosssection(const std::string& file_name, std::vector<vec3>& points);
	void compute_CDT(const std::vector<vec3>& points);

protected:
    void init() override;
    void draw() override;

    void cleanup() override;

private:
	easy3d::ShaderProgram*  program_;

	easy3d::LinesDrawable*	input_segments_;
	easy3d::PointsDrawable*	input_vertices_;

	easy3d::LinesDrawable*	CDT_segments_;
};

