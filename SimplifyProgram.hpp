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

struct SimplifyProgram {
	SimplifyProgram();
	~SimplifyProgram();

	GLuint program = 0;

    GLuint width = -1U;
    GLuint height = -1U;
    GLuint lut_size = -1U;
};

extern Load< CalculateGradientProgram > calculate_gradient_program;
extern Load< SimplifyProgram > simplify_program;
