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
#include <random>
#include <cmath>
#ifdef __linux
#include <GLES3/gl3.h>
#elif defined _WIN32
#include <GL/glew.h>
#endif
#include "Image.h"


namespace
{

class Viewport
{
	public:
		Viewport(const int width,const int height)
		{
			Viewport::Get(oldViewportWidth,oldViewportHeight);
			glViewport(0,0,width,height);
		}

		~Viewport()
		{
			glViewport(0,0,oldViewportWidth,oldViewportHeight);
		}

		template <class T>
		static void Get(T& width,T& height)
		{
			int data[4];
			glGetIntegerv(GL_VIEWPORT,data);
			width = static_cast<T>(data[2]);
			height = static_cast<T>(data[3]);
		}
	private:
		int oldViewportWidth;
		int oldViewportHeight;

		Viewport(Viewport&&)=delete;
		Viewport(const Viewport&)=delete;
		Viewport& operator=(Viewport&)=delete;
};

};

Painter::Painter()
	: imageProgram(ShaderProgram::FromFile("image.vert","image.frag").value()),
	  lineProgram(ShaderProgram::FromFile("line.vert","line.frag").value())
{
}

void Painter::DrawImage(const float x,float y,float width,float height,const Image& image)
{
	if(image.data.empty())
		return;

	imageProgram.Use();

	float windowWidth = 0.0f;
	float windowHeight = 0.0f;
	Viewport::Get(windowWidth,windowHeight);

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

void Painter::DrawImage(const std::vector<Point>& targetPoints,const Image& image)
{
	if(image.data.empty())
		return;
	if(targetPoints.size() != 16)
		return;

	imageProgram.Use();

	float windowWidth = 0.0f;
	float windowHeight = 0.0f;
	Viewport::Get(windowWidth,windowHeight);

	std::vector<GLfloat> vertices;
	for(unsigned int y = 0; y < 4;y++)
	{
		for(unsigned int x = 0;x < 4;x++)
		{
			const unsigned int targetPointsIndex = (y * 4) + x;
			vertices.push_back((targetPoints[targetPointsIndex].x / windowWidth) * 2.0f - 1.0f); //X
			vertices.push_back(1.0f - (targetPoints[targetPointsIndex].y / windowHeight) * 2.0f); //Y
			vertices.push_back(0.0f); //Z

			vertices.push_back(x * 1.0f / 3.0f); //U
			vertices.push_back(y * 1.0f / 3.0f); //V
		}
	}

	const GLuint indices[] = {
		0,1,5,
		0,5,4,
		1,2,6,
		1,6,5,
		2,3,7,
		2,7,6,
		4,5,9,
		4,9,8,
		5,6,10,
		5,10,9,
		6,7,11,
		6,11,10,
		8,9,13,
		8,13,12,
		9,10,14,
		9,14,13,
		10,11,15,
		10,15,14,
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
	glBufferData(GL_ARRAY_BUFFER,vertices.size() * sizeof(float),&vertices[0],GL_STATIC_DRAW);
	glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(GLfloat) * 5,nullptr);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(GLfloat) * 5,reinterpret_cast<GLvoid*>(sizeof(GLfloat) * 3));
	glEnableVertexAttribArray(1);

	glDrawElements(GL_TRIANGLES,18*3,GL_UNSIGNED_INT,indices);

	glBindVertexArray(0);

	glDeleteBuffers(1,&vbo);
	glDeleteVertexArrays(1,&vao);
	glDeleteTextures(1,&texture);
	glUseProgram(0);
}

