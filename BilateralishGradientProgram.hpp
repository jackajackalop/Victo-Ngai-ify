#include "GL.hpp"
#include "Load.hpp"

//CPUGradient*Program does a horizontal pass and a vertical pass of gaussian blur
//and bilateral blur using some macro trickery
struct CPUGradientProgram {
	//opengl program object:
	GLuint program = 0;

	//uniform locations:
	CPUGradientProgram();
};

extern Load< CPUGradientProgram > cpu_gradient_program;
