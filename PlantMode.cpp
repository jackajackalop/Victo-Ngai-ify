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

    GL_ERRORS();
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
            alloc_tex(&id_tex, GL_RGBA8, GL_RGBA);
            alloc_tex(&depth_tex, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT);
            alloc_tex(&gradient_tex, GL_RGBA8, GL_RGBA);
            alloc_tex(&gradient_temp_tex, GL_RGBA8, GL_RGBA);
            alloc_tex(&final_tex, GL_RGBA8, GL_RGBA);
            GL_ERRORS();
        }

    }
} textures;


void PlantMode::draw_scene(GLuint *basic_tex_, GLuint *color_tex_,
        GLuint *id_tex_, GLuint *depth_tex_)
{
    assert(basic_tex_);
    assert(color_tex_);
    assert(id_tex_);
    assert(depth_tex_);
    auto &basic_tex = *basic_tex_;
    auto &color_tex = *color_tex_;
    auto &id_tex = *id_tex_;
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
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
            depth_tex, 0);
    GLenum bufs[3] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
                    GL_COLOR_ATTACHMENT2};
    glDrawBuffers(3, bufs);
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
    glBindTexture(GL_TEXTURE_3D, *lut_tex);
    glUseProgram(scene_program->program);
    glUniform1i(scene_program->lut_size, lut_size);
    scene->draw(*camera);
    glBindVertexArray(empty_vao);
    GL_ERRORS();
}

void PlantMode::draw_gradients_blur(GLuint basic_tex, GLuint color_tex,
        GLuint id_tex,
        GLuint *gradient_temp_tex_, GLuint *gradient_tex_)
{
    assert(gradient_temp_tex_);
    assert(gradient_tex_);
    auto &gradient_temp_tex = *gradient_temp_tex_;
    auto &gradient_tex = *gradient_tex_;

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
    glBindTexture(GL_TEXTURE_2D, id_tex);

    glUseProgram(bilateralish_gradientH_program->program);
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
    glBindTexture(GL_TEXTURE_2D, id_tex);

    glUseProgram(bilateralish_gradientV_program->program);
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

//thanks to http://sciencefair.math.iit.edu/analysis/linereg/hand/
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
    std::vector<float> xsum (num_objects, 0.0f);
    std::vector<float> ysum (num_objects, 0.0f);
    std::vector<float> xysum (num_objects, 0.0f);
    std::vector<float> x2sum (num_objects, 0.0f);
    std::vector<float> y2sum (num_objects, 0.0f);
    std::vector<int> n (num_objects, 0);

    for(int y = 0; y<height; y++) {
        for(int x = 0; x<width; x++){
            int index = x+y*width;
            float r = pixels[index*4];
            float g = pixels[index*4+1];
            float b = pixels[index*4+2];
            float value = glm::max(glm::max(r, g), b);
            int id = ids[index*4]*255;

            xsum[id] += y;
            ysum[id] += value;
            xysum[id] += y*value;
            x2sum[id] += y*y;
            y2sum[id] += value*value;
            n[id]++;
        }
    }

    for(int y = 0; y<height; y++){
        for(int x = 0; x<width; x++){
            int index = x+y*width;
            float r = pixels[index*4];
            float g = pixels[index*4+1];
            float b = pixels[index*4+2];

            int id = ids[index*4]*255;
            float slope = ((n[id]*xysum[id]) - (xsum[id]*ysum[id]))/
                      ((n[id]*x2sum[id]) - (xsum[id]*xsum[id]));
            float intercept = ((x2sum[id]*ysum[id]) - (xsum[id]*xysum[id]))/
                      ((n[id]*x2sum[id]) - (xsum[id]*xsum[id]));
            float gradient_val = slope*y+intercept;

            gradient[index*4] = r*gradient_val;
            gradient[index*4+1] = g*gradient_val;
            gradient[index*4+2] = b*gradient_val;
            gradient[index*4+3] = 1.0f;
        }
    }
    gradient_png_tex = load_texture(width, height, gradient);
}