void Painter::DrawLine(float x1,float y1,float x2,float y2,const unsigned char red,const unsigned char green,const unsigned char blue)
{
	lineProgram.Use();

	float windowWidth = 0.0f;
	float windowHeight = 0.0f;
	Viewport::Get(windowWidth,windowHeight);

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

void Painter::ExtractImage(const Image& srcImage,const std::vector<Point>& srcPoints,const float srcPointScaleX,const float srcPointScaleY,Image& dstImage,const unsigned int dstImageWidth,const unsigned int dstImageHeight)
{
	//srcPoints should be 4x4 points that are used for generating where srcImage will be sampled.
	//The extra points are used to reduce the bilinear artifacts when performing a perspective warp.
	//See Digital Image Warping section 7.2.3.

	if(srcImage.data.empty())
		return;
	if(srcPoints.size() != 16)
		return;

	imageProgram.Use();

	std::vector<GLfloat> vertices;
	for(unsigned int y = 0; y < 4;y++)
	{
		for(unsigned int x = 0;x < 4;x++)
		{
			vertices.push_back(-1.0f + x * 2.0f / 3.0f); //X
			vertices.push_back(-1.0f + y * 2.0f / 3.0f); //Y
			vertices.push_back(0.0f); //Z

			const unsigned int srcPointsIndex = (y * 4) + x;
			vertices.push_back(srcPoints[srcPointsIndex].x * srcPointScaleX); //U
			vertices.push_back(srcPoints[srcPointsIndex].y * srcPointScaleY); //V
		}
	}

	const GLuint indices[] = {
		0,1,5,
		0,5,4,
		1,2,6,
		1,6,5,
		2,3,7,
		2,7,6,
		4,5,9,
		4,9,8,
		5,6,10,
		5,10,9,
		6,7,11,
		6,11,10,
		8,9,13,
		8,13,12,
		9,10,14,
		9,14,13,
		10,11,15,
		10,15,14,
	};

	GLuint outputTexture;
	glGenTextures(1,&outputTexture);
	glBindTexture(GL_TEXTURE_2D,outputTexture);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,dstImageWidth,dstImageHeight,0,GL_RGB,GL_UNSIGNED_BYTE,nullptr);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);

	GLuint inputTexture;
	glGenTextures(1,&inputTexture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,inputTexture);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glUniform1i(imageProgram.Uniform("inputTexture"),0);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,srcImage.width,srcImage.height,0,GL_RGB,GL_UNSIGNED_BYTE,&srcImage.data[0]);

	GLuint fbo;
	glGenFramebuffers(1,&fbo);
	glBindFramebuffer(GL_FRAMEBUFFER,fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,outputTexture,0);
	assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

	GLuint vao;
	glGenVertexArrays(1,&vao);
	glBindVertexArray(vao);

	GLuint vbo;
	glGenBuffers(1,&vbo);
	glBindBuffer(GL_ARRAY_BUFFER,vbo);
	glBufferData(GL_ARRAY_BUFFER,vertices.size() * sizeof(float),&vertices[0],GL_STATIC_DRAW);
	glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(GLfloat) * 5,nullptr);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(GLfloat) * 5,reinterpret_cast<GLvoid*>(sizeof(GLfloat) * 3));
	glEnableVertexAttribArray(1);

	Viewport viewport(dstImageWidth,dstImageHeight);
	glDrawElements(GL_TRIANGLES,18*3,GL_UNSIGNED_INT,indices);

	dstImage.width = dstImageWidth;
	dstImage.height = dstImageHeight;
	dstImage.data.resize(dstImage.width * dstImage.height * 3);
	glReadPixels(0,0,dstImage.width,dstImage.height,GL_RGB,GL_UNSIGNED_BYTE,&dstImage.data[0]);

	glBindVertexArray(0);

	glDeleteBuffers(1,&vbo);
	glDeleteVertexArrays(1,&vao);
	glDeleteFramebuffers(1,&fbo);
	glDeleteTextures(1,&inputTexture);
	glDeleteTextures(1,&outputTexture);
	glUseProgram(0);
}

void Painter::ScaleImage(const Image& srcImage,Image& dstImage,const unsigned int dstImageWidth,const unsigned int dstImageHeight)
{
	std::vector<Point> points = {
		{0.0f,0.0f},
		{1/3.0f,0.0f},
		{2/3.0f,0.0f},
		{1.0f,0.0f},

		{0.0f,1/3.0f},
		{1/3.0f,1/3.0f},
		{2/3.0f,1/3.0f},
		{1.0f,1/3.0f},

		{0.0f,2/3.0f},
		{1/3.0f,2/3.0f},
		{2/3.0f,2/3.0f},
		{1.0f,2/3.0f},

		{0.0f,1.0f},
		{1/3.0f,1.0f},
		{2/3.0f,1.0f},
		{1.0f,1.0f},
	};
	ExtractImage(srcImage,points,1.0f,1.0f,dstImage,dstImageWidth,dstImageHeight);
}

