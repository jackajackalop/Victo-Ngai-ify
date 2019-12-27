#pragma once

#include "GL.hpp"
#include "Load.hpp"
#include "Scene.hpp"

struct CombineProgram {
	CombineProgram();
	~CombineProgram();

	GLuint program = 0;

    GLuint width = -1U;
    GLuint height = -1U;
};

extern Load< CombineProgram > combine_program;
