#include "ShadowProgram.hpp"

#include "gl_compile_program.hpp"
#include "gl_errors.hpp"

Load< ShadowProgram > shadow_program(LoadTagEarly, []() -> ShadowProgram const * {
	ShadowProgram *ret = new ShadowProgram();
	return ret;
});

ShadowProgram::ShadowProgram() {
	//Compile vertex and fragment shaders using the convenient 'gl_compile_program' helper function:
	program = gl_compile_program(
		//vertex shader:
		"#version 330\n"
		"uniform mat4 OBJECT_TO_CLIP;\n"
		"in vec4 Position;\n"
		"in vec3 Normal;\n"
		"out vec3 color;\n"
		"void main() {\n"
		"	gl_Position = OBJECT_TO_CLIP * Position;\n"
        "   color = 0.5+0.5*Normal; \n"
		"}\n"
	,
		//fragment shader:
		"#version 330\n"
		"in vec3 color;\n"
		"out vec4 color_out;\n"

		"void main() {\n"
        "   color_out = vec4(color, 1.0); \n"
		"}\n"
	);

    //look up the locations of uniforms:
	OBJECT_TO_CLIP_mat4 = glGetUniformLocation(program, "OBJECT_TO_CLIP");
}

ShadowProgram::~ShadowProgram() {
	glDeleteProgram(program);
	program = 0;
}

