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
    GLuint depth_gradient_extent = -1U;
    GLuint depth_gradient_brightness = -1U;
    GLuint lod = -1U;
    GLuint shadow_fade = -1U;
    GLuint shadow_extent = -1U;
    GLuint line_depth_threshold = -1U;
    GLuint line_normal_threshold = -1U;
};

extern Load< CalculateGradientProgram > calculate_gradient_program;
extern Load< SimplifyProgram > simplify_program;
