#include "PlantMode.hpp"

#include "BoneLitColorTextureProgram.hpp"
#include "CopyToScreenProgram.hpp"
#include "SceneProgram.hpp"
#include "GradientProgram.hpp"
#include "BilateralishGradientProgram.hpp"
#include "Load.hpp"
#include "Mesh.hpp"
#include "Scene.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"
#include "load_save_png.hpp"
#include "check_fb.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <fstream>
#include <map>
#include <cstddef>
#include <png.h>
#include <random>
#include <unordered_map>

enum Stages {
    BASIC = 0,
    FLATS = 1,
    GRADIENTS_BLUR = 2,
    GRADIENTS_LINFIT = 3
};

int show = 1;
int lut_size = 64;

GLuint meshes_for_scene_program = 0;

static Load< MeshBuffer > meshes(LoadTagDefault, []() -> MeshBuffer const * {
        MeshBuffer *ret = new MeshBuffer(data_path("rat_girl.pnct"));
        meshes_for_scene_program = ret->make_vao_for_program(scene_program->program);
        return ret;
        });

static Load< Scene > scene(LoadTagLate, []() -> Scene const * {
        Scene *ret = new Scene();
        ret->load(data_path("rat_girl.scene"), [](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
                auto &mesh = meshes->lookup(mesh_name);
                scene.drawables.emplace_back(transform);
                Scene::Drawable::Pipeline &pipeline = scene.drawables.back().pipeline;
                pipeline = scene_program_pipeline;
                pipeline.vao = meshes_for_scene_program;
                pipeline.type = mesh.type;
                pipeline.start = mesh.start;
                pipeline.count = mesh.count;
                });
        return ret;
        });

GLuint load_texture(std::string const &filename) {
	glm::uvec2 size;
	std::vector< glm::u8vec4 > data;
	load_png(filename, &size, &data, LowerLeftOrigin);

	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);
	GL_ERRORS();

	return tex;
}

//referenced this https://github.com/youandhubris/GPU-LUT-OpenFrameworks
GLuint load_LUT(std::string const &filename) {
    struct RGB { float r, g, b; };
    std::vector<RGB> LUT;

    std::ifstream LUTfile(filename.c_str());
    if (!LUTfile) {
		throw std::runtime_error("Failed to open LUT file '" + filename + "'.");
	}

    while(!LUTfile.eof()){
        std::string LUTline;
        getline(LUTfile, LUTline);
        if(!LUTline.empty()){
            RGB l;
            if (sscanf(LUTline.c_str(), "%f %f %f", &l.r, &l.g, &l.b) == 3){
                LUT.push_back(l);
            }
        }
    }
    if (LUT.size() != pow(lut_size, 3.0)) {
		throw std::runtime_error("LUT size is wrong");
	}

    GLuint tex = 0;
    glEnable(GL_TEXTURE_3D);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_3D, tex);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB, lut_size, lut_size, lut_size, 0,
            GL_RGB, GL_FLOAT, &LUT[0]);

    glBindTexture(GL_TEXTURE_3D, 0);
    glDisable(GL_TEXTURE_3D);

    return tex;
}


GLuint gradient_png_tex = load_texture(data_path("gradient.png"));
GLuint lut_tex = load_LUT(data_path("lut.cube"));

PlantMode::PlantMode() {
    assert(scene->cameras.size() && "Scene requires a camera.");

    camera = const_cast< Scene::Camera *> (&scene->cameras.front());
}

PlantMode::~PlantMode() {
}

bool PlantMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
    //ignore any keys that are the result of automatic key repeat:
    if (evt.type == SDL_KEYDOWN && evt.key.repeat) {
        return false;
    }
    if(evt.type == SDL_KEYDOWN){
        glm::mat3 directions = glm::mat3_cast(camera->transform->rotation);
        if(evt.key.keysym.scancode == SDL_SCANCODE_Q){
            glm::vec3 step = -5.0f * directions[2];
            camera->transform->position+=step;
        }else if(evt.key.keysym.scancode == SDL_SCANCODE_E){
            glm::vec3 step = 5.0f * directions[2];
            camera->transform->position+=step;
        }else if(evt.key.keysym.scancode == SDL_SCANCODE_W){
            glm::vec3 step = 5.0f * directions[1];
            camera->transform->position+=step;
        }else if(evt.key.keysym.scancode == SDL_SCANCODE_S){
            glm::vec3 step = -5.0f * directions[1];
            camera->transform->position+=step;
        }else if(evt.key.keysym.scancode == SDL_SCANCODE_A){
            glm::vec3 step = -5.0f * directions[0];
            camera->transform->position+=step;
        }else if(evt.key.keysym.scancode == SDL_SCANCODE_D){
            glm::vec3 step = 5.0f * directions[0];
            camera->transform->position+=step;
        }else if(evt.key.keysym.scancode == SDL_SCANCODE_0){
            show = BASIC;
        }else if(evt.key.keysym.scancode == SDL_SCANCODE_1){
            show = FLATS;
        }else if(evt.key.keysym.scancode == SDL_SCANCODE_2){
            show = GRADIENTS_BLUR;
        }else if(evt.key.keysym.scancode == SDL_SCANCODE_3){
            show = GRADIENTS_LINFIT;
        }
    }

    return false;
}

