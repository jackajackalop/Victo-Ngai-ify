#include "BilateralishGradientProgram.hpp"

#include "gl_compile_program.hpp"
#include "gl_errors.hpp"

//weights thanks to http://dev.theomader.com/gaussian-kernel-calculator/
//please pass in "ivec2(0, i)" or "ivec2(i, 0)"
#define BLUR_SHADER(OFFSET) \
        "#version 330\n" \
		"uniform sampler2D color_tex;\n" \
        "uniform sampler2D gradient_tex;\n" \
        "uniform sampler2D basic_tex;\n" \
        "uniform sampler2D depth_tex;\n" \
        "uniform float depth_threshold;\n" \
        "uniform int blur_amount; \n" \
        "uniform float weights[20];\n"\
        "layout(location=0) out vec4 gradient_out;\n"\
        "#define OFFSET " OFFSET " \n" \
		"void main() {\n"\
        "   #define RADIUS " STR(blur_amount) "\n"\
        "//4D joint bilateral blur\n"\
        "//http://dev.theomader.com/gaussian-kernel-calculator/\n"\
        "   float weight41[41] = float[](0.02247f, 0.022745f, 0.02301f, 0.023263f, 0.023504f, 0.023733f, 0.023949f, 0.024152f, 0.024341f, 0.024517f, 0.024678f, 0.024825f, 0.024957f, 0.025075f, 0.025177f, 0.025264f, 0.025335f, 0.02539f, 0.02543f, 0.025454f, 0.025462f, 0.025454f, 0.02543f, 0.02539f, 0.025335f, 0.025264f, 0.025177f, 0.025075f, 0.024957f, 0.024825f, 0.024678f, 0.024517f, 0.024341f, 0.024152f, 0.023949f, 0.023733f, 0.023504f, 0.023263f, 0.02301f, 0.022745f, 0.02247f);\n"\
        "   gradient_out = vec4(0.0, 0.0, 0.0, 1.0);\n"\
        "   float zx = texelFetch(depth_tex, ivec2(gl_FragCoord.xy), 0).r;\n"\
        "   zx = 1.0/zx; //because of weird z value weirdness with 1/z things\n"\
        "   bool blurred = false; //to decide if basic_tex needs updating\n" \
		"   for(int i = -20; i<=20; i++){\n"\
        "       bool bleed;\n"\
        "       bleed = false;\n"\
        "       float zxi = texelFetch(depth_tex, ivec2(gl_FragCoord.xy)+OFFSET, 0).r;\n"\
        "       zxi = 1.0/zxi; \n"\
        "       if (abs(zx-zxi)< depth_threshold){ //source is behind\n"\
        "           gradient_out = gradient_out+texelFetch(gradient_tex, ivec2(gl_FragCoord.xy)+OFFSET, 0)*weight41[i+20];\n"\
        "           blurred = true;\n" \
        "       } else {\n"\
        "           gradient_out = gradient_out+texelFetch(gradient_tex, ivec2(gl_FragCoord.xy), 0)*weight41[i+20];\n"\
        "       } \n"\
        "   }\n"\
        "}\n" \


BilateralishGradientHProgram::BilateralishGradientHProgram() {
	program = gl_compile_program(
		"#version 330\n"
		"void main() {\n"
        "   gl_Position = vec4(4*(gl_VertexID & 1) -1, 2 * (gl_VertexID &2) -1, 0.0, 1.0);"
		"}\n"
		,
        BLUR_SHADER("ivec2(i, 0)")
	);
	glUseProgram(program);

    depth_threshold = glGetUniformLocation(program, "depth_threshold");
    blur_amount = glGetUniformLocation(program, "blur_amount");
    weights = glGetUniformLocation(program, "weights");

    glUniform1i(glGetUniformLocation(program, "basic_tex"), 0);
    glUniform1i(glGetUniformLocation(program, "color_tex"), 1);
    glUniform1i(glGetUniformLocation(program, "gradient_tex"), 2);
    glUniform1i(glGetUniformLocation(program, "depth_tex"), 3);

	glUseProgram(0);

	GL_ERRORS();
}

BilateralishGradientVProgram::BilateralishGradientVProgram() {
	program = gl_compile_program(
		"#version 330\n"
		"void main() {\n"
        "   gl_Position = vec4(4*(gl_VertexID & 1) -1, 2 * (gl_VertexID &2) -1, 0.0, 1.0);"
		"}\n"
		,
        BLUR_SHADER("ivec2(0, i)")
	);
	glUseProgram(program);

    depth_threshold = glGetUniformLocation(program, "depth_threshold");
    blur_amount = glGetUniformLocation(program, "blur_amount");
    weights = glGetUniformLocation(program, "weights");

    glUniform1i(glGetUniformLocation(program, "basic_tex"), 0);
    glUniform1i(glGetUniformLocation(program, "color_tex"), 1);
    glUniform1i(glGetUniformLocation(program, "gradient_tex"), 2);
    glUniform1i(glGetUniformLocation(program, "depth_tex"), 3);

	glUseProgram(0);

	GL_ERRORS();
}

Load< BilateralishGradientVProgram > bilateralish_gradientV_program (LoadTagEarly, [](){
	return new BilateralishGradientVProgram();
});
Load< BilateralishGradientHProgram > bilateralish_gradientH_program (LoadTagEarly, [](){
	return new BilateralishGradientHProgram();
});
