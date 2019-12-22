#pragma once

#include "GL.hpp"
#include "Load.hpp"
#include "Scene.hpp"

struct ShadingProgram {
	ShadingProgram();
	~ShadingProgram();

	GLuint program = 0;

    GLuint width = -1U;
    GLuint height = -1U;
};

extern Load< ShadingProgram > shading_program;
