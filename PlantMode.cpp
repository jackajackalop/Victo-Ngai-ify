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

#include "http-tweak/tweak.hpp"

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
float toon_threshold = 0.4;
float depth_gradient_extent = 0.0;
float depth_gradient_brightness = 1.0;
int line_weight = 0;
float shadow_fade = 2.4;
float shadow_extent = 0.2;

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

//referenced this https://github.com/youandhubris/GPU-LUT-OpenFrameworks
GLuint load_LUT(std::string const &filename) {
    struct RGB { float r, g, b; };
    std::vector<float> LUT;

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
                LUT.emplace_back(l.r);
                LUT.emplace_back(l.g);
                LUT.emplace_back(l.b);
            }
        }
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);

    glBindTexture(GL_TEXTURE_3D, tex);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB, lut_size, lut_size, lut_size, 0,
            GL_RGB, GL_FLOAT, LUT.data());
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);

    glBindTexture(GL_TEXTURE_3D, 0);
    GL_ERRORS();

    return tex;
}

static Load< MeshBuffer > meshes(LoadTagDefault, []() -> MeshBuffer const * {
        MeshBuffer *ret = new MeshBuffer(data_path("shower.pnct"));
        meshes_for_scene_program = ret->make_vao_for_program(scene_program->program);
        return ret;
        });

GLuint load_LUT(std::string const &filename);
static Load< GLuint > lut_tex(LoadTagDefault, []() -> GLuint const *{
        return new GLuint(load_LUT(data_path("shower_lut.cube")));
        });

static Load< GLuint > toon_lut_tex(LoadTagDefault, []() -> GLuint const *{
        return new GLuint(load_LUT(data_path("toon_lut.cube")));
        });

static Load< GLuint > shadow_lut_tex(LoadTagDefault, []() -> GLuint const *{
        return new GLuint(load_LUT(data_path("shadow_lut.cube")));
        });

static Load< GLuint > line_lut_tex(LoadTagDefault, []() -> GLuint const *{
        return new GLuint(load_LUT(data_path("line_lut.cube")));
        });

static Load< GLuint > paper_tex(LoadTagDefault, [](){
        return new GLuint (load_texture(data_path("textures/paper.png")));
        });

static Load< GLuint > print0_tex(LoadTagDefault, [](){
        return new GLuint (load_texture(data_path("textures/0.png")));
        });

static Load< GLuint > print1_tex(LoadTagDefault, [](){
        return new GLuint (load_texture(data_path("textures/1.png")));
        });

static Load< GLuint > print2_tex(LoadTagDefault, [](){
        return new GLuint (load_texture(data_path("textures/2.png")));
        });

static Load< GLuint > print3_tex(LoadTagDefault, [](){
        return new GLuint (load_texture(data_path("textures/3.png")));
        });

static Load< GLuint > print4_tex(LoadTagDefault, [](){
        return new GLuint (load_texture(data_path("textures/4.png")));
        });

static Load< GLuint > print5_tex(LoadTagDefault, [](){
        return new GLuint (load_texture(data_path("textures/5.png")));
        });

static Load< GLuint > vignette_tex(LoadTagDefault, [](){
        return new GLuint (load_texture(data_path("textures/vignette.png")));
        });

static Load< GLuint > tex1(LoadTagDefault, [](){
        return new GLuint (load_texture(data_path("textures/wood.png")));
        });

static Load< GLuint > tex2(LoadTagDefault, [](){
        return new GLuint (load_texture(data_path("textures/dash.png")));
        });

static Load< GLuint > tex3(LoadTagDefault, [](){
        return new GLuint (load_texture(data_path("textures/tile.png")));
        });

