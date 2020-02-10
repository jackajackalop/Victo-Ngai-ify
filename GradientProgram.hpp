#pragma once

#include "GL.hpp"
#include "Load.hpp"
#include "Scene.hpp"

struct CalculateGradientProgram {
	CalculateGradientProgram();
	~CalculateGradientProgram();

	GLuint program = 0;

    GLuint width = -1U;
    GLuint height = -1U;
};

struct GradientProgram {
	GradientProgram();
	~GradientProgram();

	GLuint program = 0;

    GLuint width = -1U;
    GLuint height = -1U;
    GLuint lut_size = -1U;
};

extern Load< CalculateGradientProgram > calculate_gradient_program;
extern Load< GradientProgram > gradient_program;
