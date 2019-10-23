#include "GL.hpp"
#include "Load.hpp"

//BilateralishGradient*Program does a horizontal pass and a vertical pass of gaussian blur
//and bilateral blur using some macro trickery
struct BilateralishGradientHProgram {
	//opengl program object:
	GLuint program = 0;

	//uniform locations:
    GLuint weights = -1U;
    GLuint blur_amount = -1U;
    GLuint depth_threshold = -1U;
	BilateralishGradientHProgram();
};

struct BilateralishGradientVProgram {
	//opengl program object:
	GLuint program = 0;

	//uniform locations:
    GLuint weights = -1U;
    GLuint blur_amount = -1U;
    GLuint depth_threshold = -1U;
	BilateralishGradientVProgram();
};
extern Load< BilateralishGradientHProgram > bilateralish_gradientH_program;
extern Load< BilateralishGradientVProgram > bilateralish_gradientV_program;
