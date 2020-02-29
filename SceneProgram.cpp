#include "SceneProgram.hpp"

#include "gl_compile_program.hpp"
#include "gl_errors.hpp"

Scene::Drawable::Pipeline scene_program_pipeline;

Load< SceneProgram > scene_program(LoadTagEarly, []() -> SceneProgram const * {
	SceneProgram *ret = new SceneProgram();

	//----- build the pipeline template -----
	scene_program_pipeline.program = ret->program;

	scene_program_pipeline.OBJECT_TO_CLIP_mat4 = ret->OBJECT_TO_CLIP_mat4;
	scene_program_pipeline.OBJECT_TO_LIGHT_mat4x3 = ret->OBJECT_TO_LIGHT_mat4x3;
	scene_program_pipeline.NORMAL_TO_LIGHT_mat3 = ret->NORMAL_TO_LIGHT_mat3;

	return ret;
});

SceneProgram::SceneProgram() {
	//Compile vertex and fragment shaders using the convenient 'gl_compile_program' helper function:
	program = gl_compile_program(
		//vertex shader:
		"#version 330\n"
		"uniform mat4 OBJECT_TO_CLIP;\n"
		"uniform mat4x3 OBJECT_TO_LIGHT;\n"
		"uniform mat3 NORMAL_TO_LIGHT;\n"
        "uniform mat4 LIGHT_TO_SPOT;\n"
		"in vec4 Position;\n"
		"in vec3 GeoNormal;\n"
		"in vec3 ShadingNormal;\n"
		"in vec4 Color;\n"
		"in vec2 TexCoord;\n"
		"out vec3 position;\n"
		"out vec3 shadingNormal;\n"
		"out vec3 geoNormal;\n"
		"out vec4 color;\n"
		"out vec2 texCoord;\n"
		"out vec4 shadowCoord;\n"
		"void main() {\n"
		"	gl_Position = OBJECT_TO_CLIP * Position;\n"
		"	position = OBJECT_TO_LIGHT * Position;\n"
		"	shadingNormal = NORMAL_TO_LIGHT * ShadingNormal;\n"
		"	geoNormal = GeoNormal;\n"
		"	color = Color;\n"
		"	texCoord = TexCoord;\n"
        "   shadowCoord = LIGHT_TO_SPOT * vec4(position, 1.0); \n"
		"}\n"
	,
		//fragment shader:
		"#version 330\n"
        "uniform sampler2DShadow shadow_depth_tex; \n"
        "uniform sampler3D lut_tex; \n"
        "uniform sampler3D shadow_lut_tex; \n"
        "uniform int lut_size; \n"
        "uniform vec3 spot_position; \n"
        "uniform int id; \n"
		"in vec3 position;\n"
		"in vec3 geoNormal;\n"
		"in vec3 shadingNormal;\n"
		"in vec4 color;\n"
		"in vec2 texCoord;\n"
        "in vec4 shadowCoord; \n"
		"layout(location=0) out vec4 basic_out;\n"
		"layout(location=1) out vec4 color_out;\n"
        "layout(location=2) out vec4 id_out; \n"
        "layout(location=3) out vec4 normal_out; \n"
        "layout(location=4) out vec4 shadow_out; \n"
        "layout(location=5) out vec4 toon_out; \n"

		"void main() {\n"
        "   normal_out = vec4(geoNormal, 1.0); \n"
        "   float id_color = float(id)/255.0; \n"
        "   id_out = vec4(id_color, id_color, id_color, 1.0); \n"
		"	vec3 n = normalize(shadingNormal);\n"
		"	vec4 albedo = color;\n"
        "   vec3 light = vec3(0.0, 0.0, 0.0); \n"
        "   float nl = 0.0; \n"

        "   { \n"
        "       vec3 l = normalize(spot_position-position); \n"
		"	    nl = max(0.0, dot(n,l));\n"
		"   	float shadow = textureProj(shadow_depth_tex, shadowCoord);\n"
        "       shadow_out = vec4(0.0); \n"
        "       if(nl*shadow == 0.0) shadow_out = vec4(0.5, 0.5, 0.5, 1.0); \n"
		"	    light = mix(vec3(0,0,0), vec3(1.0,1.0,0.95), 0.5*nl+0.5);\n"
        "   } \n"

		"	basic_out = vec4(albedo.rgb*light, albedo.a);\n"
        "   color_out = basic_out; \n"
        "   vec3 scale = vec3(lut_size - 1.0)/lut_size; \n"
        "   vec3 offset = vec3(1.0/(2.0*lut_size)); \n"
        "   vec3 lut_color = texture(lut_tex, scale*color_out.rgb+offset).rgb; \n"
        "   vec3 shadow_lut_color = texture(shadow_lut_tex, scale*lut_color+offset).rgb; \n"
        "   color_out = vec4(lut_color, 1.0); \n"

         //toon shading
         "  if(nl<0.3) toon_out = vec4(shadow_lut_color, 1.0); \n"
         "  else toon_out = vec4(0.0, 0.0, 0.0, 0.0); \n"
		"}\n"
	);
	//As you can see above, adjacent strings in C/C++ are concatenated.
	// this is very useful for writing long shader programs inline.

	//look up the locations of vertex attributes:
	Position_vec4 = glGetAttribLocation(program, "Position");
	ShadingNormal_vec3 = glGetAttribLocation(program, "ShadingNormal");
	GeoNormal_vec3 = glGetAttribLocation(program, "GeoNormal");
	Color_vec4 = glGetAttribLocation(program, "Color");
	TexCoord_vec2 = glGetAttribLocation(program, "TexCoord");

	//look up the locations of uniforms:
	OBJECT_TO_CLIP_mat4 = glGetUniformLocation(program, "OBJECT_TO_CLIP");
	OBJECT_TO_LIGHT_mat4x3 = glGetUniformLocation(program, "OBJECT_TO_LIGHT");
	NORMAL_TO_LIGHT_mat3 = glGetUniformLocation(program, "NORMAL_TO_LIGHT");
	LIGHT_TO_SPOT = glGetUniformLocation(program, "LIGHT_TO_SPOT");
	spot_position = glGetUniformLocation(program, "spot_position");
    lut_size = glGetUniformLocation(program, "lut_size");
    id = glGetUniformLocation(program, "id");

	//set TEX to always refer to texture binding zero:
	glUseProgram(program); //bind program -- glUniform* calls refer to this program now

	glUniform1i(glGetUniformLocation(program, "shadow_depth_tex"), 0);
	glUniform1i(glGetUniformLocation(program, "lut_tex"), 1);
	glUniform1i(glGetUniformLocation(program, "shadow_lut_tex"), 2);

	glUseProgram(0); //unbind program -- glUniform* calls refer to ??? now
}

SceneProgram::~SceneProgram() {
	glDeleteProgram(program);
	program = 0;
}

