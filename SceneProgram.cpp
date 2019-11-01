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

	//make a 1-pixel white texture to bind by default:
	GLuint tex;
	glGenTextures(1, &tex);

	glBindTexture(GL_TEXTURE_2D, tex);
	std::vector< glm::u8vec4 > tex_data(1, glm::u8vec4(0xff));
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex_data.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);

	scene_program_pipeline.textures[0].texture = tex;
	scene_program_pipeline.textures[0].target = GL_TEXTURE_2D;

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
		"in vec4 Position;\n"
		"in vec3 Normal;\n"
		"in vec4 Color;\n"
		"in vec2 TexCoord;\n"
		"out vec3 position;\n"
		"out vec3 normal;\n"
		"out vec4 color;\n"
		"out vec2 texCoord;\n"
		"void main() {\n"
		"	gl_Position = OBJECT_TO_CLIP * Position;\n"
		"	position = OBJECT_TO_LIGHT * Position;\n"
		"	normal = NORMAL_TO_LIGHT * Normal;\n"
		"	color = Color;\n"
		"	texCoord = TexCoord;\n"
		"}\n"
	,
		//fragment shader:
		"#version 330\n"
		"uniform sampler2D TEX;\n"
        "uniform sampler3D lut_tex; \n"
        "uniform int lut_size; \n"
        "uniform int id; \n"
		"in vec3 position;\n"
		"in vec3 normal;\n"
		"in vec4 color;\n"
		"in vec2 texCoord;\n"
		"layout(location=0) out vec4 basic_out;\n"
		"layout(location=1) out vec4 color_out;\n"
        "layout(location=2) out vec4 id_out; \n"

		"void main() {\n"
        "   float id_color = float(id)/255.0; \n"
        "   id_out = vec4(id_color, id_color, id_color, 1.0); \n"
		"	vec3 n = normalize(normal);\n"
		"	vec3 l = normalize(vec3(0.1, 0.1, 1.0));\n"
		"	vec4 albedo = texture(TEX, texCoord) * color;\n"
		//simple hemispherical lighting model:
		"	vec3 light = mix(vec3(0.0,0.0,0.1), vec3(1.0,1.0,0.95), dot(n,l)*0.5+0.5);\n"
        "   color_out = albedo; \n"
		"	basic_out = vec4(light*albedo.rgb, albedo.a);\n"
        "   vec3 scale = vec3(lut_size - 1.0)/lut_size; \n"
        "   vec3 offset = vec3(1.0/(2.0*lut_size)); \n"
        "   vec3 lut_color = texture(lut_tex, scale*albedo.rgb+offset).rgb; \n"
        "   color_out = vec4(lut_color, 1.0); \n"
		"}\n"
	);
	//As you can see above, adjacent strings in C/C++ are concatenated.
	// this is very useful for writing long shader programs inline.

	//look up the locations of vertex attributes:
	Position_vec4 = glGetAttribLocation(program, "Position");
	Normal_vec3 = glGetAttribLocation(program, "Normal");
	Color_vec4 = glGetAttribLocation(program, "Color");
	TexCoord_vec2 = glGetAttribLocation(program, "TexCoord");

	//look up the locations of uniforms:
	OBJECT_TO_CLIP_mat4 = glGetUniformLocation(program, "OBJECT_TO_CLIP");
	OBJECT_TO_LIGHT_mat4x3 = glGetUniformLocation(program, "OBJECT_TO_LIGHT");
	NORMAL_TO_LIGHT_mat3 = glGetUniformLocation(program, "NORMAL_TO_LIGHT");
    lut_size = glGetUniformLocation(program, "lut_size");
    id = glGetUniformLocation(program, "id");
	GLuint TEX_sampler2D = glGetUniformLocation(program, "TEX");

	//set TEX to always refer to texture binding zero:
	glUseProgram(program); //bind program -- glUniform* calls refer to this program now

	glUniform1i(TEX_sampler2D, 0); //set TEX to sample from GL_TEXTURE0
	glUniform1i(glGetUniformLocation(program, "lut_tex"), 1);

	glUseProgram(0); //unbind program -- glUniform* calls refer to ??? now
}

SceneProgram::~SceneProgram() {
	glDeleteProgram(program);
	program = 0;
}

