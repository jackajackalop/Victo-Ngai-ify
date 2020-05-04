#pragma once

#include "GL.hpp"
#include "Load.hpp"
#include "Scene.hpp"

//Shader program that draws transformed, lit, textured vertices tinted with vertex colors:
struct SceneProgram {
	SceneProgram();
	~SceneProgram();

	GLuint program = 0;

	//Attribute (per-vertex variable) locations:
	GLuint Position_vec4 = -1U;
	GLuint GeoNormal_vec3 = -1U;
	GLuint ShadingNormal_vec3 = -1U;
	GLuint Color_vec4 = -1U;
	GLuint ControlColor_vec4 = -1U;
	GLuint LineColor_vec4 = -1U;
	GLuint TexColor_vec4 = -1U;
	GLuint TexCoord_vec2 = -1U;

	//Uniform (per-invocation variable) locations:
    GLuint OBJECT_TO_CLIP_mat4 = -1U;
	GLuint OBJECT_TO_LIGHT_mat4x3 = -1U;
	GLuint NORMAL_TO_LIGHT_mat3 = -1U;
	GLuint LIGHT_TO_SPOT = -1U;
	GLuint spot_position = -1U;
    GLuint lut_size = -1U;
    GLuint id = -1U;
    GLuint toon_threshold = -1U;
    GLuint contrast = -1U;
};

extern Load< SceneProgram > scene_program;

//For convenient scene-graph setup, copy this object:
// NOTE: by default, has texture bound to 1-pixel white texture -- so it's okay to use with vertex-color-only meshes.
extern Scene::Drawable::Pipeline scene_program_pipeline;

struct TranspProgram {
	TranspProgram();
	~TranspProgram();

	GLuint program = 0;

	//Attribute (per-vertex variable) locations:
	GLuint Position_vec4 = -1U;
	GLuint GeoNormal_vec3 = -1U;
	GLuint ShadingNormal_vec3 = -1U;
	GLuint Color_vec4 = -1U;
	GLuint ControlColor_vec4 = -1U;
	GLuint LineColor_vec4 = -1U;
	GLuint TexColor_vec4 = -1U;
	GLuint TexCoord_vec2 = -1U;

	//Uniform (per-invocation variable) locations:
	GLuint OBJECT_TO_CLIP = -1U;
	GLuint OBJECT_TO_LIGHT = -1U;
	GLuint NORMAL_TO_LIGHT = -1U;
	GLuint LIGHT_TO_SPOT = -1U;
	GLuint spot_position = -1U;
    GLuint lut_size = -1U;
    GLuint id = -1U;
};

extern Load< TranspProgram > transp_program;

//For convenient scene-graph setup, copy this object:
// NOTE: by default, has texture bound to 1-pixel white texture -- so it's okay to use with vertex-color-only meshes.
extern Scene::Drawable::Pipeline transp_program_pipeline;
