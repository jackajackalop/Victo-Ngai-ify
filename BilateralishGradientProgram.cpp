#include "BilateralishGradientProgram.hpp"

#include "gl_compile_program.hpp"
#include "gl_errors.hpp"

CPUGradientProgram::CPUGradientProgram() {
	program = gl_compile_program(
		"#version 330\n"
		"void main() {\n"
        "   gl_Position = vec4(4*(gl_VertexID & 1) -1, 2 * (gl_VertexID &2) -1, 0.0, 1.0);"
		"}\n"
		,
    	"#version 330\n"
		"uniform sampler2D gradient_tex;\n"
		"layout(location=0) out vec4 gradient_out;\n"

		"void main() {\n"
        "   gradient_out = texelFetch(gradient_tex, ivec2(gl_FragCoord.xy), 0); \n"
		"}\n"
	);
	glUseProgram(program);

    glUniform1i(glGetUniformLocation(program, "gradient_tex"), 0);

	glUseProgram(0);

	GL_ERRORS();
}

Load< CPUGradientProgram > cpu_gradient_program (LoadTagEarly, [](){
	return new CPUGradientProgram();
});