static Load< GLuint > tex4(LoadTagDefault, [](){
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

        TWEAK_CONFIG(8888, data_path("../http-tweak/tweak-ui.html"));
        static TWEAK_HINT(toon_threshold, "float 0.0 1.0");
        static TWEAK_HINT(depth_gradient_extent, "float 0.0 10.0");
        static TWEAK_HINT(depth_gradient_brightness, "float 0.0 5.0");
        static TWEAK_HINT(line_weight, "int 0 5");
        static TWEAK_HINT(shadow_fade, "float 0.0 5.0");
        static TWEAK_HINT(shadow_extent, "float 0.0 1.0");

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
            show = BASIC;
        }else if(evt.key.keysym.scancode == SDL_SCANCODE_1){
            show = TRANSPARENT;
        }else if(evt.key.keysym.scancode == SDL_SCANCODE_2){
            show = SHADOWS;
        }else if(evt.key.keysym.scancode == SDL_SCANCODE_3){
            show = MATERIALS;
        }else if(evt.key.keysym.scancode == SDL_SCANCODE_4){
            show = SURFACE;
        }else if(evt.key.keysym.scancode == SDL_SCANCODE_5){
            show = FLATS;
        }else if(evt.key.keysym.scancode == SDL_SCANCODE_6){
            show = SIMPLIFY;
        }else if(evt.key.keysym.scancode == SDL_SCANCODE_7){
            show = LINES;
        }else if(evt.key.keysym.scancode == SDL_SCANCODE_8){
            show = SHADED;
        }
    }

    return false;
}

void PlantMode::update(float elapsed) {
    camera_parent_transform->rotation = glm::angleAxis(camera_spin.x, glm::vec3(0.0f, 0.0f, 1.0f))
        *glm::angleAxis(camera_spin.y, glm::vec3(0.0, 1.0, 0.0));
    camera_parent_transform->position = camera_shift;
    TWEAK_SYNC();
}

//This code allocates and resizes them as needed:
struct Textures {
    glm::uvec2 size = glm::uvec2(0,0); //remember the size of the framebuffer

    GLuint basic_tex = 0;
    GLuint color_tex = 0;
    GLuint transp_color_tex = 0;
    GLuint tex_color_tex = 0;
    GLuint control_tex = 0;
    GLuint id_tex = 0;
    GLuint normal_tex = 0;
    GLuint toon_tex = 0;
    GLuint gradient_tex = 0;
    GLuint gradient_shadow_tex = 0;
    GLuint gradient_toon_tex = 0;
    GLuint line_tex = 0;
    GLuint shaded_tex = 0;
    GLuint surface_tex = 0;
    GLuint shadow_tex = 0;

    GLuint depth_tex = 0;
    GLuint shadow_depth_tex = 0;
    glm::uvec2 shadow_size = glm::uvec2(1024, 1024);
    glm::mat4 shadow_world_to_clip = glm::mat4(1.0);

    GLuint final_tex = 0;
    void allocate(glm::uvec2 const &new_size) {
        //allocate full-screen framebuffer:
        if (size != new_size) {
            size = new_size;
            surfaced = false;
            shadowed = false;

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

            alloc_tex(&basic_tex, GL_RGBA8, GL_RGBA, size);
            alloc_tex(&color_tex, GL_RGBA8, GL_RGBA, size);
            alloc_tex(&transp_color_tex, GL_RGBA8, GL_RGBA, size);
            alloc_tex(&tex_color_tex, GL_RGBA8, GL_RGBA, size);
            alloc_tex(&control_tex, GL_RGBA8, GL_RGBA, size);
            alloc_tex(&id_tex, GL_RGBA8, GL_RGBA, size);
            alloc_tex(&normal_tex, GL_RGBA8, GL_RGBA, size);
            alloc_tex(&toon_tex, GL_RGBA8, GL_RGBA, size);
            alloc_tex(&gradient_tex, GL_RGBA8, GL_RGBA, size);
            alloc_tex(&gradient_shadow_tex, GL_RGBA8, GL_RGBA, size);
            alloc_tex(&gradient_toon_tex, GL_RGBA8, GL_RGBA, size);
            alloc_tex(&line_tex, GL_RGBA8, GL_RGBA, size);
            alloc_tex(&depth_tex, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, size);
            alloc_tex(&shadow_depth_tex, GL_DEPTH_COMPONENT24,
                    GL_DEPTH_COMPONENT, shadow_size);
            alloc_tex(&shaded_tex, GL_RGBA8, GL_RGBA, size);
            alloc_tex(&shadow_tex, GL_RGBA8, GL_RGBA, size);
            alloc_tex(&surface_tex, GL_RGBA8, GL_RGBA, size);
            alloc_tex(&final_tex, GL_RGBA8, GL_RGBA, size);
            GL_ERRORS();
        }
    }
} textures;

