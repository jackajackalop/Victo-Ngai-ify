#pragma once

#include "GL.hpp"
#include "Load.hpp"
#include "Scene.hpp"

struct CalculateGradientProgram {
	CalculateGradientProgram();
	~CalculateGradientProgram();

	GLuint program = 0;
};

struct GradientProgram {
	GradientProgram();
	~GradientProgram();

	GLuint program = 0;
};

extern Load< CalculateGradientProgram > calculate_gradient_program;
extern Load< GradientProgram > gradient_program;
