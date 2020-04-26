#include "PlantMode.hpp"

#include "CopyToScreenProgram.hpp"
#include "SceneProgram.hpp"
#include "SimplifyProgram.hpp"
#include "CombineProgram.hpp"
#include "SurfaceProgram.hpp"
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
#include <glm/gtx/string_cast.hpp>

#include <iostream>
#include <fstream>
#include <map>
#include <cstddef>
#include <png.h>
#include <random>
#include <unordered_map>

enum Stages {
    BASIC = 0,
    TRANSPARENT = 1,
    SHADOWS = 2,
    MATERIALS = 3,
    SURFACE = 4,
    FLATS = 5,
    SIMPLIFY = 6,
    LINES = 7,
    SHADED = 8,
};

//art directable globals
int show = 8;

//other globals
int lut_size = 64;
bool surfaced = false;
bool shadowed = false;
GLuint n_ssbo;
GLuint meshes_for_scene_program = 0;
Scene::Transform *camera_parent_transform = nullptr;
Scene::Camera *camera = nullptr;
Scene::Light *spot = nullptr;
glm::vec2 camera_spin = glm::vec2(0.0f, 0.0f);
glm::vec3 camera_shift = glm::vec3(0.0f, 0.0f, 0.0f);
GLuint gradient_png_tex = 0;

GLuint load_texture(std::string const &filename) {
    glm::uvec2 size;
    std::vector< glm::u8vec4 > data;
    load_png(filename, &size, &data, LowerLeftOrigin);

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    GL_ERRORS();

    return tex;
}

GLuint load_texture_from_data(int width, int height, std::vector<GLfloat> data) {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_FLOAT, data.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    GL_ERRORS();

    return tex;
}

static Load< MeshBuffer > meshes(LoadTagDefault, []() -> MeshBuffer const * {
        MeshBuffer *ret = new MeshBuffer(data_path("shower.pnct"));
        meshes_for_scene_program = ret->make_vao_for_program(scene_program->program);
        return ret;
        });


static Load< GLuint > start_tex(LoadTagDefault, [](){
        return new GLuint (load_texture(data_path("textures/flowers.png")));
        });

static Load< Scene > scene(LoadTagLate, []() -> Scene const * {
        Scene *ret = new Scene();
        ret->load(data_path("shower.scene"), [](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
                auto &mesh = meshes->lookup(mesh_name);
                scene.drawables.emplace_back(transform);
                Scene::Drawable::Pipeline &pipeline = scene.drawables.back().pipeline;
                pipeline = scene_program_pipeline;
                pipeline.vao = meshes_for_scene_program;
                pipeline.type = mesh.type;
                pipeline.start = mesh.start;
                pipeline.count = mesh.count;
                });

        //look up camera parent transform:
        for (auto t = ret->transforms.begin(); t != ret->transforms.end(); ++t) {
        if (t->name == "CameraParent") {
        if (camera_parent_transform) throw std::runtime_error("Multiple 'CameraParent' transforms in scene.");
        camera_parent_transform = &(*t);
        }
        }
        if (!camera_parent_transform) throw std::runtime_error("No 'CameraParent' transform in scene.");
        camera_shift = camera_parent_transform->position;

        //look up the camera:
        for (auto c = ret->cameras.begin(); c != ret->cameras.end(); ++c) {
            if (c->transform->name == "Camera") {
                if (camera) throw std::runtime_error("Multiple 'Camera' objects in scene.");
                camera = &(*c);
            }
        }
        if (!camera) throw std::runtime_error("No 'Camera' camera in scene.");

        //look up the spot:
        for (auto c = ret->lights.begin(); c != ret->lights.end(); ++c) {
            if (c->transform->name == "Spot") {
                if (spot) throw std::runtime_error("Multiple 'Spot' objects in scene.");
                spot = &(*c);
            }
        }
        if (!spot) throw std::runtime_error("No 'Spot' in scene.");

        return ret;
});


PlantMode::PlantMode() {
}

PlantMode::~PlantMode() {
}

bool PlantMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
    //ignore any keys that are the result of automatic key repeat:
    if (evt.type == SDL_KEYDOWN && evt.key.repeat) {
        return false;
    }

    if (evt.type == SDL_MOUSEMOTION) {
        if (evt.motion.state & SDL_BUTTON(SDL_BUTTON_LEFT)) {
            camera_spin.x -= 3.0*evt.motion.xrel / float(window_size.x);
            camera_spin.y -= 3.0*evt.motion.yrel / float(window_size.y);
            return true;
        } else if (evt.motion.state & SDL_BUTTON(SDL_BUTTON_RIGHT)) {
            camera_shift.y -= 100.0*evt.motion.xrel / float(window_size.x);
            camera_shift.z += 100.0*evt.motion.yrel / float(window_size.y);
            return true;
        }
    }

    if(evt.type == SDL_MOUSEWHEEL) {
        if(evt.wheel.y > 0){
            camera_shift.x += 4000.0*evt.wheel.y / float(window_size.x);
        }else if(evt.wheel.y < 0) {
            camera_shift.x += 4000.0*evt.wheel.y / float(window_size.x);
        }
    }

    if(evt.type == SDL_KEYDOWN){
        if(evt.key.keysym.scancode == SDL_SCANCODE_0){
            show = 0;
        }else if(evt.key.keysym.scancode == SDL_SCANCODE_1){
            show = 1;
        }
    }

    return false;
}

