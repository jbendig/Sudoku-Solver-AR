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

#include "ShaderProgram.h"
#include <iostream>
#include <cstdio>

static std::experimental::optional<std::string> ReadFile(const std::string& path)
{
	FILE* file = fopen(path.c_str(),"rb");
	if(file == nullptr)
		return {};

	fseek(file,0,SEEK_END);
	const long length = ftell(file);
	rewind(file);

	if(length < 0)
	{
		fclose(file);
		return {};
	}

	std::string data;
	data.resize(length);
	const size_t bytesRead = fread(const_cast<char*>(data.data()),1,length,file); //TODO: Remove const_cast once full C++17 is available.
	fclose(file);

	if(bytesRead != static_cast<size_t>(length))
		return {};

	return data;
}

static bool CompileShader(const GLuint shader,const std::string& shaderSourceString)
{
	const char* shaderSources = shaderSourceString.c_str();
	glShaderSource(shader,1,&shaderSources,nullptr);
	glCompileShader(shader);

	GLint success = 0;
	glGetShaderiv(shader,GL_COMPILE_STATUS,&success);
	if(!success)
	{
		GLchar infoLog[4096];
		glGetShaderInfoLog(shader,4096,nullptr,infoLog);
		std::cout << "Shader compile error: " << infoLog << std::endl;
		return false;
	}

	return true;
}

ShaderProgram::ShaderProgram(ShaderProgram&& other)
{
	program = other.program;
	other.program = 0;
}

ShaderProgram::~ShaderProgram()
{
	glDeleteProgram(program);
}

std::experimental::optional<ShaderProgram> ShaderProgram::FromFile(const std::string& vertexShaderPath,const std::string& fragmentShaderPath)
{
	//Load source from file.
	const auto vertexFileData = ReadFile(vertexShaderPath);
	if(!vertexFileData)
	{
		std::cout << "Could not load vertex shader from: " << vertexShaderPath << std::endl;
		return {};
	}
	const std::string vertexShaderSource = vertexFileData.value();

	const auto fragmentFileData = ReadFile(fragmentShaderPath);
	if(!fragmentFileData)
	{
		std::cout << "Could not load fragment shader from: " << fragmentShaderPath << std::endl;
		return {};
	}
	const std::string fragmentShaderSource = fragmentFileData.value();

	//Compile shaders.
	const GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	if(!CompileShader(vertexShader,vertexShaderSource))
	{
		glDeleteShader(vertexShader);
		return {};
	}

	const GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	if(!CompileShader(fragmentShader,fragmentShaderSource))
	{
		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);
		return {};
	}

	//Link program.
	const GLuint shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram,vertexShader);
	glAttachShader(shaderProgram,fragmentShader);
	glLinkProgram(shaderProgram);

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	GLint success = 0;
	glGetProgramiv(shaderProgram,GL_LINK_STATUS,&success);
	if(!success)
	{
		GLchar infoLog[4096];
		glGetProgramInfoLog(shaderProgram,4096,nullptr,infoLog);
		std::cout << "Program linker error: " << infoLog << std::endl;
		return {};
	}

	return ShaderProgram(shaderProgram);
}

void ShaderProgram::Use() const
{
	glUseProgram(program);
}

GLuint ShaderProgram::Uniform(const std::string& uniformName) const
{
	return glGetUniformLocation(program,uniformName.c_str());
}

ShaderProgram::ShaderProgram(const GLuint program)
	: program(program)
{
}