void PlantMode::draw_shadows(GLuint *shadow_depth_tex_)
{
    assert(shadow_depth_tex_);
    auto &shadow_depth_tex = *shadow_depth_tex_;

    static GLuint fb = 0;
    if(fb==0) glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
            shadow_depth_tex, 0);
    check_fb();

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glViewport(0, 0, textures.shadow_size.x, textures.shadow_size.y);
    camera->aspect = textures.shadow_size.x / float(textures.shadow_size.y);

    //set up basic OpenGL state:
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);

    glm::vec3 scale = spot->transform->scale;
    glm::vec3 inv_scale =glm::vec3(1.0);
    inv_scale.x = (scale.x == 0.0f ? 0.0f : 1.0f / scale.x);
    inv_scale.y = (scale.y == 0.0f ? 0.0f : 1.0f / scale.y);
    inv_scale.z = (scale.z == 0.0f ? 0.0f : 1.0f / scale.z);
    textures.shadow_world_to_clip = spot->make_projection()
        * glm::mat4( //un-scale
                glm::vec4(inv_scale.x, 0.0f, 0.0f, 0.0f),
                glm::vec4(0.0f, inv_scale.y, 0.0f, 0.0f),
                glm::vec4(0.0f, 0.0f, inv_scale.z, 0.0f),
                glm::vec4(0.0f, 0.0f, 0.0f, 1.0f))
        * glm::mat4(glm::inverse(spot->transform->rotation))
        * glm::mat4( //un-translate
                glm::vec4(1.0f, 0.0f, 0.0f, 0.0f),
                glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
                glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
                glm::vec4(-spot->transform->position, 1.0f)
                );

    scene->draw( textures.shadow_world_to_clip, glm::mat4(1.0), true, false);
    glBindVertexArray(empty_vao);

    GL_ERRORS();
}

