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
		"in vec3 position;\n"
		"in vec3 normal;\n"
		"in vec4 color;\n"
		"in vec2 texCoord;\n"
		"layout(location=0) out vec4 basic_out;\n"
		"layout(location=1) out vec4 color_out;\n"

        "vec3 rgb_to_hsv(float r, float g, float b){ \n"
        "   float hue = 0.0; \n"
        "   float saturation = 0.0; \n"
        "   float minVal = min(r, min(g, b)); \n"
        "   float maxVal = max(r, max(g, b)); \n"
        "   float value = maxVal; \n"
        "   if(maxVal != minVal){ \n"
        "       float valRange = maxVal-minVal; \n"
        "       if(maxVal == r) hue = (g-b)/valRange; \n"
        "       if(maxVal == g) hue = 2.0+(b-r)/valRange; \n"
        "       if(maxVal == b) hue = 4.0+(r-g)/valRange; \n"
        "       hue *= 60.0; \n"
        "       if(hue < 0.0) hue += 360.0; \n"
        "       if(value < 0.5) saturation = valRange/(maxVal+minVal); \n"
        "       else saturation = valRange/value; \n"
        "   } \n"
        "   return vec3(hue, saturation, value); \n"
        "} \n"

        "vec3 hsv_to_rgb(float hue, float saturation, float value){ \n"
        "   float h = hue/60.0; \n"
        "   int i = int(h); \n"
        "   float a = value*(1.0-saturation); \n"
        "   float b = value*(1.0-saturation*(h-i)); \n"
        "   float c = value*(1.0-saturation*(1.0-(h-i))); \n"
        "   if(i == 0) return vec3(value, c, a); \n"
        "   else if(i == 1) return vec3(b, value, a); \n"
        "   else if(i == 2) return vec3(a, value, c); \n"
        "   else if(i == 3) return vec3(a, b, value); \n"
        "   else if(i == 4) return vec3(c, a, value); \n"
        "   else if(i == 5) return vec3(value, a, b); \n"
        "   return vec3(0.0, 0.0, 0.0); \n"
        "} \n"

		"void main() {\n"
		"	vec3 n = normalize(normal);\n"
		"	vec3 l = normalize(vec3(0.1, 0.1, 1.0));\n"
		"	vec4 albedo = texture(TEX, texCoord) * color;\n"
		//simple hemispherical lighting model:
		"	vec3 light = mix(vec3(0.0,0.0,0.1), vec3(1.0,1.0,0.95), dot(n,l)*0.5+0.5);\n"
		"	basic_out = vec4(light*albedo.rgb, albedo.a);\n"
        "   color_out = albedo; \n"

        "   vec3 hsv = rgb_to_hsv(albedo.r, albedo.g, albedo.b); \n"
        "   if(hsv[0] > 100.0) hsv[0] += 50.0; \n"
        "   vec3 rgb = hsv_to_rgb(hsv[0], hsv[1], hsv[2]); \n"
        "   color_out = vec4(rgb.r, rgb.g, rgb.b, 1.0); \n"
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
	GLuint TEX_sampler2D = glGetUniformLocation(program, "TEX");

	//set TEX to always refer to texture binding zero:
	glUseProgram(program); //bind program -- glUniform* calls refer to this program now

	glUniform1i(TEX_sampler2D, 0); //set TEX to sample from GL_TEXTURE0

	glUseProgram(0); //unbind program -- glUniform* calls refer to ??? now
}

SceneProgram::~SceneProgram() {
	glDeleteProgram(program);
	program = 0;
}