void PlantMode::update(float elapsed) {
    camera_parent_transform->rotation = glm::angleAxis(camera_spin.x, glm::vec3(0.0f, 0.0f, 1.0f))
        *glm::angleAxis(camera_spin.y, glm::vec3(0.0, 1.0, 0.0));
    camera_parent_transform->position = camera_shift;
}

//This code allocates and resizes them as needed:
struct Textures {
    glm::uvec2 size = glm::uvec2(500,500); //remember the size of the framebuffer

    GLuint end_tex = 0;

    void allocate(glm::uvec2 const &new_size) {
        //allocate full-screen framebuffer:
        if (size != new_size) {
            //size = new_size;

            auto alloc_tex = [this](GLuint *tex, GLint internalformat, GLint format, glm::uvec2 size){
                if (*tex == 0) glGenTextures(1, tex);
                glBindTexture(GL_TEXTURE_2D, *tex);
                glTexImage2D(GL_TEXTURE_2D, 0, internalformat, size.x,
                        size.y, 0, format, GL_UNSIGNED_BYTE, NULL);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glBindTexture(GL_TEXTURE_2D, 0);
            };

            alloc_tex(&end_tex, GL_RGBA8, GL_RGBA, size);
            GL_ERRORS();
        }
    }
} textures;

void PlantMode::draw_gradient(GLuint *end_tex_)
{
    assert(end_tex_);
    auto &end_tex = *end_tex_;

    static GLuint fb = 0;
    if(fb == 0) glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
            end_tex, 0);
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

    int chunk_num = 1;
    //*3 because each chunk has an associated toon and cast shadow
    //+1 is because the background is not a drawable but still exists

    std::vector<float> wsum (chunk_num, 0.0f);
    std::vector<float> hsum (chunk_num, 0.0f);
    std::vector<float> w2sum (chunk_num, 0.0f);
    std::vector<float> h2sum (chunk_num, 0.0f);
    std::vector<float> whsum (chunk_num, 0.0f);
    std::vector<float> wvsum (chunk_num*3, 0.0f);
    std::vector<float> hvsum (chunk_num*3, 0.0f);
    std::vector<float> vsum (chunk_num*3, 0.0f);
    std::vector<int> n (chunk_num, 0);

    //lots of help from this stackoverflow question
    //https://stackoverflow.com/questions/32094598/opengl-compute-shader-ssbo
    //calculate the linfit gradient equation things
    GLuint wsum_ssbo;
    GLuint hsum_ssbo;
    GLuint w2sum_ssbo;
    GLuint h2sum_ssbo;
    GLuint whsum_ssbo;
    GLuint wvsum_ssbo;
    GLuint hvsum_ssbo;
    GLuint vsum_ssbo;

    glUseProgram(calculate_gradient_program->program);
    glUniform1i(calculate_gradient_program->width, textures.size.x);
    glUniform1i(calculate_gradient_program->height, textures.size.y);

    glGenBuffers(1, &wsum_ssbo);
    glGenBuffers(1, &hsum_ssbo);
    glGenBuffers(1, &w2sum_ssbo);
    glGenBuffers(1, &h2sum_ssbo);
    glGenBuffers(1, &whsum_ssbo);
    glGenBuffers(1, &wvsum_ssbo);
    glGenBuffers(1, &hvsum_ssbo);
    glGenBuffers(1, &vsum_ssbo);
    glGenBuffers(1, &n_ssbo);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, wsum_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, chunk_num*sizeof(float), wsum.data(), GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, hsum_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, chunk_num*sizeof(float), hsum.data(), GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, w2sum_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, chunk_num*sizeof(float), w2sum.data(), GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, h2sum_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, chunk_num*sizeof(float), h2sum.data(), GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, whsum_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, chunk_num*sizeof(float), whsum.data(), GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, wvsum_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 3*chunk_num*sizeof(float), wvsum.data(), GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, hvsum_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 3*chunk_num*sizeof(float), hvsum.data(), GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, vsum_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 3*chunk_num*sizeof(float), vsum.data(), GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, n_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, chunk_num*sizeof(int), n.data(), GL_DYNAMIC_COPY);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, *start_tex);

    glDispatchCompute(textures.size.x, textures.size.y, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    //draw the gradient using the calculated values
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, *start_tex);

    glUseProgram(simplify_program->program);
    glUniform1i(simplify_program->width, textures.size.x);
    glUniform1i(simplify_program->height, textures.size.y);
    GL_ERRORS();
    glDrawArrays(GL_TRIANGLES, 0, 3);

    GL_ERRORS();
}

void PlantMode::draw(glm::uvec2 const &drawable_size) {
    textures.allocate(drawable_size);

    draw_gradient(&textures.end_tex);

    //Copy scene from color buffer to screen, performing post-processing effects:
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glActiveTexture(GL_TEXTURE0);
    if(show == 0) glBindTexture(GL_TEXTURE_2D, *start_tex);
    else glBindTexture(GL_TEXTURE_2D, textures.end_tex);

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