void PlantMode::draw_scene(GLuint shadow_depth_tex, GLuint *basic_tex_,
        GLuint *color_tex_, GLuint *transp_color_tex_, GLuint *tex_color_tex_, GLuint *control_tex_,
        GLuint *depth_tex_, GLuint *id_tex_,
        GLuint *normal_tex_, GLuint *shadow_tex_, GLuint *toon_tex_)
{
    assert(basic_tex_);
    assert(color_tex_);
    assert(transp_color_tex_);
    assert(tex_color_tex_);
    assert(control_tex_);
    assert(depth_tex_);
    assert(id_tex_);
    assert(normal_tex_);
    assert(shadow_tex_);
    assert(toon_tex_);
    auto &basic_tex = *basic_tex_;
    auto &transp_color_tex = *transp_color_tex_;
    auto &color_tex = *color_tex_;
    auto &tex_color_tex = *tex_color_tex_;
    auto &control_tex = *control_tex_;
    auto &depth_tex = *depth_tex_;
    auto &id_tex = *id_tex_;
    auto &normal_tex = *normal_tex_;
    auto &shadow_tex = *shadow_tex_;
    auto &toon_tex = *toon_tex_;

    static GLuint fb = 0;
    if(fb==0) glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
            basic_tex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D,
            color_tex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D,
            tex_color_tex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D,
            control_tex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D,
            id_tex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT5, GL_TEXTURE_2D,
            normal_tex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT6, GL_TEXTURE_2D,
            shadow_tex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT7, GL_TEXTURE_2D,
            toon_tex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
            depth_tex, 0);
    GLenum bufs[8] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
        GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4,
        GL_COLOR_ATTACHMENT5, GL_COLOR_ATTACHMENT6, GL_COLOR_ATTACHMENT7};
    glDrawBuffers(8, bufs);
    check_fb();

    //Draw scene:
    glViewport(0, 0, textures.size.x, textures.size.y);
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
    glDepthMask(GL_TRUE);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, shadow_depth_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LESS);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_3D, *lut_tex);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_3D, *toon_lut_tex);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_3D, *shadow_lut_tex);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, *tex1);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, *tex2);
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, *tex3);
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, *tex4);
    glUseProgram(scene_program->program);

    glm::mat4 spot_to_world = spot->transform->make_local_to_world();
    glUniform3fv(scene_program->spot_position, 1, glm::value_ptr(glm::vec3(spot_to_world[3])));
    glUniform1i(scene_program->lut_size, lut_size);
    glUniform1f(scene_program->toon_threshold, toon_threshold);

    glm::mat4 world_to_shadow_texture =
        //This matrix converts from the spotlight's clip space ([-1,1]^3) into depth map texture coordinates ([0,1]^2) and depth map Z values ([0,1]):
        glm::mat4(
                0.5f, 0.0f, 0.0f, 0.0f,
                0.0f, 0.5f, 0.0f, 0.0f,
                0.0f, 0.0f, 0.5f, 0.0f,
                0.5f, 0.5f, 0.5f-1e-5f /* <-- bias */, 1.0f
                )
        //this is the world-to-clip matrix used when rendering the shadow map:
        * textures.shadow_world_to_clip;
    glUniformMatrix4fv(scene_program->LIGHT_TO_SPOT, 1, GL_FALSE,
            glm::value_ptr(world_to_shadow_texture));
    scene->draw(*camera);
    glBindVertexArray(empty_vao);

    GL_ERRORS();

    //transparent pass
    static GLuint transp_fb = 0;
    if(transp_fb==0) glGenFramebuffers(1, &transp_fb);
    glBindFramebuffer(GL_FRAMEBUFFER, transp_fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
            transp_color_tex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
            depth_tex, 0);
    GLenum transp_bufs[1] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, transp_bufs);
    check_fb();

    //Draw scene:
    glViewport(0, 0, textures.size.x, textures.size.y);
    camera->aspect = textures.size.x / float(textures.size.y);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    //set up basic OpenGL state:
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);

    glUseProgram(transp_program->program);
    scene->draw(*camera, false, true);
    glBindVertexArray(empty_vao);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    GL_ERRORS();
}

void PlantMode::draw_gradients_cpu(GLuint basic_tex, GLuint color_tex,
        GLuint id_tex, GLuint *gradient_tex_)
{
    assert(gradient_tex_);
    auto &gradient_tex = *gradient_tex_;

    cpu_gradient(basic_tex, color_tex, id_tex);

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

    glUseProgram(cpu_gradient_program->program);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);

    GL_ERRORS();
}

float pack_float (float v){
    return float(v*4096.0f);
}

float unpack_float(float v){
    return float(v)/4096.0f;
}

