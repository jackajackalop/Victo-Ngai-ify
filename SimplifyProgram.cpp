#include "SimplifyProgram.hpp"

#include "gl_compile_program.hpp"
#include "gl_errors.hpp"

Load< CalculateGradientProgram > calculate_gradient_program
                    (LoadTagEarly, []() -> CalculateGradientProgram const * {
    return new CalculateGradientProgram();
});

Load< SimplifyProgram > simplify_program(LoadTagEarly, []() -> SimplifyProgram const * {
    return new SimplifyProgram();
});

CalculateGradientProgram::CalculateGradientProgram() {
	//Compile vertex and fragment shaders using the convenient 'gl_compile_program' helper function:
	program = gl_compile_program({ {GL_COMPUTE_SHADER,
		"#version 430\n"
        "uniform sampler2D basic_tex; \n"
        "uniform sampler2D color_tex; \n"
        "uniform sampler2D shadow_tex; \n"
        "uniform sampler2D toon_tex; \n"
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

        "void add_vals(int id, vec4 color){ \n"
        "   if(color.a>0.0){ \n"
        "       float value = max(color.r, max(color.g, color.b));"
        "       float w = float(gl_GlobalInvocationID.x)/width; \n"
        "       float h = float(gl_GlobalInvocationID.y)/height; \n"
        "       atomicAdd(wsum[id], pack_float(w)); \n"
        "       atomicAdd(hsum[id], pack_float(h)); \n"
        "       atomicAdd(w2sum[id], pack_float(w*w)); \n"
        "       atomicAdd(h2sum[id], pack_float(h*h)); \n"
        "       atomicAdd(whsum[id], pack_float(w*h)); \n"
        "       atomicAdd(wvsum[3*id], pack_float(w*color.r)); \n"
        "       atomicAdd(wvsum[3*id+1], pack_float(w*color.g)); \n"
        "       atomicAdd(wvsum[3*id+2], pack_float(w*color.b)); \n"
        "       atomicAdd(hvsum[3*id], pack_float(h*color.r)); \n"
        "       atomicAdd(hvsum[3*id+1], pack_float(h*color.g)); \n"
        "       atomicAdd(hvsum[3*id+2], pack_float(h*color.b)); \n"
        "       atomicAdd(vsum[3*id], pack_float(color.r)); \n"
        "       atomicAdd(vsum[3*id+1], pack_float(color.g)); \n"
        "       atomicAdd(vsum[3*id+2], pack_float(color.b)); \n"
        "       atomicAdd(n[id], 1); \n"
        "   } \n"

        "} \n"

		"void main() {\n"
        "   ivec2 coord = ivec2(gl_GlobalInvocationID.xy); \n"
        "   vec4 color = texelFetch(color_tex, coord, 0); \n"
        "   int id = int(texelFetch(id_tex, coord, 0).r*255.0); \n"
        "   add_vals(3*id, color); \n"
        "   color = texelFetch(shadow_tex, coord, 0); \n"
        "   add_vals(3*id+1, color); \n"
        "   color = texelFetch(toon_tex, coord, 0); \n"
        "   add_vals(3*id+2, color); \n"
		"}\n"
        }}
	);
	glUseProgram(program); //bind program -- glUniform* calls refer to this program now

    width = glGetUniformLocation(program, "width");
    height = glGetUniformLocation(program, "height");

    glUniform1i(glGetUniformLocation(program, "basic_tex"), 0);
    glUniform1i(glGetUniformLocation(program, "color_tex"), 1);
    glUniform1i(glGetUniformLocation(program, "shadow_tex"), 2);
    glUniform1i(glGetUniformLocation(program, "toon_tex"), 3);
    glUniform1i(glGetUniformLocation(program, "id_tex"), 4);

	glUseProgram(0); //unbind program -- glUniform* calls refer to ??? now
    GL_ERRORS();
}

