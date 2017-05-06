// Copyright 2017 James Bendig. See the COPYRIGHT file at the top-level
// directory of this distribution.
//
// Licensed under:
//   the MIT license
//     <LICENSE-MIT or https://opensource.org/licenses/MIT>
//   or the Apache License, Version 2.0
//     <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>,
// at your option. This file may not be copied, modified, or distributed
// except according to those terms.

#ifndef SHADERPROGRAM_H
#define SHADERPROGRAM_H

#ifdef __linux
#include <experimental/optional>
#elif defined _WIN32
#include <optional>
#endif
#include <string>

#ifdef __linux
#include <GLES3/gl3.h>
#elif defined _WIN32
#include <GL/glew.h>
#endif

class ShaderProgram
{
	public:
		ShaderProgram(ShaderProgram&& other);
		~ShaderProgram();

		static
#ifdef __linux
			std::experimental::optional<ShaderProgram>
#elif defined _WIN32
			std::optional<ShaderProgram>
#endif
			FromFile(const std::string& vertexShaderPath,const std::string& fragmentShaderPath);

		void Use() const;
		GLuint Uniform(const std::string& uniformName) const;
	private:
		GLuint program;

		ShaderProgram(const GLuint program);
		ShaderProgram(const ShaderProgram&)=delete;
		ShaderProgram& operator=(ShaderProgram&)=delete;
};

#endif

