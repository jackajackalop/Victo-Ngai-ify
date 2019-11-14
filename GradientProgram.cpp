#include "GradientProgram.hpp"

#include "gl_compile_program.hpp"
#include "gl_errors.hpp"

Load< CalculateGradientProgram > calculate_gradient_program
                    (LoadTagEarly, []() -> CalculateGradientProgram const * {
    return new CalculateGradientProgram();
});

Load< GradientProgram > gradient_program(LoadTagEarly, []() -> GradientProgram const * {
    return new GradientProgram();
});

CalculateGradientProgram::CalculateGradientProgram() {
	//Compile vertex and fragment shaders using the convenient 'gl_compile_program' helper function:
	program = gl_compile_program({ {GL_COMPUTE_SHADER,
		"#version 430\n"
        "uniform sampler2D color_tex; \n"
        "uniform sampler2D id_tex; \n"
        "layout(std430, binding=0) buffer xbuffer { float xsum[]; }; \n"
        "layout(std430, binding=1) buffer ybuffer { float ysum[]; }; \n"
        "layout(std430, binding=2) buffer xybuffer { float xysum[]; }; \n"
        "layout(std430, binding=3) buffer x2buffer { float x2sum[]; }; \n"
        "layout(std430, binding=4) buffer y2buffer { float y2sum[]; }; \n"
        "layout(std430, binding=5) buffer nbuffer { int n[]; }; \n"
        "layout( local_size_x = 128, local_size_y = 1, local_size_z = 1 ) in; \n"
		"void main() {\n"
        "   ivec2 coord = ivec2(gl_GlobalInvocationID.xy); \n"
        "   vec4 color = texelFetch(color_tex, coord, 0); \n"
        "   int id = int(texelFetch(id_tex, coord, 0).r*255); \n"
        "   uint height = gl_GlobalInvocationID.y; \n"
        "   float value = max(max(color.r, color.g), color.b); \n"
        "   xsum[id] += height;"
        "   ysum[id] += value;"
        "   xysum[id] += height*value;"
        "   x2sum[id] += height*height;"
        "   y2sum[id] += value*value;"
        "   n[id] += 1;"
		"}\n"
        }}
	);
	glUseProgram(program); //bind program -- glUniform* calls refer to this program now

    glUniform1i(glGetUniformLocation(program, "color_tex"), 0);
    glUniform1i(glGetUniformLocation(program, "id_tex"), 1);

	glUseProgram(0); //unbind program -- glUniform* calls refer to ??? now
    GL_ERRORS();
}

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
		"layout(location=0) out vec4 gradient_out;\n"

		"void main() {\n"
        "   gradient_out = vec4(1.0, 1.0, 1.0, 1.0); \n"
		"}\n"
	);
	glUseProgram(program); //bind program -- glUniform* calls refer to this program now

	glUseProgram(0); //unbind program -- glUniform* calls refer to ??? now
    GL_ERRORS();
}

CalculateGradientProgram::~CalculateGradientProgram() {
	glDeleteProgram(program);
	program = 0;
}

GradientProgram::~GradientProgram() {
	glDeleteProgram(program);
	program = 0;
}

