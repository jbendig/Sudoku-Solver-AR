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

#include "Painter.h"
#ifdef __linux
#include <GLES3/gl3.h>
#elif defined _WIN32
#include <GL/glew.h>
#endif
#include "Image.h"

Painter::Painter(const unsigned int windowWidth,const unsigned int windowHeight)
	: windowWidth(static_cast<float>(windowWidth)),
	  windowHeight(static_cast<float>(windowHeight)),
	  imageProgram(ShaderProgram::FromFile("image.vert","image.frag").value()),
	  lineProgram(ShaderProgram::FromFile("line.vert","line.frag").value())
{
}

void Painter::DrawImage(const float x,float y,float width,float height,const Image& image)
{
	if(image.data.empty())
		return;

	imageProgram.Use();

	//Convert coordinates from origin being the top left and the range being 0 to windowWidth and 0
	//to windowHeight to Normalized Device Coordinates where the origin is the center and the range
	//is from -1 to 1.0.
	float left = x / windowWidth;
	float right = left + width / windowWidth;
	float top = y / windowHeight;
	float bottom = top + height / windowHeight;

	left = left * 2.0f - 1.0f;
	right = right * 2.0f - 1.0f;
	top = 1.0f - top * 2.0f;
	bottom = 1.0f - bottom * 2.0f;

	const GLfloat vertices[] = {
		left,  top,    0.0f, 0.0f, 0.0f,
		right, top,    0.0f, 1.0f, 0.0f,
		right, bottom, 0.0f, 1.0f, 1.0f,
		left,  bottom, 0.0f, 0.0f, 1.0f,
	};

	const GLuint indices[] = {
		0,1,2,
		2,3,0,
	};

	GLuint texture;
	glGenTextures(1,&texture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,texture);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glUniform1i(imageProgram.Uniform("inputTexture"),0);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,image.width,image.height,0,GL_RGB,GL_UNSIGNED_BYTE,&image.data[0]);

	GLuint vao;
	glGenVertexArrays(1,&vao);
	glBindVertexArray(vao);

	GLuint vbo;
	glGenBuffers(1,&vbo);
	glBindBuffer(GL_ARRAY_BUFFER,vbo);
	glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);
	glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(GLfloat) * 5,nullptr);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(GLfloat) * 5,reinterpret_cast<GLvoid*>(sizeof(GLfloat) * 3));
	glEnableVertexAttribArray(1);

	glDrawElements(GL_TRIANGLES,6,GL_UNSIGNED_INT,indices);

	glBindVertexArray(0);

	glDeleteBuffers(1,&vbo);
	glDeleteVertexArrays(1,&vao);
	glDeleteTextures(1,&texture);
	glUseProgram(0);
}

void Painter::DrawLine(float x1,float y1,float x2,float y2,const unsigned char red,const unsigned char green,const unsigned char blue)
{
	lineProgram.Use();

	const float redf = red / 255.0f;
	const float greenf = green / 255.0f;
	const float bluef = blue / 255.0f;
	const GLfloat vertices[] = {
		(x1 / windowWidth) * 2.0f - 1.0f, 1.0f - (y1 / windowHeight) * 2.0f, 0.0f, redf, greenf, bluef,
		(x2 / windowWidth) * 2.0f - 1.0f, 1.0f - (y2 / windowHeight) * 2.0f, 0.0f, redf, greenf, bluef,
	};

	const GLuint indices[] = {
		0,1,
	};

	GLuint vao;
	glGenVertexArrays(1,&vao);
	glBindVertexArray(vao);

	GLuint vbo;
	glGenBuffers(1,&vbo);
	glBindBuffer(GL_ARRAY_BUFFER,vbo);
	glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);
	glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(GLfloat) * 6,nullptr);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(GLfloat) * 6,reinterpret_cast<GLvoid*>(sizeof(GLfloat) * 3));
	glEnableVertexAttribArray(1);

	glDrawElements(GL_LINES,2,GL_UNSIGNED_INT,indices);

	glBindVertexArray(0);

	glDeleteBuffers(1,&vbo);
	glDeleteVertexArrays(1,&vao);
	glUseProgram(0);
}
