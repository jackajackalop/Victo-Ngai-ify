#include "SurfaceProgram.hpp"

#include "gl_compile_program.hpp"
#include "gl_errors.hpp"

Load< ShadowDebugProgram > shadow_debug_program(LoadTagEarly, []() -> ShadowDebugProgram const * {
    return new ShadowDebugProgram();
});

ShadowDebugProgram::ShadowDebugProgram() {
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
        "uniform sampler2D shadow_depth_tex; \n"
		"layout(location=0) out vec4 shadow_debug_out;\n"

		"void main() {\n"
        //used this http://glampert.com/2014/01-26/visualizing-the-depth-buffer/
        "   float s = texelFetch(shadow_depth_tex, ivec2(gl_FragCoord.xy), 0).r;"
        "   s = (2.0*0.1)/(600.0+0.1-s*(600.0-0.1));"
        "   shadow_debug_out = vec4(s, s, s, 1.0);"
		"}\n"
	);
	glUseProgram(program); //bind program -- glUniform* calls refer to this program now

    glUniform1i(glGetUniformLocation(program, "shadow_depth_tex"), 0);

	glUseProgram(0); //unbind program -- glUniform* calls refer to ??? now
    GL_ERRORS();
}

ShadowDebugProgram::~ShadowDebugProgram() {
	glDeleteProgram(program);
	program = 0;
}

Load< SurfaceProgram > surface_program(LoadTagEarly, []() -> SurfaceProgram const * {
    return new SurfaceProgram();
});

SurfaceProgram::SurfaceProgram() {
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
        "uniform sampler2D paper_tex; \n"
		"layout(location=0) out vec4 surface_out;\n"

		"void main() {\n"
        "   vec2 texCoord = gl_FragCoord.xy/textureSize(paper_tex, 0); \n"
        "   vec4 paperColor = texture(paper_tex, texCoord); \n"
        "   float paperHeight = paperColor.r; \n"
        "   vec3 xdirection = normalize(vec3(1.0, 0.0, dFdx(paperHeight))); \n"
        "   vec3 ydirection = normalize(vec3(0.0, 1.0, dFdy(paperHeight))); \n"
        "   vec3 n = normalize(cross(xdirection, ydirection)); \n"
        "   vec3 l = normalize(vec3(1.0, 1.0, 1.0)); \n"
        "   float nl = (dot(n, l) + 1.0)/2.0; \n"
        "   nl = mix(-0.3, 1.3, nl); \n"
        "   surface_out = vec4(paperHeight, 0.5*n.xy+0.5, nl);"
		"}\n"
	);
	glUseProgram(program); //bind program -- glUniform* calls refer to this program now

    glUniform1i(glGetUniformLocation(program, "paper_tex"), 0);

	glUseProgram(0); //unbind program -- glUniform* calls refer to ??? now
    GL_ERRORS();
}

SurfaceProgram::~SurfaceProgram() {
	glDeleteProgram(program);
	program = 0;
}