void PlantMode::cpu_gradient(GLuint basic_tex, GLuint color_tex,
        GLuint id_tex){
    int width = textures.size.x;
    int height = textures.size.y;

    std::vector<GLfloat> pixels(width*height*4, 0.0f);
    glBindTexture(GL_TEXTURE_2D, color_tex);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, pixels.data());

    std::vector<GLfloat> ids(width*height*4, 0.0f);
    glBindTexture(GL_TEXTURE_2D, id_tex);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, ids.data());

    std::vector<GLfloat> gradient(width*height*4, 0.0f);

    //first calculate linear fit gradient equations
    //y is value, x is the height in the screen
    //the following values are calculated for each object in the scene and
    //is stored at the index of the id assigned to the object
    int num_objects = scene->drawables.size();
    std::vector<double> wsum (num_objects, 0);
    std::vector<double> hsum (num_objects, 0);
    std::vector<double> w2sum (num_objects, 0);
    std::vector<double> h2sum (num_objects, 0);
    std::vector<double> whsum (num_objects, 0);
    std::vector<double> wvsum (num_objects, 0);
    std::vector<double> hvsum (num_objects, 0);
    std::vector<double> vsum (num_objects, 0);
    std::vector<double> n (num_objects, 0);

    for(int y = 0; y<height; y++) {
        for(int x = 0; x<width; x++){
            int index = x+y*width;
            float r = pixels[index*4];
            float g = pixels[index*4+1];
            float b = pixels[index*4+2];
            float value = glm::max(glm::max(r, g), b);
            int id = ids[index*4]*255;
            assert(id<int(wsum.size()));

            wsum[id] += (x);
            hsum[id] +=  (y);
            w2sum[id] += (x*x);
            h2sum[id] += (y*y);
            whsum[id] += (x*y);
            wvsum[id] += (x*value);
            hvsum[id] += (y*value);
            vsum[id] +=  (value);
            n[id]++;
        }
    }

    for(int y = 0; y<height; y++){
        for(int x = 0; x<width; x++){
            int index = x+y*width;
            float r = pixels[index*4];
            float g = pixels[index*4+1];
            float b = pixels[index*4+2];
            float value = glm::max(glm::max(r, g), b);

            int id = ids[index*4]*255;
            double w = (wsum[id]);
            double h = (hsum[id]);
            double w2 =(w2sum[id]);
            double h2 =(h2sum[id]);
            double wh =(whsum[id]);
            double wv =(wvsum[id]);
            double hv =(hvsum[id]);
            double v = (vsum[id]);

            glm::vec3 RHS = glm::vec3(wv, hv, v);
            glm::mat3 A = glm::mat3(glm::vec3(w2, wh, w),
                    glm::vec3(wh, h2, h),
                    glm::vec3(w, h, n[id]));
            glm::vec3 soln = glm::inverse(A)*RHS;
            /* glm::vec2 RHS2 = glm::vec2(wv, v);
               glm::mat2 A2 = glm::mat2(w2, w, w, n[id]);
               glm::vec2 soln2 = glm::inverse(A2)*RHS2;*/
            float gradient_val = soln.x*x+soln.y*y+soln.z;
            //gradient_val = abs(glm::determinant(A));
            if(x==200 && y==200){
                //                std::cout<<glm::to_string(A[0])<<"\n"<<glm::to_string(A[1])<<"\n"<<glm::to_string(A[2])<<" "<<gradient_val<<std::endl;
                // std::cout<<gradient_val<<std::endl;
                // std::cout<<glm::to_string((A*soln-RHS)/RHS)<<"\n"<<std::endl;
            }
            //            gradient_val = soln2.x*x+soln2.y;
            float output = gradient_val-value;
            if(output<0){
                gradient[index*4] = 0.5-output;
            }else{
                gradient[index*4] = 0.5;
            }
            gradient[index*4+1] = 0.5;
            if(output>0){
                gradient[index*4+2] = 0.5+output;
            }else{
                gradient[index*4+2] = 0.5;
            }
            gradient[index*4] = gradient_val;
            gradient[index*4+1] = gradient_val;
            gradient[index*4+2] = gradient_val;
            gradient[index*4+3] = 1.0f;
        }
    }
    gradient_png_tex = load_texture_from_data(width, height, gradient);
}

