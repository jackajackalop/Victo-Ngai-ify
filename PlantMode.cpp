#include "PlantMode.hpp"

#include "BoneLitColorTextureProgram.hpp"
#include "CopyToScreenProgram.hpp"
#include "SceneProgram.hpp"
#include "GradientProgram.hpp"
#include "ShadingProgram.hpp"
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
    FLATS = 1,
    GRADIENTS_BLUR = 2,
    GRADIENTS_LINFIT = 3,
    SHADOWS = 4,
    SHADED = 5
};

int show = 1;
int lut_size = 64;

GLuint meshes_for_scene_program = 0;
Scene::Transform *camera_parent_transform = nullptr;
Scene::Camera *camera = nullptr;
glm::vec2 camera_spin = glm::vec2(0.0f, 0.0f);
glm::vec3 camera_shift = glm::vec3(0.0f, 0.0f, 0.0f);


static Load< MeshBuffer > meshes(LoadTagDefault, []() -> MeshBuffer const * {
        MeshBuffer *ret = new MeshBuffer(data_path("rat_girl.pnct"));
        meshes_for_scene_program = ret->make_vao_for_program(scene_program->program);
        return ret;
        });

GLuint load_LUT(std::string const &filename);
static Load< GLuint > lut_tex(LoadTagDefault, []() -> GLuint const *{
        return new GLuint(load_LUT(data_path("lut-fixed.cube")));
        });

static Load< GLuint > shadow_lut_tex(LoadTagDefault, []() -> GLuint const *{
        return new GLuint(load_LUT(data_path("shadow_lut.cube")));
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

        return ret;
});

GLuint load_texture(int width, int height, std::vector<GLfloat> data) {
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


GLuint gradient_png_tex = 0;

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
            camera_shift.x -= 100.0*evt.motion.xrel / float(window_size.x);
            camera_shift.z += 100.0*evt.motion.yrel / float(window_size.y);
            return true;
        }
    }

    if(evt.type == SDL_MOUSEWHEEL) {
        if(evt.wheel.y > 0){
            camera_shift.y += 4000.0*evt.wheel.y / float(window_size.x);
        }else if(evt.wheel.y < 0) {
            camera_shift.y += 4000.0*evt.wheel.y / float(window_size.x);
        }
    }

    if(evt.type == SDL_KEYDOWN){
        if(evt.key.keysym.scancode == SDL_SCANCODE_0){
            show = BASIC;
        }else if(evt.key.keysym.scancode == SDL_SCANCODE_1){
            show = FLATS;
        }else if(evt.key.keysym.scancode == SDL_SCANCODE_2){
            show = GRADIENTS_BLUR;
        }else if(evt.key.keysym.scancode == SDL_SCANCODE_3){
            show = GRADIENTS_LINFIT;
        }else if(evt.key.keysym.scancode == SDL_SCANCODE_4){
            show = SHADED;
        }
    }

    return false;
}

void PlantMode::update(float elapsed) {
    camera_parent_transform->rotation = glm::angleAxis(camera_spin.x, glm::vec3(0.0f, 0.0f, 1.0f))
        *glm::angleAxis(camera_spin.y, glm::vec3(1.0, 0.0, 0.0));
    camera_parent_transform->position = camera_shift;

}

//This code allocates and resizes them as needed:
struct Textures {
    glm::uvec2 size = glm::uvec2(0,0); //remember the size of the framebuffer

    GLuint basic_tex = 0;
    GLuint color_tex = 0;
    GLuint id_tex = 0;
    GLuint toon_tex = 0;
    GLuint depth_tex = 0;
    GLuint gradient_tex = 0;
    GLuint shaded_tex = 0;

	GLuint shadow_depth_tex = 0;

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
            alloc_tex(&id_tex, GL_RGBA8, GL_RGBA);
            alloc_tex(&toon_tex, GL_RGBA8, GL_RGBA);
            alloc_tex(&depth_tex, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT);
            alloc_tex(&gradient_tex, GL_RGBA8, GL_RGBA);
            alloc_tex(&shadow_depth_tex, GL_DEPTH_COMPONENT24,
                    GL_DEPTH_COMPONENT);
            alloc_tex(&shaded_tex, GL_RGBA8, GL_RGBA);
            alloc_tex(&final_tex, GL_RGBA8, GL_RGBA);
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
    glCullFace(GL_FRONT);

    scene->draw(*camera, true);
    glBindVertexArray(empty_vao);
    GL_ERRORS();

}

