#pragma once

#include "GL.hpp"
#include "Load.hpp"
#include "Scene.hpp"

struct SurfaceProgram {
	SurfaceProgram();
	~SurfaceProgram();

	GLuint program = 0;

    GLuint width = -1U;
    GLuint height = -1U;
};

extern Load< SurfaceProgram > surface_program;
