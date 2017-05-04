#ifndef SHADERPROGRAM_H
#define SHADERPROGRAM_H

#include <experimental/optional>
#include <string>
#include <GLES3/gl3.h>

class ShaderProgram
{
	public:
		ShaderProgram(ShaderProgram&& other);
		~ShaderProgram();

		static std::experimental::optional<ShaderProgram> FromFile(const std::string& vertexShaderPath,const std::string& fragmentShaderPath);

		void Use() const;
		GLuint Uniform(const std::string& uniformName) const;
	private:
		GLuint program;

		ShaderProgram(const GLuint program);
		ShaderProgram(const ShaderProgram&)=delete;
		ShaderProgram& operator=(ShaderProgram&)=delete;
};

#endif

