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
        "uniform int width; \n"
        "uniform int height; \n"
        "layout(std430, binding=0) buffer wbuffer { uint wsum[]; }; \n"
        "layout(std430, binding=1) buffer hbuffer { uint hsum[]; }; \n"
        "layout(std430, binding=2) buffer w2buffer { uint w2sum[]; }; \n"
        "layout(std430, binding=3) buffer h2buffer { uint h2sum[]; }; \n"
        "layout(std430, binding=4) buffer whbuffer { uint whsum[]; }; \n"
        "layout(std430, binding=5) buffer wvbuffer { uint wvsum[]; }; \n"
        "layout(std430, binding=6) buffer hvbuffer { uint hvsum[]; }; \n"
        "layout(std430, binding=7) buffer vbuffer { uint vsum[]; }; \n"
        "layout(std430, binding=8) buffer nbuffer { uint n[]; }; \n"
        "layout( local_size_x = 1, local_size_y = 1 ) in; \n"

        "int pack_float(float v){ \n"
        "   return int(v*4096.0); \n"
        "} \n"

		"void main() {\n"
        "   ivec2 coord = ivec2(gl_GlobalInvocationID.xy); \n"
        "   vec4 color = texelFetch(color_tex, coord, 0); \n"
        "   int id = int(texelFetch(id_tex, coord, 0).r*255.0); \n"
        "   float w = float(gl_GlobalInvocationID.x)/width; \n"
        "   float h = float(gl_GlobalInvocationID.y)/height; \n"
        "   atomicAdd(wsum[id], pack_float(w)); \n"
        "   atomicAdd(hsum[id], pack_float(h)); \n"
        "   atomicAdd(w2sum[id], pack_float(w*w)); \n"
        "   atomicAdd(h2sum[id], pack_float(h*h)); \n"
        "   atomicAdd(whsum[id], pack_float(w*h)); \n"
        "   atomicAdd(wvsum[3*id], pack_float(w*color.r)); \n"
        "   atomicAdd(wvsum[3*id+1], pack_float(w*color.g)); \n"
        "   atomicAdd(wvsum[3*id+2], pack_float(w*color.b)); \n"
        "   atomicAdd(hvsum[3*id], pack_float(h*color.r)); \n"
        "   atomicAdd(hvsum[3*id+1], pack_float(h*color.g)); \n"
        "   atomicAdd(hvsum[3*id+2], pack_float(h*color.b)); \n"
        "   atomicAdd(vsum[3*id], pack_float(color.r)); \n"
        "   atomicAdd(vsum[3*id+1], pack_float(color.g)); \n"
        "   atomicAdd(vsum[3*id+2], pack_float(color.b)); \n"
        "   atomicAdd(n[id], 1); \n"
		"}\n"
        }}
	);
	glUseProgram(program); //bind program -- glUniform* calls refer to this program now

    width = glGetUniformLocation(program, "width");
    height = glGetUniformLocation(program, "height");

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
        "uniform int width; \n"
        "uniform int height; \n"
        "layout(std430, binding=0) buffer xbuffer { uint wsums_packed[]; }; \n"
        "layout(std430, binding=1) buffer ybuffer { uint hsums_packed[]; }; \n"
        "layout(std430, binding=2) buffer w2buffer { uint w2sums_packed[]; }; \n"
        "layout(std430, binding=3) buffer h2buffer { uint h2sums_packed[]; }; \n"
        "layout(std430, binding=4) buffer whbuffer { uint whsums_packed[]; }; \n"
        "layout(std430, binding=5) buffer wvbuffer { uint wvsums_packed[]; }; \n"
        "layout(std430, binding=6) buffer hvbuffer { uint hvsums_packed[]; }; \n"
        "layout(std430, binding=7) buffer vbuffer { uint vsums_packed[]; }; \n"
        "layout(std430, binding=8) buffer nbuffer { uint ns[]; }; \n"
		"layout(location=0) out vec4 gradient_out;\n"

        "float unpack_float(uint v){ \n"
        "   return float(v)/4096.0; \n"
        "} \n"

        "vec3 calculate_eq(int channel){ \n"
        "   ivec2 coord = ivec2(gl_FragCoord.xy); \n"
        "   int id = int(texelFetch(id_tex, coord, 0).r*255); \n"
        "   float wsum = unpack_float(wsums_packed[id]); \n"
        "   float hsum = unpack_float(hsums_packed[id]); \n"
        "   float w2sum = unpack_float(w2sums_packed[id]); \n"
        "   float h2sum = unpack_float(h2sums_packed[id]); \n"
        "   float whsum = unpack_float(whsums_packed[id]); \n"
        "   float wvsum = unpack_float(wvsums_packed[3*id+channel]); \n"
        "   float hvsum = unpack_float(hvsums_packed[3*id+channel]); \n"
        "   float vsum = unpack_float(vsums_packed[3*id+channel]); \n"
        "   float n = ns[id]; \n"

        "   vec3 RHS = vec3(wvsum, hvsum, vsum); \n"
        "   mat3 A = mat3(w2sum, whsum, wsum,"
        "                 whsum, h2sum, hsum,"
        "                 wsum, hsum, n); \n"
        "   return inverse(A)*RHS; \n"
        "} \n"

		"void main() {\n"
        "   ivec2 coord = ivec2(gl_FragCoord.xy); \n"
        "   vec4 color = texelFetch(color_tex, coord, 0); \n"
        "   int id = int(texelFetch(id_tex, coord, 0).r*255); \n"
        "   vec3 eqR = calculate_eq(0); \n"
        "   vec3 eqG = calculate_eq(1); \n"
        "   vec3 eqB = calculate_eq(2); \n"
        "   float normalizedX = float(coord.x)/width; \n"
        "   float normalizedY = float(coord.y)/height; \n"
        "   float gradient_valR = eqR.x*normalizedX+eqR.y*normalizedY+eqR.z; \n"
        "   float gradient_valG = eqG.x*normalizedX+eqG.y*normalizedY+eqG.z; \n"
        "   float gradient_valB = eqB.x*normalizedX+eqB.y*normalizedY+eqB.z; \n"
        "   gradient_out = vec4(gradient_valR, gradient_valG, gradient_valB, 1.0); \n"
		"}\n"
	);
	glUseProgram(program); //bind program -- glUniform* calls refer to this program now

    width = glGetUniformLocation(program, "width");
    height = glGetUniformLocation(program, "height");

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

