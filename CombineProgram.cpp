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
        "uniform sampler2D color_tex; \n"
        "uniform sampler2D transp_color_tex; \n"
        "uniform sampler2D id_tex; \n"
        "uniform sampler2D texColor_tex; \n"
        "uniform sampler2D control_tex; \n"
        "uniform sampler2D gradient_tex; \n"
        "uniform sampler2D gradient_toon_tex; \n"
        "uniform sampler2D line_tex; \n"
        "uniform sampler2D surface_tex; \n"
        "uniform sampler2D vignette_tex; \n"
        "uniform sampler2D tex1; \n"
        "uniform sampler2D tex2; \n"
        "uniform sampler2D tex3; \n"
        "uniform sampler2D tex4; \n"
        "layout (std430, binding=0) buffer nbuffer{ uint n_sum[]; }; \n"
		"layout(location=0) out vec4 combine_out;\n"

        "vec4 pow_col(vec4 base, float exp){ \n"
        "   return vec4(pow(base.r, exp), pow(base.g, exp), pow(base.b, exp), base.a); \n"
        "} \n"

		"void main() {\n"
        "   ivec2 coord = ivec2(gl_FragCoord.xy); \n"
        "   int id = int(texelFetch(id_tex, coord, 0)*255.0); \n"
        "   uint n = n_sum[id]; \n"

        //paper distortion
        "   int TEXTURE_LIMIT = 500; \n"
        "   vec4 surfaceColor = texelFetch(surface_tex, coord, 0); \n"
        "   if(n<TEXTURE_LIMIT) surfaceColor = vec4(0.0, 0.0, 0.0, 0.0); \n"
        "   vec2 shift_amt = surfaceColor.gb; \n"
        "   ivec2 shifted_coord = ivec2(coord+shift_amt); \n"

        "   vec4 color = texelFetch(color_tex, shifted_coord, 0); \n"
        "   vec4 ctrl = texelFetch(control_tex, shifted_coord, 0);"
        "   vec4 gradient = texelFetch(gradient_tex, shifted_coord, 0); \n"
        "   if(ctrl.r>0) gradient = color; \n"
        "   vec4 gradient_toon = texelFetch(gradient_toon_tex, shifted_coord, 0); \n"
        "   vec4 line = texelFetch(line_tex, shifted_coord, 0); \n"

        "   vec4 shaded = gradient; \n"
        "   if(ctrl.b==0 && gradient_toon.a>0.0){ \n"
        "       shaded.rgb *= gradient_toon.rgb;"
        "   } \n"
        "   if(line.a>0.0) shaded = line; \n"

        //paper granulation
        "   vec4 surface = texelFetch(surface_tex, shifted_coord, 0); \n"
        "   if(ctrl.r>0) surface = vec4(1.0, 1.0, 1.0, 1.0);"
        "   float Piv = 0.5*(0.8-surface.r); \n"
        "   float density_amt = 0.5; \n"
        "   vec4 powed = pow_col(shaded, 1.0+density_amt*Piv); \n"
        "   vec4 granulated = shaded.a*(shaded-density_amt*Piv)+(1.0-shaded.a)*powed; \n"

        //detailing
        "   vec4 texColor = texelFetch(texColor_tex, ivec2(gl_FragCoord.xy), 0);"
        "   combine_out = texColor*texColor.a+granulated*(1.0-texColor.a); \n"

        //transparent elements
        "   vec4 transp = texelFetch(transp_color_tex, ivec2(gl_FragCoord.xy), 0);"
        "   combine_out = transp*transp.a+combine_out*(1.0-transp.a); \n"

        //vignette
        "   vec2 tex_coord = gl_FragCoord.xy/textureSize(surface_tex, 0); \n"
        "   vec4 vignette = texture(vignette_tex, tex_coord); \n"
        "   if(combine_out.a>0) \n"
        "       combine_out = vignette*vignette.a+combine_out*(1.0-vignette.a); \n"
        "}\n"
	);
	glUseProgram(program); //bind program -- glUniform* calls refer to this program now

    glUniform1i(glGetUniformLocation(program, "color_tex"), 0);
    glUniform1i(glGetUniformLocation(program, "transp_color_tex"), 1);
    glUniform1i(glGetUniformLocation(program, "id_tex"), 2);
    glUniform1i(glGetUniformLocation(program, "texColor_tex"), 3);
    glUniform1i(glGetUniformLocation(program, "control_tex"), 4);
    glUniform1i(glGetUniformLocation(program, "gradient_tex"), 5);
    glUniform1i(glGetUniformLocation(program, "gradient_toon_tex"), 6);
    glUniform1i(glGetUniformLocation(program, "line_tex"), 7);
    glUniform1i(glGetUniformLocation(program, "surface_tex"), 8);
    glUniform1i(glGetUniformLocation(program, "vignette_tex"), 9);

	glUseProgram(0); //unbind program -- glUniform* calls refer to ??? now
    GL_ERRORS();
}

CombineProgram::~CombineProgram() {
	glDeleteProgram(program);
	program = 0;
}