void PlantMode::draw_gradients_linfit(GLuint basic_tex, GLuint color_tex,
                                GLuint id_tex, GLuint *gradient_tex_)
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
    std::vector<float> xsum (chunk_num, 0.0f);
    std::vector<float> ysum (chunk_num, 0.0f);
    std::vector<float> xysum (chunk_num, 0.0f);
    std::vector<float> x2sum (chunk_num, 0.0f);
    std::vector<float> y2sum (chunk_num, 0.0f);
    std::vector<int> n (chunk_num, 0);

    //lots of help from this stackoverflow question
    //https://stackoverflow.com/questions/32094598/opengl-compute-shader-ssbo
    //calculate the linfit gradient equation things
    {
        GLuint xsum_ssbo;
        GLuint ysum_ssbo;
        GLuint xysum_ssbo;
        GLuint x2sum_ssbo;
        GLuint y2sum_ssbo;
        GLuint n_ssbo;

        glUseProgram(calculate_gradient_program->program);

        glGenBuffers(1, &xsum_ssbo);
        glGenBuffers(1, &ysum_ssbo);
        glGenBuffers(1, &xysum_ssbo);
        glGenBuffers(1, &x2sum_ssbo);
        glGenBuffers(1, &y2sum_ssbo);
        glGenBuffers(1, &n_ssbo);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, xsum_ssbo);
        glBufferData(GL_SHADER_STORAGE_BUFFER, chunk_num*sizeof(float), &xsum, GL_DYNAMIC_COPY);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ysum_ssbo);
        glBufferData(GL_SHADER_STORAGE_BUFFER, chunk_num*sizeof(float), &ysum, GL_DYNAMIC_COPY);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, xysum_ssbo);
        glBufferData(GL_SHADER_STORAGE_BUFFER, chunk_num*sizeof(float), &xysum, GL_DYNAMIC_COPY);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, x2sum_ssbo);
        glBufferData(GL_SHADER_STORAGE_BUFFER, chunk_num*sizeof(float), &x2sum, GL_DYNAMIC_COPY);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, y2sum_ssbo);
        glBufferData(GL_SHADER_STORAGE_BUFFER, chunk_num*sizeof(float), &y2sum, GL_DYNAMIC_COPY);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, n_ssbo);
        glBufferData(GL_SHADER_STORAGE_BUFFER, chunk_num*sizeof(int), &n, GL_DYNAMIC_COPY);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, color_tex);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, id_tex);


        glDispatchCompute(chunk_num/2, 1, 1);

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        auto load_result = [chunk_num](std::vector<float> *result, GLuint ssbo){
            assert(result);

            (*result).clear();
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);

            float *ptr = (float *) glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_WRITE_ONLY);
            for(int i = 0; i<chunk_num; i++){
                (*result).emplace_back(ptr[i]);
            }
        };

        load_result(&xsum, xsum_ssbo);
        load_result(&ysum, ysum_ssbo);
        load_result(&xysum, xysum_ssbo);
        load_result(&x2sum, x2sum_ssbo);
        load_result(&y2sum, y2sum_ssbo);

        n.clear();
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, n_ssbo);

        int *ptr = (int *) glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_WRITE_ONLY);
        for(int i = 0; i<chunk_num; i++){
            n.emplace_back(ptr[i]);
        }
    }

    //draw the gradient using the calculated values
    {

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, basic_tex);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, color_tex);

        glUseProgram(gradient_program->program);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
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

    draw_scene(&textures.basic_tex, &textures.color_tex, &textures.id_tex,
            &textures.depth_tex);
    if(show == GRADIENTS_BLUR){
        draw_gradients_blur(textures.basic_tex, textures.color_tex,
                textures.id_tex,
                &textures.gradient_temp_tex, &textures.gradient_tex);
    }else{
        draw_gradients_linfit(textures.basic_tex, textures.color_tex,
                textures.id_tex, &textures.gradient_tex);
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
