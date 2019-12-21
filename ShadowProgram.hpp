#pragma once

#include "GL.hpp"
#include "Load.hpp"

//Shader program that draws transformed, lit, textured vertices tinted with vertex colors:
struct ShadowProgram {
	ShadowProgram();
	~ShadowProgram();

	GLuint program = 0;

	//Uniform (per-invocation variable) locations:
	GLuint OBJECT_TO_CLIP_mat4 = -1U;
};

extern Load< ShadowProgram > shadow_program;

//For convenient scene-graph setup, copy this object:
// NOTE: by default, has texture bound to 1-pixel white texture -- so it's okay to use with vertex-color-only meshes.
