#pragma once

#include "GL.hpp"
#include "Load.hpp"
#include "Scene.hpp"

struct GradientProgram {
	GradientProgram();
	~GradientProgram();

	GLuint program = 0;
};

extern Load< GradientProgram > gradient_program;
