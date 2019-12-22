#include "ShadingProgram.hpp"

#include "gl_compile_program.hpp"
#include "gl_errors.hpp"

Load< ShadingProgram > shading_program(LoadTagEarly, []() -> ShadingProgram const * {
    return new ShadingProgram();
});

ShadingProgram::ShadingProgram() {
	//Compile vertex and fragment shaders using the convenient 'gl_compile_program' helper function:
	program = gl_compile_program(
		//vertex shader:
		"#version 430\n"
		"void main() {\n"
		"	gl_Position = vec4(4*(gl_VertexID&1)-1, 2*(gl_VertexID&2)-1, 0.0, 1.0);\n"
		"}\n"
	,
		//fragment shader:
		"#version 430\n"
        "uniform sampler2D gradient_tex; \n"
        "uniform sampler2D toon_tex; \n"
		"layout(location=0) out vec4 shading_out;\n"

		"void main() {\n"
        "   ivec2 coord = ivec2(gl_FragCoord.xy); \n"
        "   vec4 gradient = texelFetch(gradient_tex, coord, 0); \n"
        "   vec4 toon = texelFetch(toon_tex, coord, 0); \n"
        "   shading_out = gradient-toon; \n"
		"}\n"
	);
	glUseProgram(program); //bind program -- glUniform* calls refer to this program now

    glUniform1i(glGetUniformLocation(program, "gradient_tex"), 0);
    glUniform1i(glGetUniformLocation(program, "toon_tex"), 1);
    //TODO shadow lut

	glUseProgram(0); //unbind program -- glUniform* calls refer to ??? now
    GL_ERRORS();
}

ShadingProgram::~ShadingProgram() {
	glDeleteProgram(program);
	program = 0;
}
