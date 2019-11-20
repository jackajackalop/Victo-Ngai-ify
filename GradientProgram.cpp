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
        "layout(std430, binding=0) buffer xbuffer { uint xsum[]; }; \n"
        "layout(std430, binding=1) buffer ybuffer { uint ysum[]; }; \n"
        "layout(std430, binding=2) buffer xybuffer { uint xysum[]; }; \n"
        "layout(std430, binding=3) buffer x2buffer { uint x2sum[]; }; \n"
        "layout(std430, binding=4) buffer y2buffer { uint y2sum[]; }; \n"
        "layout(std430, binding=5) buffer nbuffer { uint n[]; }; \n"
        "layout( local_size_x = 1, local_size_y = 1 ) in; \n"
		"void main() {\n"
        "   ivec2 coord = ivec2(gl_GlobalInvocationID.xy); \n"
        "   vec4 color = texelFetch(color_tex, coord, 0); \n"
        "   int id = int(texelFetch(id_tex, coord, 0).r*255.0); \n"
        "   uint height = gl_GlobalInvocationID.y; \n"
        "   float value = max(max(color.r, color.g), color.b); \n"
        "   atomicAdd(xsum[id], height); \n"
        "   atomicAdd(ysum[id], int(value*256)); \n"
        "   atomicAdd(xysum[id], int(height*value)); \n"
        "   atomicAdd(x2sum[id], height*height); \n"
        "   atomicAdd(y2sum[id], int(value*value*256)); \n"
        "   atomicAdd(n[id], 1); \n"
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
		"#version 430\n"
		"void main() {\n"
		"	gl_Position = vec4(4*(gl_VertexID&1)-1, 2*(gl_VertexID&2)-1, 0.0, 1.0);\n"
		"}\n"
	,
		//fragment shader:
		"#version 430\n"

        "uniform sampler2D color_tex; \n"
        "uniform sampler2D id_tex; \n"
        "layout(std430, binding=0) buffer xbuffer { uint xsums[]; }; \n"
        "layout(std430, binding=1) buffer ybuffer { uint ysums_packed[]; }; \n"
        "layout(std430, binding=2) buffer xybuffer { uint xysums_packed[]; }; \n"
        "layout(std430, binding=3) buffer x2buffer { uint x2sums[]; }; \n"
        "layout(std430, binding=4) buffer y2buffer { uint y2sums_packed[]; }; \n"
        "layout(std430, binding=5) buffer nbuffer { int ns[]; }; \n"
		"layout(location=0) out vec4 gradient_out;\n"

		"void main() {\n"
        "   ivec2 coord = ivec2(gl_FragCoord.xy); \n"
        "   vec4 color = texelFetch(color_tex, coord, 0); \n"
        "   int id = int(texelFetch(id_tex, coord, 0).r*255); \n"
        "   float ysum = float(ysums_packed[id])/256.0; \n"
        "   float xysum = float(xysums_packed[id]); \n"
        "   float xsum = xsums[id]; \n"
        "   float x2sum = x2sums[id]; \n"
        "   float n = ns[id]; \n"

        "   vec2 RHS = vec2(ysum, xysum); \n"
        "   mat2 A = mat2(xsum, x2sum, n, xsum); \n"
        "   vec2 soln = inverse(A)*RHS; \n"
        "   float slope = soln.x; \n"
        "   float intercept = soln.y; \n"
        "   float gradient_val = slope*gl_FragCoord.y+intercept; \n"
        "   gradient_out = vec4((color.rgb*gradient_val), 1.0); \n"
      //  "   gradient_out = vec4(gradient_val, gradient_val, gradient_val, 1.0); \n"
        //"   gradient_out = vec4(log(xysum)/30.0, 0, 0, 1.0); \n"
		"}\n"
	);
	glUseProgram(program); //bind program -- glUniform* calls refer to this program now

    glUniform1i(glGetUniformLocation(program, "color_tex"), 0);
    glUniform1i(glGetUniformLocation(program, "id_tex"), 1);


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