void PlantMode::draw_simplify(GLuint basic_tex, GLuint color_tex,
        GLuint transp_color_tex, GLuint shadow_tex,
        GLuint toon_tex, GLuint id_tex, GLuint normal_tex, GLuint depth_tex,
        GLuint *gradient_tex_, GLuint *gradient_shadow_tex_,
        GLuint *gradient_toon_tex_, GLuint *line_tex_)
{
    assert(gradient_tex_);
    assert(gradient_shadow_tex_);
    assert(gradient_toon_tex_);
    assert(line_tex_);
    auto &gradient_tex = *gradient_tex_;
    auto &gradient_shadow_tex = *gradient_shadow_tex_;
    auto &gradient_toon_tex = *gradient_toon_tex_;
    auto &line_tex = *line_tex_;

    static GLuint fb = 0;
    if(fb == 0) glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
            gradient_tex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D,
            gradient_shadow_tex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D,
            gradient_toon_tex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D,
            line_tex, 0);
    GLenum bufs[4] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
        GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3};
    glDrawBuffers(4, bufs);
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

    int chunk_num = 3*(scene->drawables.size()+1);
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
    glBindTexture(GL_TEXTURE_2D, basic_tex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, color_tex);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, shadow_tex);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, toon_tex);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, id_tex);

    glDispatchCompute(textures.size.x, textures.size.y, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    //draw the gradient using the calculated values
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, color_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, transp_color_tex);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, shadow_tex);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, toon_tex);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, id_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, normal_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, depth_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_3D, *line_lut_tex);
    glActiveTexture(GL_TEXTURE8);
    glBindTexture(GL_TEXTURE_2D, *print0_tex);
    glActiveTexture(GL_TEXTURE9);
    glBindTexture(GL_TEXTURE_2D, *print1_tex);
    glActiveTexture(GL_TEXTURE10);
    glBindTexture(GL_TEXTURE_2D, *print2_tex);
    glActiveTexture(GL_TEXTURE11);
    glBindTexture(GL_TEXTURE_2D, *print3_tex);
    glActiveTexture(GL_TEXTURE12);
    glBindTexture(GL_TEXTURE_2D, *print4_tex);
    glActiveTexture(GL_TEXTURE13);
    glBindTexture(GL_TEXTURE_2D, *print5_tex);

    glUseProgram(simplify_program->program);
    glUniform1i(simplify_program->width, textures.size.x);
    glUniform1i(simplify_program->height, textures.size.y);
    glUniform1i(simplify_program->lut_size, lut_size);
    glUniform1f(simplify_program->depth_gradient_extent, depth_gradient_extent);
    glUniform1f(simplify_program->depth_gradient_brightness, depth_gradient_brightness);
    glUniform1i(simplify_program->lod, line_weight);
    glUniform1f(simplify_program->shadow_fade, shadow_fade);
    glUniform1f(simplify_program->shadow_extent, shadow_extent);

    glDrawArrays(GL_TRIANGLES, 0, 3);

    GL_ERRORS();
}

void PlantMode::draw_combine(GLuint color_tex, GLuint transp_color_tex,
        GLuint id_tex, GLuint tex_color_tex,
        GLuint control_tex, GLuint gradient_tex,
        GLuint gradient_shadow_tex, GLuint gradient_toon_tex, GLuint line_tex,
        GLuint surface_tex, GLuint *shaded_tex_){
    assert(shaded_tex_);
    auto &shaded_tex = *shaded_tex_;

    static GLuint fb = 0;
    if(fb==0) glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
            shaded_tex, 0);
    GLenum bufs[1] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, bufs);
    check_fb();

    //Draw scene:
    glViewport(0, 0, textures.size.x, textures.size.y);
    camera->aspect = textures.size.x / float(textures.size.y);

    GLfloat black[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    glClearBufferfv(GL_COLOR, 0, black);

    //set up basic OpenGL state:
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, color_tex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, transp_color_tex);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, id_tex);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, tex_color_tex);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, control_tex);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, gradient_tex);
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, gradient_shadow_tex);
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, gradient_toon_tex);
    glActiveTexture(GL_TEXTURE8);
    glBindTexture(GL_TEXTURE_2D, line_tex);
    glActiveTexture(GL_TEXTURE9);
    glBindTexture(GL_TEXTURE_2D, surface_tex);
    glActiveTexture(GL_TEXTURE10);
    glBindTexture(GL_TEXTURE_2D, *vignette_tex);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, n_ssbo);

    glUseProgram(combine_program->program);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);

    GL_ERRORS();
}

