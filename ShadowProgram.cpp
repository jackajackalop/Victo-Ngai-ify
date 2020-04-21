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
        "layout (location = 5) in vec4 ControlColor; \n"
        "out vec4 controlColor; \n"
		"void main() {\n"
		"	gl_Position = OBJECT_TO_CLIP * Position;\n"
        "   controlColor = ControlColor; \n"
		"}\n"
	,
		//fragment shader:
		"#version 330\n"
        "in vec4 controlColor; \n"

		"void main() {\n"
        "   if(controlColor.g > 0) discard;"
		"}\n"
	);

    //look up the locations of uniforms:
	OBJECT_TO_CLIP_mat4 = glGetUniformLocation(program, "OBJECT_TO_CLIP");
	ControlColor_vec4 = glGetAttribLocation(program, "ControlColor");
}

ShadowProgram::~ShadowProgram() {
	glDeleteProgram(program);
	program = 0;
}