SimplifyProgram::SimplifyProgram() {
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
        "uniform sampler2D shadow_tex; \n"
        "uniform sampler2D toon_tex; \n"
        "uniform sampler2D id_tex; \n"
        "uniform sampler2D normal_tex; \n"
        "uniform sampler2D depth_tex; \n"
        "uniform sampler3D line_lut_tex; \n"
        "uniform sampler2D print0_tex; \n"
        "uniform sampler2D print1_tex; \n"
        "uniform sampler2D print2_tex; \n"
        "uniform sampler2D print3_tex; \n"
        "uniform sampler2D print4_tex; \n"
        "uniform sampler2D print5_tex; \n"
        "uniform int lut_size; \n"
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
		"layout(location=1) out vec4 gradient_shadow_out;\n"
		"layout(location=2) out vec4 gradient_toon_out;\n"
		"layout(location=3) out vec4 line_out;\n"

        "int lod = 0; \n"
        "vec3 line_color = vec3(0, 0, 0); \n"

        "float unpack_float(uint v){ \n"
        "   return float(v)/4096.0; \n"
        "} \n"

        //type is 0 for base color, 1 for shadows, 2 for toon shading
        "vec2 calculate_eq(int channel, int type){ \n"
        "   ivec2 coord = ivec2(gl_FragCoord.xy); \n"
        "   int id = 3*int(texelFetch(id_tex, coord, 0).r*255)+type; \n"
        "   float wsum = unpack_float(wsums_packed[id]); \n"
        "   float hsum = unpack_float(hsums_packed[id]); \n"
        "   float w2sum = unpack_float(w2sums_packed[id]); \n"
        "   float h2sum = unpack_float(h2sums_packed[id]); \n"
        "   float whsum = unpack_float(whsums_packed[id]); \n"
        "   float wvsum = unpack_float(wvsums_packed[3*id+channel]); \n"
        "   float hvsum = unpack_float(hvsums_packed[3*id+channel]); \n"
        "   float vsum = unpack_float(vsums_packed[3*id+channel]); \n"
        "   float n = ns[id]; \n"

        "   vec2 RHS = vec2(hvsum, vsum); \n"
        "   mat2 A = mat2(h2sum, hsum,"
        "                 hsum, n); \n"
        "   return inverse(A)*RHS; \n"
        "} \n"

        "bool boundaryCheck(ivec2 coord) { \n"
        "   ivec2 c = ivec2(coord/(pow(2.0, lod))); \n"
        "   int id = 3*int(texelFetch(id_tex, c, lod).r*255); \n"
        "   int idl = 3*int(texelFetch(id_tex, c+ivec2(-1, 0), lod).r*255); \n"
        "   int idr = 3*int(texelFetch(id_tex, c+ivec2(1, 0), lod).r*255); \n"
        "   int idu = 3*int(texelFetch(id_tex, c+ivec2(0, 1), lod).r*255); \n"
        "   int idd = 3*int(texelFetch(id_tex, c+ivec2(0, -1), lod).r*255); \n"
        "   return !(id==idl && id==idr && id==idu && id==idd);"
        "} \n"

        //referenced this https://roystan.net/articles/outline-shader.html
        "bool depthCheck(ivec2 coord){ \n"
        "   ivec2 c = ivec2(coord/(pow(2.0, lod))); \n"
        "   float threshold = 0.005; \n"
        "   float d0 = texelFetch(depth_tex, c+ivec2(1, -1), lod).r; \n"
        "   float d1 = texelFetch(depth_tex, c+ivec2(1, 1), lod).r; \n"
        "   float d2 = texelFetch(depth_tex, c+ivec2(-1, -1), lod).r; \n"
        "   float d3 = texelFetch(depth_tex, c+ivec2(-1, 1), lod).r; \n"
        "   float mind = min(d0, min(d1, min(d2, d3))); \n"
        "   if(mind == d0) line_color = texelFetch(color_tex, c+ivec2(1, -1), lod).rgb; \n"
        "   if(mind == d1) line_color = texelFetch(color_tex, c+ivec2(1, 1), lod).rgb; \n"
        "   if(mind == d2) line_color = texelFetch(color_tex, c+ivec2(-1, -1), lod).rgb; \n"
        "   if(mind == d3) line_color = texelFetch(color_tex, c+ivec2(-1, 1), lod).rgb; \n"

        "   float ddif0 = d1-d0; \n"
        "   float ddif1 = d3-d2; \n"
        "   float edged = sqrt(ddif0*ddif0 + ddif1*ddif1)*100; \n"
        "   return edged>threshold;"
        "} \n"

        "bool normalCheck(ivec2 coord) { \n"
        "   ivec2 c = ivec2(coord/(pow(2.0, lod))); \n"
        "   float threshold = 0.3; \n"
        "   vec3 n0 = texelFetch(normal_tex, c+ivec2(1, -1), lod).xyz; \n"
        "   vec3 n1 = texelFetch(normal_tex, c+ivec2(1, 1), lod).xyz; \n"
        "   vec3 n2 = texelFetch(normal_tex, c+ivec2(-1, -1), lod).xyz; \n"
        "   vec3 n3 = texelFetch(normal_tex, c+ivec2(-1, 1), lod).xyz; \n"
        "   vec3 ndif0 = n1-n0; \n"
        "   vec3 ndif1 = n3-n2; \n"
        "   float edgen = sqrt(dot(ndif0, ndif0) + dot(ndif1, ndif1)); \n"
        "   return edgen>threshold;"
        "} \n"

        //Based on Jaume Sanchez Elias's implementation of "Real-Time Hatching"
        //https://github.com/spite/cross-hatching
        //http://hhoppe.com/hatching.pdf
        "vec4 tone(vec4 og){ \n"
        "   vec2 tile = textureSize(print0_tex, 0);"
        "   vec2 uv = gl_FragCoord.xy/tile*4.0; \n"
        "   vec4 print0 = texture(print0_tex, uv, 0);\n"
        "   vec4 print1 = texture(print1_tex, uv, 0);\n"
        "   vec4 print2 = texture(print2_tex, uv, 0);\n"
        "   vec4 print3 = texture(print3_tex, uv, 0);\n"
        "   vec4 print4 = texture(print4_tex, uv, 0);\n"
        "   vec4 print5 = texture(print5_tex, uv, 0);\n"
        "   float ambientWeight = 0.08; \n"
        "   float diffuseWeight = 1.0; \n"
        "   float rimWeight = 0.46; \n"
        "   float specularWeight = 1.0; \n"
        "   float shininess = 49.0; \n"
        "   float shading = (og.r+og.g+og.b)/3.0; \n"
        "   if(shading>0) shading+=0.5; \n"
        "   vec4 c = vec4( 0.0, 0.0, 0.0, 0.0); \n"
        "   float step = 1.0/6.0; \n"
        "   if( shading <= step && shading > 0.0) \n"
        "       c = mix(print5, print4, 6.0*shading); \n"
        "   if( shading>step && shading<=2.0*step) \n"
        "       c = mix(print4, print3, 6.0*(shading-step)); \n"
        "   if( shading>2.0*step && shading<=3.0*step) \n"
        "       c = mix(print3, print2, 6.0*(shading-2.0*step)); \n"
        "   if( shading>3.0*step && shading<=4.0*step) \n"
        "       c = mix(print2, print1, 6.0*(shading-3.0*step)); \n"
        "   if( shading>4.0*step && shading<=5.0*step) \n"
        "       c = mix(print1, print0, 6.0*(shading-4.0*step)); \n"
        "   if( shading>5.0*step ) \n"
        "       c = mix( print0, vec4(1.0), 6.0*(shading-5.0*step)); \n"
        "   if(c.a>0.0) return og*0.8; \n"
        "   return og; \n"
        "} \n"

        "void main() {\n"
        "   ivec2 coord = ivec2(gl_FragCoord.xy); \n"

        //calculates the main gradients
        "   vec2 eqR = calculate_eq(0, 0); \n"
        "   vec2 eqG = calculate_eq(1, 0); \n"
        "   vec2 eqB = calculate_eq(2, 0); \n"
        "   float normalizedY = float(coord.y)/height; \n"
        "   float gR = eqR.x*normalizedY+eqR.y; \n"
        "   float gG = eqG.x*normalizedY+eqG.y; \n"
        "   float gB = eqB.x*normalizedY+eqB.y; \n"
        "   gradient_out = vec4(gR, gG, gB, 1.0); \n"
        //adds a depth based gradient
        "   float s = texelFetch(depth_tex, coord, 0).r;"
        "   s = (2.0*0.1)/(1000.0+0.1-s*(1000.0-0.1));"
        "   gradient_out.rgb *= 1.0-s*0.4; \n"

        //gradients the cast shadows
        "   vec4 shadow_color = texelFetch(shadow_tex, coord, 0); \n"
        "   if(shadow_color.a>0.0){ \n"
        "       eqR = calculate_eq(0, 1); \n"
        "       eqG = calculate_eq(1, 1); \n"
        "       eqB = calculate_eq(2, 1); \n"
        "       gR = eqR.x*normalizedY+eqR.y; \n"
        "       gG = eqG.x*normalizedY+eqG.y; \n"
        "       gB = eqB.x*normalizedY+eqB.y; \n"
        "       s = texelFetch(depth_tex, coord, 0).r;"
        "       s = (2.0*0.1)/(600.0+0.1-s*(600.0-0.1));"
        "       float adj = (1.0-s)*(1.0-s); \n"
        "       gR = gR*(gR-adj*gradient_out.r)+(1.0-adj)*gradient_out.r; \n"
        "       gG = gG*(gG-adj*gradient_out.g)+(1.0-adj)*gradient_out.g; \n"
        "       gB = gB*(gB-adj*gradient_out.b)+(1.0-adj)*gradient_out.b; \n"
        "       gradient_shadow_out = vec4(gR, gG, gB, 1.0); \n"
        "   } else { \n"
        "       gradient_shadow_out = vec4(0.0); \n"
        "   } \n"

        //gradients the toon shading
        "   vec4 toon_color = texelFetch(toon_tex, coord, 0); \n"
        "   if(toon_color.a>0.0){ \n"
        "       eqR = calculate_eq(0, 2); \n"
        "       eqG = calculate_eq(1, 2); \n"
        "       eqB = calculate_eq(2, 2); \n"
        "       gR = eqR.x*normalizedY+eqR.y; \n"
        "       gG = eqG.x*normalizedY+eqG.y; \n"
        "       gB = eqB.x*normalizedY+eqB.y; \n"
        "       gradient_toon_out = tone(vec4(gR, gG, gB, 1.0)); \n"
        "   } else { \n"
        "       gradient_toon_out = vec4(0.0); \n"
        "   } \n"

        //line drawing
        "   if(depthCheck(coord)||boundaryCheck(coord)||normalCheck(coord)){ \n"
        "       vec3 scale = vec3(lut_size - 1.0)/lut_size; \n"
        "       vec3 offset = vec3(1.0/(2.0*lut_size)); \n"
        "       line_color = texture(line_lut_tex, scale*line_color+offset).rgb; \n"
        "       line_out = vec4(line_color, 1.0);"
        "   } else { \n"
        "       line_out = vec4(0.0); \n"
        "   } \n"
		"}\n"
	);
	glUseProgram(program); //bind program -- glUniform* calls refer to this program now

    width = glGetUniformLocation(program, "width");
    height = glGetUniformLocation(program, "height");
    lut_size = glGetUniformLocation(program, "lut_size");

    glUniform1i(glGetUniformLocation(program, "color_tex"), 0);
	glUniform1i(glGetUniformLocation(program, "shadow_tex"), 1);
    glUniform1i(glGetUniformLocation(program, "toon_tex"), 2);
    glUniform1i(glGetUniformLocation(program, "id_tex"), 3);
    glUniform1i(glGetUniformLocation(program, "normal_tex"), 4);
    glUniform1i(glGetUniformLocation(program, "depth_tex"), 5);
    glUniform1i(glGetUniformLocation(program, "line_lut_tex"), 6);
    glUniform1i(glGetUniformLocation(program, "print0_tex"), 7);
    glUniform1i(glGetUniformLocation(program, "print1_tex"), 8);
    glUniform1i(glGetUniformLocation(program, "print2_tex"), 9);
    glUniform1i(glGetUniformLocation(program, "print3_tex"), 10);
    glUniform1i(glGetUniformLocation(program, "print4_tex"), 11);
    glUniform1i(glGetUniformLocation(program, "print5_tex"), 12);

	glUseProgram(0); //unbind program -- glUniform* calls refer to ??? now
    GL_ERRORS();
}

CalculateGradientProgram::~CalculateGradientProgram() {
	glDeleteProgram(program);
	program = 0;
}

SimplifyProgram::~SimplifyProgram() {
	glDeleteProgram(program);
	program = 0;
}