void PlantMode::draw_shadow_debug(GLuint shadow_depth_tex, GLuint *shadow_tex_)
{
    assert(shadow_tex_);
    auto &shadow_tex = *shadow_tex_;

    static GLuint fb = 0;
    if(fb==0) glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
            shadow_tex, 0);
    GLenum bufs[1] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, bufs);
    check_fb();

    //Draw scene:
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glViewport(0, 0, textures.size.x, textures.size.y);
    camera->aspect = textures.size.x / float(textures.size.y);

    GLfloat black[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    glClearBufferfv(GL_COLOR, 0, black);

    //set up basic OpenGL state:
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, shadow_depth_tex);

    glUseProgram(shadow_debug_program->program);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);

    GL_ERRORS();
}

void PlantMode::draw_surface(GLuint paper_tex, GLuint *surface_tex_)
{
    assert(surface_tex_);
    auto &surface_tex = *surface_tex_;

    static GLuint fb = 0;
    if(fb==0) glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
            surface_tex, 0);
    GLenum bufs[1] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, bufs);
    check_fb();

    //Draw scene:
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glViewport(0, 0, textures.size.x, textures.size.y);
    camera->aspect = textures.size.x / float(textures.size.y);

    GLfloat black[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    glClearBufferfv(GL_COLOR, 0, black);

    //set up basic OpenGL state:
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, paper_tex);

    glUseProgram(surface_program->program);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);

    GL_ERRORS();
}

void PlantMode::draw(glm::uvec2 const &drawable_size) {
    textures.allocate(drawable_size);

    if(!shadowed) {
        shadowed = true;
        draw_shadows(&textures.shadow_depth_tex);
    }
    draw_scene(textures.shadow_depth_tex,
            &textures.basic_tex, &textures.color_tex,
            &textures.transp_color_tex, &textures.tex_color_tex,
            &textures.control_tex, &textures.depth_tex,
            &textures.id_tex, &textures.normal_tex, &textures.shadow_tex,
            &textures.toon_tex);
    if(!surfaced) {
        surfaced = true;
        draw_surface(*paper_tex, &textures.surface_tex);
    }
    if(show == SHADOWS) {
        draw_shadow_debug(textures.shadow_depth_tex, &textures.shadow_tex);
    }
    if(show >= SIMPLIFY){
        draw_simplify(textures.basic_tex, textures.color_tex,
                textures.transp_color_tex, textures.shadow_tex,
                textures.toon_tex, textures.id_tex, textures.normal_tex,
                textures.depth_tex, &textures.gradient_tex,
                &textures.gradient_shadow_tex, &textures.gradient_toon_tex,
                &textures.line_tex);
    }
    if(show >= SHADED) {
        draw_combine(textures.color_tex, textures.transp_color_tex,
                textures.id_tex, textures.tex_color_tex,
                textures.control_tex,
                textures.gradient_tex, textures.gradient_shadow_tex,
                textures.gradient_toon_tex, textures.line_tex,
                textures.surface_tex, &textures.shaded_tex);
    }

    //Copy scene from color buffer to screen, performing post-processing effects:
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glActiveTexture(GL_TEXTURE0);

    if(show == BASIC){
        glBindTexture(GL_TEXTURE_2D, textures.id_tex);
    }else if(show == TRANSPARENT) {
        glBindTexture(GL_TEXTURE_2D, textures.transp_color_tex);
    }else if(show == MATERIALS){
        glBindTexture(GL_TEXTURE_2D, textures.tex_color_tex);
    }else if(show == FLATS){
        glBindTexture(GL_TEXTURE_2D, textures.color_tex);
    }else if(show == SIMPLIFY){
        glBindTexture(GL_TEXTURE_2D, textures.gradient_tex);
    }else if(show == SHADED){
        glBindTexture(GL_TEXTURE_2D, textures.shaded_tex);
    }else if(show == SURFACE){
        glBindTexture(GL_TEXTURE_2D, textures.surface_tex);
    }else if(show == SHADOWS){
        glBindTexture(GL_TEXTURE_2D, textures.shadow_tex);
    }else if(show == LINES){
        glBindTexture(GL_TEXTURE_2D, textures.line_tex);
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
