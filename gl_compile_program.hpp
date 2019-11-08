#pragma once

#include "GL.hpp"

#include <string>
#include <vector>

//compiles+links an OpenGL shader program from source.
// throws on compilation error.
GLuint gl_compile_program(
	std::string const &vertex_shader_source,
	std::string const &fragment_shader_source);

GLuint gl_compile_program(
        std::vector< std::pair <GLenum, std::string> > const &shader_sources);