void Painter::DrawPuzzleOverlay(const Image& srcImage,const float borderLineWidth,const float gridMinorLineWidth,const float gridMajorLineWidth,const float noiseDelta,Image& dstImage)
{
	GLuint outputTexture;
	glGenTextures(1,&outputTexture);
	glBindTexture(GL_TEXTURE_2D,outputTexture);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,srcImage.width,srcImage.height,0,GL_RGB,GL_UNSIGNED_BYTE,&srcImage.data[0]);

	GLuint fbo;
	glGenFramebuffers(1,&fbo);
	glBindFramebuffer(GL_FRAMEBUFFER,fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,outputTexture,0);
	assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

	Viewport viewport(srcImage.width,srcImage.height);

	//Draw border.
	glLineWidth(borderLineWidth);
	DrawLine(0,0,srcImage.width,0,0,0,0);
	DrawLine(srcImage.width,0,srcImage.width,srcImage.height,0,0,0);
	DrawLine(0,srcImage.height,srcImage.width,srcImage.height,0,0,0);
	DrawLine(0,0,0,srcImage.height,0,0,0);

	//Draw grid.
	for(unsigned int x = 1;x < 9;x++)
	{
		if((x % 3) == 0)
			glLineWidth(gridMajorLineWidth);
		else
			glLineWidth(gridMinorLineWidth);

		const float dx = x * (srcImage.width / 9.0f);
		const float dy = x * (srcImage.height / 9.0f);
		DrawLine(dx,0,dx,srcImage.height,0,0,0);
		DrawLine(0,dy,srcImage.width,dy,0,0,0);
	}

	//Create a noise textures and use blending to apply them.
	Image addNoiseImage(srcImage.width,srcImage.height);
	Image subNoiseImage(srcImage.width,srcImage.height);
	std::random_device randomDevice;
	std::mt19937 randomNumberGenerator(randomDevice());
	for(unsigned int x = 0;x < addNoiseImage.width * addNoiseImage.height;x++)
	{
		const unsigned int index = x * 3;
		const int value = lrint((randomNumberGenerator() / static_cast<double>(std::mt19937::max()) - 0.5) * noiseDelta * 255.0);
		if(value >= 0)
		{
			addNoiseImage.data[index + 0] = value;
			addNoiseImage.data[index + 1] = value;
			addNoiseImage.data[index + 2] = value;
			subNoiseImage.data[index + 0] = 0;
			subNoiseImage.data[index + 1] = 0;
			subNoiseImage.data[index + 2] = 0;
		}
		else
		{
			addNoiseImage.data[index + 0] = 0;
			addNoiseImage.data[index + 1] = 0;
			addNoiseImage.data[index + 2] = 0;
			subNoiseImage.data[index + 0] = -value;
			subNoiseImage.data[index + 1] = -value;
			subNoiseImage.data[index + 2] = -value;
		}
	}

	glEnable(GL_BLEND);
	glBlendFuncSeparate(GL_ONE,GL_ONE,GL_ONE,GL_ONE);
	glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
	DrawImage(0,0,addNoiseImage.width,addNoiseImage.height,addNoiseImage);
	glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT,GL_FUNC_ADD);
	DrawImage(0,0,subNoiseImage.width,subNoiseImage.height,subNoiseImage);
	glDisable(GL_BLEND);

	//Extract final image.
	dstImage.MatchSize(srcImage);
	glReadPixels(0,0,dstImage.width,dstImage.height,GL_RGB,GL_UNSIGNED_BYTE,&dstImage.data[0]);

	glDeleteFramebuffers(1,&fbo);
	glDeleteTextures(1,&outputTexture);
	glLineWidth(1.0f);
	glUseProgram(0);
}
