#pragma once

#include "Mode.hpp"
#include "GL.hpp"
#include "Scene.hpp"

#include <SDL.h>
#include <glm/glm.hpp>

#include <vector>
#include <list>

struct PlantMode : public Mode {
	PlantMode();
	virtual ~PlantMode();

	virtual bool handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

    //creates the shadow map
    void draw_shadows(GLuint *shadow_depth_tex_);
	//copys the shadow map to the screen
    void draw_shadow_debug(GLuint shadow_depth_tex, GLuint *shadow_tex_);
	//draws out the paper textures
	void draw_surface(GLuint paper_tex, GLuint *surface_tex_);
    //draws out basic scene render, adjusted flat color fills, cell shading, depth buffer, and also the geometric normals to separate textures
	void draw_scene(GLuint shadow_depth_tex,
            GLuint *basic_tex_, GLuint *color_tex_, GLuint *texColor_tex_,
            GLuint *control_tex_, GLuint *depth_tex_,
            GLuint *id_tex_, GLuint *normal_tex_, GLuint *shadow_tex_,
            GLuint *toon_tex_);
	//draws out linear-fit gradients for both main masses and the toon shaded elements. Also creates line art.
	void draw_simplify(GLuint basic_tex, GLuint color_tex,
                        GLuint shadow_tex,
                        GLuint toon_tex, GLuint id_tex, GLuint normal_tex,
                        GLuint depth_tex,
                        GLuint *gradient_tex_, GLuint *gradient_shadow_tex_,
                        GLuint *gradient_toon_tex_, GLuint *line_tex);
    //cpu gradient fitting for debugging purposes
    void draw_gradients_cpu(GLuint basic_tex, GLuint color_tex,
                        GLuint id_tex, GLuint *gradient_tex_);
    void cpu_gradient(GLuint basic_tex, GLuint color_tex, GLuint id_tex);
    //combines gradient effect, toon shading, line art, paper textures, detail textures, and vignette effect
    void draw_combine(GLuint color_tex,
            GLuint id_tex, GLuint texColor_tex, GLuint control_tex,
            GLuint gradient_tex, GLuint gradient_shadow_tex,
            GLuint gradient_toon_tex, GLuint line_tex, GLuint surface_tex,
            GLuint *shaded_tex_);
};
