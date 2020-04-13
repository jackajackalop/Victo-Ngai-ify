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
        "uniform sampler2D id_tex; \n"
        "uniform sampler2D texColor_tex; \n"
        "uniform sampler2D control_tex; \n"
        "uniform sampler2D gradient_tex; \n"
        "uniform sampler2D gradient_shadow_tex; \n"
        "uniform sampler2D gradient_toon_tex; \n"
        "uniform sampler2D line_tex; \n"
        "uniform sampler2D surface_tex; \n"
        "uniform sampler2D vignette_tex; \n"
        "uniform sampler2D tex1; \n"
        "uniform sampler2D tex2; \n"
        "uniform sampler2D tex3; \n"
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

        "   vec4 color = texelFetch(color_tex, shifted_coord, 0); \n"
        "   float gradient_ctrl = texelFetch(control_tex, shifted_coord, 0).r;"
        "   vec4 gradient = texelFetch(gradient_tex, shifted_coord, 0); \n"
        "   if(gradient_ctrl>0) gradient = color; \n"
        "   vec4 gradient_shadow = texelFetch(gradient_shadow_tex, shifted_coord, 0); \n"
        "   vec4 gradient_toon = texelFetch(gradient_toon_tex, shifted_coord, 0); \n"
        "   vec4 line = texelFetch(line_tex, shifted_coord, 0); \n"

        "   vec4 shaded = gradient; \n"
        "   if(gradient_toon.a>0.0){ \n"
        "       shaded.r *= gradient_toon.r; \n"
        "       shaded.g *= gradient_toon.g; \n"
        "       shaded.b *= gradient_toon.b; \n"
        "   } \n"
        "   if(gradient_shadow.a>0.0){ \n"
        "       shaded.r *= gradient_shadow.r; \n"
        "       shaded.g *= gradient_shadow.g; \n"
        "       shaded.b *= gradient_shadow.b; \n"
        "   } \n"
        "   if(line.a>0.0) shaded = line; \n"

        //paper granulation
        "   vec4 surface = texelFetch(surface_tex, shifted_coord, 0); \n"
        "   if(n<TEXTURE_LIMIT) surface = vec4(1.0, 1.0, 1.0, 1.0);"
        "   float Piv = 0.5*(1.0-surface.r); \n"
        "   float density_amt = 0.5; \n"
        "   vec4 powed = pow_col(shaded, 1.0+density_amt*Piv); \n"
        "   vec4 granulated = shaded*(shaded-density_amt*Piv)+(1.0-shaded)*powed; \n"

        //detailing
        "   combine_out = granulated; \n"
        "   vec4 mat_id = texelFetch(texColor_tex, shifted_coord, 0);"
        "   vec4 t1 = texture(tex1, 2*gl_FragCoord.xy/textureSize(tex1, 0)); \n"
        "   vec4 t2 = texture(tex2, 6*gl_FragCoord.xy/textureSize(tex2, 0)); \n"
        "   vec4 t3 = texture(tex3, 10*gl_FragCoord.xy/textureSize(tex3, 0)); \n"
        "   if(mat_id.r > 0) { \n"
        "       combine_out = t1*t1.a+combine_out*(1.0-t1.a); \n"
        "   } \n"
        "   if(mat_id.g > 0) { \n"
        "       combine_out = t2*t2.a+combine_out*(1.0-t2.a); \n"
        "   } \n"
        "   if(mat_id.b > 0) { \n"
        "       combine_out = t3*t3.a+combine_out*(1.0-t3.a); \n"
        "   } \n"

        //vignette
        "   vec2 tex_coord = gl_FragCoord.xy/textureSize(surface_tex, 0); \n"
        "   vec4 vignette = texture(vignette_tex, tex_coord); \n"
        "   combine_out = vignette*vignette.a+combine_out*(1.0-vignette.a); \n"
		"}\n"
	);
	glUseProgram(program); //bind program -- glUniform* calls refer to this program now

    glUniform1i(glGetUniformLocation(program, "color_tex"), 0);
    glUniform1i(glGetUniformLocation(program, "id_tex"), 1);
    glUniform1i(glGetUniformLocation(program, "texColor_tex"), 2);
    glUniform1i(glGetUniformLocation(program, "control_tex"), 3);
    glUniform1i(glGetUniformLocation(program, "gradient_tex"), 4);
    glUniform1i(glGetUniformLocation(program, "gradient_shadow_tex"), 5);
    glUniform1i(glGetUniformLocation(program, "gradient_toon_tex"), 6);
    glUniform1i(glGetUniformLocation(program, "line_tex"), 7);
    glUniform1i(glGetUniformLocation(program, "surface_tex"), 8);
    glUniform1i(glGetUniformLocation(program, "vignette_tex"), 9);
    glUniform1i(glGetUniformLocation(program, "tex1"), 10);
    glUniform1i(glGetUniformLocation(program, "tex2"), 11);
    glUniform1i(glGetUniformLocation(program, "tex3"), 12);

	glUseProgram(0); //unbind program -- glUniform* calls refer to ??? now
    GL_ERRORS();
}

CombineProgram::~CombineProgram() {
	glDeleteProgram(program);
	program = 0;
}

