#pragma once

#include "Mode.hpp"

#include "BoneAnimation.hpp"
#include "GL.hpp"
#include "Scene.hpp"

#include <SDL.h>
#include <glm/glm.hpp>

#include <vector>
#include <list>

// The 'PlantMode' mode shows off some bone animations:

struct PlantMode : public Mode {
	PlantMode();
	virtual ~PlantMode();

	virtual bool handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;
	//draws out basic scene render, adjusted flat color fills with cell shading
	void draw_scene(GLuint *basic_tex_, GLuint *color_tex_, GLuint *depth_tex_);
	//draws out gradients
	void draw_gradients_linfit(GLuint basic_tex, GLuint color_tex,
                        GLuint *gradient_tex_);
    void draw_gradients_blur(GLuint basic_tex, GLuint color_tex,
                        GLuint depth_tex,
                        GLuint *gradient_temp_tex_, GLuint *gradient_tex_);
    void cpu_gradient(GLuint basic_tex, GLuint color_tex);
	//paper textures
	void draw_texture();
	//screen tones in shadows
	void draw_screentones();
	//lineart
	void draw_lines();
	//draws the vignette effect
	void draw_vignette();

	//controls:
	bool mouse_captured = false;
	bool forward = false;
	bool backward = false;

	//scene:
	Scene::Drawable *plant = nullptr;
	Scene::Camera *camera = nullptr;

};