void PlantMode::draw_scene(GLuint shadow_depth_tex, GLuint *basic_tex_,
        GLuint *color_tex_, GLuint *id_tex_, GLuint *toon_tex_,
        GLuint *depth_tex_)
{
    assert(basic_tex_);
    assert(color_tex_);
    assert(id_tex_);
    assert(toon_tex_);
    assert(depth_tex_);
    auto &basic_tex = *basic_tex_;
    auto &color_tex = *color_tex_;
    auto &id_tex = *id_tex_;
    auto &toon_tex = *toon_tex_;
    auto &depth_tex = *depth_tex_;

    static GLuint fb = 0;
    if(fb==0) glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
            basic_tex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D,
            color_tex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D,
            id_tex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D,
            toon_tex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
            depth_tex, 0);
    GLenum bufs[4] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
        GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3};
    glDrawBuffers(4, bufs);
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

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, shadow_depth_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LESS);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_3D, *lut_tex);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_3D, *shadow_lut_tex);
    glUseProgram(scene_program->program);
    glm::vec3 sun_dir = light_rotation*glm::vec3(0.0, 0.0, 1.0);
    glUniform3fv(scene_program->sun_direction, 1, glm::value_ptr(sun_dir));
    glUniform1i(scene_program->lut_size, lut_size);


	glm::mat4 world_to_spot =
		//This matrix converts from the spotlight's clip space ([-1,1]^3) into depth map texture coordinates ([0,1]^2) and depth map Z values ([0,1]):
		glm::mat4(
			0.5f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.5f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.5f, 0.0f,
			0.5f, 0.5f, 0.5f+0.00001f /* <-- bias */, 1.0f
		)
		//this is the world-to-clip matrix used when rendering the shadow map:
		* light_mat * make_light_to_local(light_rotation);
    glUniformMatrix4fv(scene_program->LIGHT_TO_SPOT, 1, GL_FALSE,
            glm::value_ptr(world_to_spot));
    scene->draw(*camera);
    glBindVertexArray(empty_vao);
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
                std::cout<<gradient_val<<std::endl;
                std::cout<<glm::to_string((A*soln-RHS)/RHS)<<"\n"<<std::endl;
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
    gradient_png_tex = load_texture(width, height, gradient);
}

void PlantMode::draw_gradients_linfit(GLuint basic_tex, GLuint color_tex,
        GLuint toon_tex, GLuint id_tex, GLuint *gradient_tex_)
{
    assert(gradient_tex_);
    auto &gradient_tex = *gradient_tex_;

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

    int chunk_num = scene->drawables.size();
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
    GLuint n_ssbo;

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
    glBindTexture(GL_TEXTURE_2D, color_tex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, id_tex);

    glDispatchCompute(textures.size.x, textures.size.y, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    //draw the gradient using the calculated values
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, wsum_ssbo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, hsum_ssbo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, w2sum_ssbo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, h2sum_ssbo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, whsum_ssbo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, wvsum_ssbo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, hvsum_ssbo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, vsum_ssbo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, n_ssbo);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, color_tex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, id_tex);

    glUseProgram(gradient_program->program);
    glUniform1i(gradient_program->width, textures.size.x);
    glUniform1i(gradient_program->height, textures.size.y);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
    GL_ERRORS();
}

void PlantMode::draw_shading(GLuint gradient_tex, GLuint toon_tex,
        GLuint *shaded_tex_){
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
    camera->aspect = textures.size.x / float(textures.size.y);

    GLfloat black[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    glClearBufferfv(GL_COLOR, 0, black);

    //set up basic OpenGL state:
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gradient_tex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, toon_tex);

    glUseProgram(shading_program->program);
    glDrawArrays(GL_TRIANGLES, 0, 3);

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

    draw_shadows(&textures.shadow_depth_tex);
    draw_scene(textures.shadow_depth_tex,
            &textures.basic_tex, &textures.color_tex, &textures.id_tex,
            &textures.toon_tex, &textures.depth_tex);
    if(show == GRADIENTS_BLUR){
        draw_gradients_cpu(textures.basic_tex, textures.color_tex,
                textures.id_tex, &textures.gradient_tex);
    }else if(show >= GRADIENTS_LINFIT){
        draw_gradients_linfit(textures.basic_tex, textures.color_tex,
                textures.toon_tex, textures.id_tex, &textures.gradient_tex);
    }
    if(show >= SHADED) {
        draw_shading(textures.gradient_tex, textures.toon_tex,
                &textures.shaded_tex);
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
    }else if(show == SHADED){
        glBindTexture(GL_TEXTURE_2D, textures.shaded_tex);
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
