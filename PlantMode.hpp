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
    void draw_shadows(GLuint *shadow_depth_tex_);
	void draw_scene(GLuint shadow_depth_tex,
            GLuint *basic_tex_, GLuint *color_tex_,
            GLuint *depth_tex_, GLuint *id_tex_, GLuint *toon_tex_);
	//draws out gradients
    void cpu_gradient(GLuint basic_tex, GLuint color_tex, GLuint id_tex);
	void draw_gradients_linfit(GLuint basic_tex, GLuint color_tex,
                        GLuint toon_tex, GLuint id_tex, GLuint *gradient_tex_,
                        GLuint *gradient_toon_tex_);
    void draw_gradients_cpu(GLuint basic_tex, GLuint color_tex,
                        GLuint id_tex, GLuint *gradient_tex_);
    void draw_shading(GLuint gradient_tex, GLuint gradient_toon_tex,
                        GLuint *shaded_tex_);
	//paper textures
	void draw_surface(GLuint paper_tex, GLuint *surface_tex_);
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

    glm::mat4 light_mat = glm::ortho( -10.0f, 10.0f, -10.0f, 10.0f, 0.0f, 100.0f);
    glm::quat light_rotation = glm::quat(glm::vec3(glm::radians(25.0f), glm::radians(25.0f), 0.0f));

    glm::mat4 make_light_to_local(glm::quat rotation) const {
        glm::vec3 inv_scale = glm::vec3(1.0, 1.0, 1.0);
        return glm::mat4( //un-scale
    		        glm::vec4(inv_scale.x, 0.0f, 0.0f, 0.0f),
        	    	glm::vec4(0.0f, inv_scale.y, 0.0f, 0.0f),
		            glm::vec4(0.0f, 0.0f, inv_scale.z, 0.0f),
            		glm::vec4(0.0f, 0.0f, 0.0f, 1.0f))
            * glm::mat4_cast(glm::inverse(light_rotation)); //un-rotate
    }
};
