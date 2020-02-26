#pragma once

#include "GL.hpp"
#include "Load.hpp"

struct ShadowDebugProgram {
	ShadowDebugProgram();
	~ShadowDebugProgram();

	GLuint program = 0;
};

extern Load< ShadowDebugProgram > shadow_debug_program;

struct SurfaceProgram {
	SurfaceProgram();
	~SurfaceProgram();

	GLuint program = 0;
};

extern Load< SurfaceProgram > surface_program;