void PlantMode::update(float elapsed) {
}

//This code allocates and resizes them as needed:
struct Textures {
    glm::uvec2 size = glm::uvec2(0,0); //remember the size of the framebuffer

    GLuint basic_tex = 0;
    GLuint color_tex = 0;
    GLuint depth_tex = 0;
    GLuint gradient_tex = 0;
    GLuint gradient_temp_tex = 0;
    GLuint final_tex = 0;
    void allocate(glm::uvec2 const &new_size) {
        //allocate full-screen framebuffer:

        if (size != new_size) {
            size = new_size;

            auto alloc_tex = [this](GLuint *tex, GLint internalformat, GLint format){
                if (*tex == 0) glGenTextures(1, tex);
                glBindTexture(GL_TEXTURE_2D, *tex);
                glTexImage2D(GL_TEXTURE_2D, 0, internalformat, size.x,
                        size.y, 0, format, GL_UNSIGNED_BYTE, NULL);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glBindTexture(GL_TEXTURE_2D, 0);
            };

            alloc_tex(&basic_tex, GL_RGBA8, GL_RGBA);
            alloc_tex(&color_tex, GL_RGBA8, GL_RGBA);
            alloc_tex(&depth_tex, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT);
            alloc_tex(&gradient_tex, GL_RGBA8, GL_RGBA);
            alloc_tex(&gradient_temp_tex, GL_RGBA8, GL_RGBA);
            alloc_tex(&final_tex, GL_RGBA8, GL_RGBA);
            GL_ERRORS();
        }

    }
} textures;


void PlantMode::draw_scene(GLuint *basic_tex_, GLuint *color_tex_, GLuint *depth_tex_)
{
    assert(basic_tex_);
    assert(color_tex_);
    assert(depth_tex_);
    auto &basic_tex = *basic_tex_;
    auto &color_tex = *color_tex_;
    auto &depth_tex = *depth_tex_;

    static GLuint fb = 0;
    if(fb==0) glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
            basic_tex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D,
            color_tex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
            depth_tex, 0);
    GLenum bufs[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, bufs);
    check_fb();

    //Draw scene:
    camera->aspect = textures.size.x / float(textures.size.y);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    //set up basic OpenGL state:
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, lut_tex);
    glUseProgram(scene_program->program);
    glUniform1i(scene_program->lut_size, lut_size);
    scene->draw(*camera);
    glBindVertexArray(empty_vao);
    GL_ERRORS();
}

void PlantMode::draw_gradients_blur(GLuint basic_tex, GLuint color_tex,
        GLuint depth_tex, GLuint *gradient_temp_tex_, GLuint *gradient_tex_)
{
    assert(gradient_temp_tex_);
    assert(gradient_tex_);
    auto &gradient_temp_tex = *gradient_temp_tex_;
    auto &gradient_tex = *gradient_tex_;

    float threshold = 0.000005f;

    static GLuint fb = 0;
    if(fb == 0) glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                            gradient_temp_tex, 0);
    GLenum bufs[1] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, bufs);
    check_fb();

    //set glViewport
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glViewport(0, 0, textures.size.x, textures.size.y);
    camera-> aspect = textures.size.x/float(textures.size.y);

    GLfloat black[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    glClearBufferfv(GL_COLOR, 0, black);

    //set up basic OpenGL state:
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, basic_tex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, color_tex);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, basic_tex);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, depth_tex);

    glUseProgram(bilateralish_gradientH_program->program);
    glUniform1f(bilateralish_gradientH_program->depth_threshold, threshold);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    GL_ERRORS();

    static GLuint fb2 = 0;
    if(fb2==0) glGenFramebuffers(1, &fb2);
    glBindFramebuffer(GL_FRAMEBUFFER, fb2);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
            gradient_tex, 0);
    bufs[1] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, bufs);
    check_fb();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, basic_tex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, color_tex);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, gradient_temp_tex);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, depth_tex);

    glUseProgram(bilateralish_gradientV_program->program);
    glUniform1f(bilateralish_gradientV_program->depth_threshold, threshold);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    GL_ERRORS();


    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, 0);

    GL_ERRORS();
}

