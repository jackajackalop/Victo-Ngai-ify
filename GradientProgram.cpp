#include "GradientProgram.hpp"

#include "gl_compile_program.hpp"
#include "gl_errors.hpp"

//Scene::Drawable::Pipeline gradient_program_pipeline;

Load< GradientProgram > gradient_program(LoadTagEarly, []() -> GradientProgram const * {
    return new GradientProgram();
});

GradientProgram::GradientProgram() {
	//Compile vertex and fragment shaders using the convenient 'gl_compile_program' helper function:
	program = gl_compile_program(
		//vertex shader:
		"#version 330\n"
		"void main() {\n"
		"	gl_Position = vec4(4*(gl_VertexID&1)-1, 2*(gl_VertexID&2)-1, 0.0, 1.0);\n"
		"}\n"
	,
		//fragment shader:
		"#version 330\n"
		"uniform sampler2D basic_tex;\n"
        "uniform sampler2D color_tex;\n"
		"layout(location=0) out vec4 gradient_out;\n"

		"void main() {\n"
        "   gradient_out = texelFetch(color_tex, ivec2(gl_FragCoord.xy), 0); \n"
		"}\n"
	);
	glUseProgram(program); //bind program -- glUniform* calls refer to this program now

	glUniform1i(glGetUniformLocation(program, "basic_tex"), 0);
    glUniform1i(glGetUniformLocation(program, "color_tex"), 1);

	glUseProgram(0); //unbind program -- glUniform* calls refer to ??? now
    GL_ERRORS();
}

GradientProgram::~GradientProgram() {
	glDeleteProgram(program);
	program = 0;
}

