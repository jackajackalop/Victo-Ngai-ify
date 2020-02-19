#pragma once

#include "GL.hpp"
#include "Load.hpp"
#include "Scene.hpp"

struct ShadowDebugProgram {
	ShadowDebugProgram();
	~ShadowDebugProgram();

	GLuint program = 0;

    GLuint width = -1U;
    GLuint height = -1U;
};

extern Load< ShadowDebugProgram > shadow_debug_program;

struct SurfaceProgram {
	SurfaceProgram();
	~SurfaceProgram();

	GLuint program = 0;

    GLuint width = -1U;
    GLuint height = -1U;
};

extern Load< SurfaceProgram > surface_program;