void PlantMode::cpu_gradient(GLuint basic_tex, GLuint color_tex){
    FILE *output = fopen("dist/gradient.png", "wb");
    int width = textures.size.x;
    int height = textures.size.y;

    if(!output) return;
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) return;
    png_infop info = png_create_info_struct(png);
    if(!info) return;
    if(setjmp(png_jmpbuf(png))) return;
    png_init_io(png, output);
    png_set_IHDR(png, info, width, height, 8,
            PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
            PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);

    glBindTexture(GL_TEXTURE_2D, color_tex);
    std::vector<GLfloat> pixels(width*height*4, 0.0f);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, pixels.data());
    png_bytep row = (png_bytep)malloc(sizeof(png_byte)*width*4);

    std::map<std::string, float> prev;
    for(int y = height-1; y >= 0; y--) {
        std::map<std::string, bool> incremented;
        for(int x = 0; x<width; x++){
            int index = x+y*width;
            int r = glm::clamp(int32_t(pixels[index*4]*255), 0, 255);
            int g = glm::clamp(int32_t(pixels[index*4+1]*255), 0, 255);
            int b = glm::clamp(int32_t(pixels[index*4+2]*255), 0, 255);

            if(y > 0) index -= width;
            int ri = glm::clamp(int32_t(pixels[index*4]*255), 0, 255);
            int gi = glm::clamp(int32_t(pixels[index*4+1]*255), 0, 255);
            int bi = glm::clamp(int32_t(pixels[index*4+2]*255), 0, 255);
            int difference = abs(r-ri)+abs(g-gi)+abs(b-bi);
            std::string color_key = std::to_string(r)+std::to_string(g)
                            +std::to_string(b);
            if(difference<10){
                if(!incremented[color_key]) prev[color_key] += 0.15f;
                incremented[color_key] = true;
                int delta = int(prev[color_key]);
                ri = r-delta;
                gi = g-delta;
                bi = b-delta;
                r = glm::clamp(ri, 0, 255);;
                g = glm::clamp(gi, 0, 255);;
                b = glm::clamp(bi, 0, 255);;
            }

            row[x*4] = r;
            row[x*4+1] = g;
            row[x*4+2] = b;
            row[x*4+3] = 255;
        }
        png_write_row(png, row);
    }
    png_write_end(png, NULL);

    free(row);
    fclose(output);
    gradient_png_tex = load_texture(data_path("gradient.png"));
}

void PlantMode::draw_gradients_linfit(GLuint basic_tex, GLuint color_tex,
                                GLuint *gradient_tex_)
{
    assert(gradient_tex_);
    auto &gradient_tex = *gradient_tex_;

    cpu_gradient(basic_tex, color_tex);

    static GLuint fb = 0;
    if(fb == 0) glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                            gradient_tex, 0);
    GLenum bufs[1] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, bufs);
    check_fb();

    //set glViewport
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glViewport(0, 0, textures.size.x, textures.size.y);
    camera-> aspect = textures.size.x/float(textures.size.y);

    GLfloat black[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    glClearBufferfv(GL_COLOR, 0, black);

    //set up basic OpenGL state:
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gradient_png_tex);

    glUseProgram(gradient_program->program);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    GL_ERRORS();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);

    GL_ERRORS();
}

void PlantMode::draw_texture()
{

}

void PlantMode::draw_screentones()
{

}

void PlantMode::draw_lines()
{

}

void PlantMode::draw_vignette(){

}

void PlantMode::draw(glm::uvec2 const &drawable_size) {
    textures.allocate(drawable_size);

    draw_scene(&textures.basic_tex, &textures.color_tex, &textures.depth_tex);
    if(show == GRADIENTS_BLUR){
        draw_gradients_blur(textures.basic_tex, textures.color_tex,
                textures.depth_tex,
                &textures.gradient_temp_tex, &textures.gradient_tex);
    }else{
        draw_gradients_linfit(textures.basic_tex, textures.color_tex,
                &textures.gradient_tex);
    }
    draw_texture();
    draw_screentones();
    draw_lines();
    draw_vignette();

    //Copy scene from color buffer to screen, performing post-processing effects:
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glActiveTexture(GL_TEXTURE0);

    if(show == BASIC){
        glBindTexture(GL_TEXTURE_2D, textures.basic_tex);
    }else if(show == FLATS){
        glBindTexture(GL_TEXTURE_2D, textures.color_tex);
    }else if(show == GRADIENTS_BLUR || show == GRADIENTS_LINFIT){
        glBindTexture(GL_TEXTURE_2D, textures.gradient_tex);
    }

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glBindVertexArray(empty_vao);
    glUseProgram(copy_to_screen_program->program);

    glDrawArrays(GL_TRIANGLES, 0, 3);

    glUseProgram(0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);

    GL_ERRORS();
}
