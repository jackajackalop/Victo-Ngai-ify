#include "CombineProgram.hpp"

#include "gl_compile_program.hpp"
#include "gl_errors.hpp"

Load< CombineProgram > combine_program(LoadTagEarly, []() -> CombineProgram const * {
    return new CombineProgram();
});

CombineProgram::CombineProgram() {
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
        "uniform sampler2D id_tex; \n"
        "uniform sampler2D gradient_tex; \n"
        "uniform sampler2D gradient_toon_tex; \n"
        "uniform sampler2D line_tex; \n"
        "uniform sampler2D surface_tex; \n"
        "layout (std430, binding=0) buffer nbuffer{ uint n_sum[]; }; \n"
		"layout(location=0) out vec4 combine_out;\n"

        "vec4 pow_col(vec4 base, float exp){ \n"
        "   return vec4(pow(base.r, exp), pow(base.g, exp), pow(base.b, exp), 1.0); \n"
        "} \n"

		"void main() {\n"
        "   ivec2 coord = ivec2(gl_FragCoord.xy); \n"
        "   int id = int(texelFetch(id_tex, coord, 0)*255.0); \n"
        "   uint n = n_sum[id]; \n"

        //paper distortion
        "   int TEXTURE_LIMIT = 1000; \n"
        "   vec4 surfaceColor = texelFetch(surface_tex, coord, 0); \n"
        "   if(n<TEXTURE_LIMIT) surfaceColor = vec4(0.0, 0.0, 0.0, 0.0); \n"
        "   vec2 shift_amt = surfaceColor.gb; \n"
        "   ivec2 shifted_coord = ivec2(coord+shift_amt); \n"

        "   vec4 gradient = texelFetch(gradient_tex, shifted_coord, 0); \n"
        "   vec4 gradient_toon = texelFetch(gradient_toon_tex, shifted_coord, 0); \n"
        "   vec4 line = texelFetch(line_tex, shifted_coord, 0); \n"

        "   vec4 shaded = gradient; \n"
        "   if(gradient_toon.a>0.0) shaded = gradient_toon; \n"
        "   if(line.a>0.0) shaded = line; \n"

        //paper granulation
        "   vec4 surface = texelFetch(surface_tex, shifted_coord, 0); \n"
        "   if(n<TEXTURE_LIMIT) surface = vec4(1.0, 1.0, 1.0, 1.0);"
        "   float Piv = 0.5*(1.0-surface.r); \n"
        "   float density_amt = 0.5; \n"
        "   vec4 powed = pow_col(shaded, 1.0+density_amt*Piv); \n"
        "   vec4 granulated = shaded*(shaded-density_amt*Piv)+(1.0-shaded)*powed; \n"
        "   combine_out = granulated; \n"
		"}\n"
	);
	glUseProgram(program); //bind program -- glUniform* calls refer to this program now

    glUniform1i(glGetUniformLocation(program, "id_tex"), 0);
    glUniform1i(glGetUniformLocation(program, "gradient_tex"), 1);
    glUniform1i(glGetUniformLocation(program, "gradient_toon_tex"), 2);
    glUniform1i(glGetUniformLocation(program, "line_tex"), 3);
    glUniform1i(glGetUniformLocation(program, "surface_tex"), 4);
    //TODO shadow lut

	glUseProgram(0); //unbind program -- glUniform* calls refer to ??? now
    GL_ERRORS();
}

CombineProgram::~CombineProgram() {
	glDeleteProgram(program);
	program = 0;
}

