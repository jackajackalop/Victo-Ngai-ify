#include "PlantMode.hpp"

#include "LitColorTextureProgram.hpp"
#include "BoneLitColorTextureProgram.hpp"
#include "Load.hpp"
#include "Mesh.hpp"
#include "Scene.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"
#include "load_save_png.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <fstream>
#include <map>
#include <cstddef>
#include <random>
#include <unordered_map>

Load< MeshBuffer > plant_meshes(LoadTagDefault, [](){
		auto ret = new MeshBuffer(data_path("plant.pnct"));
		return ret;
		});

Load< GLuint > plant_meshes_for_lit_color_texture_program(LoadTagDefault, [](){
		return new GLuint(plant_meshes->make_vao_for_program(lit_color_texture_program->program));
		});

BoneAnimation::Animation const *plant_banim_wind = nullptr;
BoneAnimation::Animation const *plant_banim_walk = nullptr;

Load< BoneAnimation > plant_banims(LoadTagDefault, [](){
		auto ret = new BoneAnimation(data_path("plant.banims"));
		plant_banim_wind = &(ret->lookup("Wind"));
		plant_banim_walk = &(ret->lookup("Walk"));
		return ret;
		});

Load< GLuint > plant_banims_for_bone_lit_color_texture_program(LoadTagDefault, [](){
		return new GLuint(plant_banims->make_vao_for_program(bone_lit_color_texture_program->program));
		});

GLuint meshes_for_lit_color_texture_program = 0;
static Load< MeshBuffer > meshes(LoadTagDefault, []() -> MeshBuffer const * {
		MeshBuffer *ret = new MeshBuffer(data_path("rat_girl.pnct"));
		meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
		return ret;
		});

static Load< Scene > scene(LoadTagLate, []() -> Scene const * {
		Scene *ret = new Scene();
		ret->load(data_path("rat_girl.scene"), [](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
				auto &mesh = meshes->lookup(mesh_name);
				scene.drawables.emplace_back(transform);
				Scene::Drawable::Pipeline &pipeline = scene.drawables.back().pipeline;

				pipeline = lit_color_texture_program_pipeline;
				pipeline.vao = meshes_for_lit_color_texture_program;
				pipeline.type = mesh.type;
				pipeline.start = mesh.start;
				pipeline.count = mesh.count;
				});
		return ret;
		});

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
		}
	}

	if (evt.type == SDL_MOUSEBUTTONDOWN) {
		if (!mouse_captured) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
			mouse_captured = true;
		}
	}
	if (evt.type == SDL_MOUSEBUTTONUP) {
		if (mouse_captured) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			mouse_captured = false;
		}
	}
	if (evt.type == SDL_MOUSEMOTION) {
		if (mouse_captured) {
			float yaw = evt.motion.xrel / float(window_size.y) * camera->fovy;
			float pitch = -evt.motion.yrel / float(window_size.y) * camera->fovy;

			//update camera angles:
			camera_elevation = glm::clamp(camera_elevation + pitch, glm::radians(10.0f), glm::radians(80.0f));
			camera_azimuth = camera_azimuth + yaw;
		}
	}

	return false;
}

void PlantMode::update(float elapsed) {

}

void PlantMode::draw(glm::uvec2 const &drawable_size) {
	//Draw scene:
	camera->aspect = drawable_size.x / float(drawable_size.y);

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//set up basic OpenGL state:
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	scene->draw(*camera);

	GL_ERRORS();
}
