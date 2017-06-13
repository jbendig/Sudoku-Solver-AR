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
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/random.hpp>
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

static glm::mat3 BuildPerspectiveMatrix(const glm::vec2 p0,const glm::vec2 p1,const glm::vec2 p2,const glm::vec2 p3)
{
	//See Digital Image Warping page 55.

	const glm::vec2 d1 = p1 - p2;
	const glm::vec2 d2 = p3 - p2;
	const glm::vec2 d3 = p0 - p1 + p2 - p3;

	const float a13 = glm::determinant(glm::mat2(d3.x,d2.x,d3.y,d2.y)) / glm::determinant(glm::mat2(d1.x,d2.x,d1.y,d2.y));
	const float a23 = glm::determinant(glm::mat2(d1.x,d3.x,d1.y,d3.y)) / glm::determinant(glm::mat2(d1.x,d2.x,d1.y,d2.y));
	const float a11 = p1.x - p0.x + a13 * p1.x;
	const float a21 = p3.x - p0.x + a23 * p3.x;
	const float a31 = p0.x;
	const float a12 = p1.y - p0.y + a13 * p1.y;
	const float a22 = p3.y - p0.y + a23 * p3.y;
	const float a32 = p0.y;
	const float a33 = 1.0f;

	return glm::mat3(a11,a21,a31,a12,a22,a32,a13,a23,a33);
}

static void BuildGrid(const glm::mat3& matrix,const unsigned int gridSize,auto addPointFunc)
{
	if(gridSize == 0)
		return;

	const float dx = 1.0f / static_cast<float>(gridSize - 1);
	for(unsigned int y = 0;y < gridSize;y++)
	{
		for(unsigned int x = 0;x < gridSize;x++)
		{
			const float u = static_cast<float>(x) * dx;
			const float v = static_cast<float>(y) * dx;
			glm::vec3 p = glm::vec3(u,v,1.0f) * matrix;
			if(p[2] != 0.0f)
				p /= p[2];

			addPointFunc(u,v,p[0],p[1]);
		}
	}
}

static void BuildGridIndices(const unsigned int gridSize,std::vector<GLuint>& indices)
{
	indices.clear();

	for(unsigned int y = 0;y < (gridSize - 1);y++)
	{
		for(unsigned int x = 0;x < (gridSize - 1);x++)
		{
			const unsigned int topLeft = (y * gridSize) + x;
			const unsigned int topRight = topLeft + 1;
			const unsigned int bottomLeft = topLeft + gridSize;
			const unsigned int bottomRight = bottomLeft + 1;

			indices.push_back(topLeft);
			indices.push_back(topRight);
			indices.push_back(bottomRight);
			indices.push_back(bottomRight);
			indices.push_back(bottomLeft);
			indices.push_back(topLeft);
		}
	}
}

static void DrawImagePrivate(const ShaderProgram& imageProgram,const Image& srcImage,const GLfloat* vertices,const GLuint vertexCount,const GLuint* indices,const GLuint indexCount)
{
	if(srcImage.data.empty())
		return;

	imageProgram.Use();

	GLuint texture;
	glGenTextures(1,&texture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,texture);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glUniform1i(imageProgram.Uniform("inputTexture"),0);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,srcImage.width,srcImage.height,0,GL_RGB,GL_UNSIGNED_BYTE,&srcImage.data[0]);

	GLuint vao;
	glGenVertexArrays(1,&vao);
	glBindVertexArray(vao);

	GLuint vbo;
	glGenBuffers(1,&vbo);
	glBindBuffer(GL_ARRAY_BUFFER,vbo);
	glBufferData(GL_ARRAY_BUFFER,vertexCount * 5 * sizeof(GLfloat),vertices,GL_STATIC_DRAW);
	glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(GLfloat) * 5,nullptr);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(GLfloat) * 5,reinterpret_cast<GLvoid*>(sizeof(GLfloat) * 3));
	glEnableVertexAttribArray(1);

	glDrawElements(GL_TRIANGLES,indexCount,GL_UNSIGNED_INT,indices);

	glBindVertexArray(0);

	glDeleteBuffers(1,&vbo);
	glDeleteVertexArrays(1,&vao);
	glDeleteTextures(1,&texture);
	glUseProgram(0);
}

Painter::Painter()
	: imageProgram(ShaderProgram::FromFile("image.vert","image.frag").value()),
	  lineProgram(ShaderProgram::FromFile("line.vert","line.frag").value())
{
}

void Painter::DrawImage(const float x,float y,float width,float height,const Image& image)
{
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

	DrawImagePrivate(imageProgram,image,vertices,4,indices,6);
}

void Painter::DrawImage(const Point topLeft,const Point topRight,const Point bottomLeft,const Point bottomRight,const Image& image)
{
	float windowWidth = 0.0f;
	float windowHeight = 0.0f;
	Viewport::Get(windowWidth,windowHeight);

	glm::mat3 matrix = BuildPerspectiveMatrix(glm::vec2(topLeft.x,    -topLeft.y)     * 2.0f / glm::vec2(windowWidth,windowHeight) + glm::vec2(-1.0f,1.0f),
											  glm::vec2(topRight.x,   -topRight.y)    * 2.0f / glm::vec2(windowWidth,windowHeight) + glm::vec2(-1.0f,1.0f),
											  glm::vec2(bottomRight.x,-bottomRight.y) * 2.0f / glm::vec2(windowWidth,windowHeight) + glm::vec2(-1.0f,1.0f),
											  glm::vec2(bottomLeft.x, -bottomLeft.y)  * 2.0f / glm::vec2(windowWidth,windowHeight) + glm::vec2(-1.0f,1.0f));
	const unsigned int GRID_SIZE = 18;
	std::vector<float> vertices;
	BuildGrid(matrix,GRID_SIZE,[&vertices](const float u,const float v,const float x,const float y) {
		vertices.push_back(x);
		vertices.push_back(y);
		vertices.push_back(0.0f);
		vertices.push_back(u);
		vertices.push_back(v);
	});

	std::vector<GLuint> indices;
	BuildGridIndices(GRID_SIZE,indices);

	DrawImagePrivate(imageProgram,image,vertices.data(),vertices.size() / 5,indices.data(),indices.size());
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

void Painter::ExtractImage(const Image& srcImage,const Point topLeft,const Point topRight,const Point bottomLeft,const Point bottomRight,Image& dstImage,const unsigned int dstImageWidth,const unsigned int dstImageHeight)
{
	glm::mat3 matrix = BuildPerspectiveMatrix(glm::vec2(topLeft.x,topLeft.y),
											  glm::vec2(topRight.x,topRight.y),
											  glm::vec2(bottomRight.x,bottomRight.y),
											  glm::vec2(bottomLeft.x,bottomLeft.y));
	const unsigned int GRID_SIZE = 18;
	std::vector<float> vertices;
	BuildGrid(matrix,GRID_SIZE,[&vertices](const float u,const float v,const float x,const float y) {
		vertices.push_back(-1.0f + u * 2.0f);
		vertices.push_back(-1.0f + v * 2.0f);
		vertices.push_back(0.0f);
		vertices.push_back(x);
		vertices.push_back(y);
	});

	std::vector<GLuint> indices;
	BuildGridIndices(GRID_SIZE,indices);

	GLuint outputTexture;
	glGenTextures(1,&outputTexture);
	glBindTexture(GL_TEXTURE_2D,outputTexture);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,dstImageWidth,dstImageHeight,0,GL_RGB,GL_UNSIGNED_BYTE,nullptr);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);

	GLuint fbo;
	glGenFramebuffers(1,&fbo);
	glBindFramebuffer(GL_FRAMEBUFFER,fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,outputTexture,0);
	assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

	Viewport viewport(dstImageWidth,dstImageHeight);
	DrawImagePrivate(imageProgram,srcImage,vertices.data(),vertices.size() / 5,indices.data(),indices.size());

	dstImage.width = dstImageWidth;
	dstImage.height = dstImageHeight;
	dstImage.data.resize(dstImage.width * dstImage.height * 3);
	glReadPixels(0,0,dstImage.width,dstImage.height,GL_RGB,GL_UNSIGNED_BYTE,&dstImage.data[0]);

	glBindVertexArray(0);

	glDeleteFramebuffers(1,&fbo);
	glDeleteTextures(1,&outputTexture);
}

void Painter::ScaleImage(const Image& srcImage,Image& dstImage,const unsigned int dstImageWidth,const unsigned int dstImageHeight)
{
	ExtractImage(srcImage,
				 {0.0f,0.0f},
				 {1.0f,0.0f},
				 {0.0f,1.0f},
				 {1.0f,1.0f},
				 dstImage,
				 dstImageWidth,
				 dstImageHeight);
}

void Painter::DrawPuzzleGrid(const Image& srcImage,const float borderLineWidth,const float gridMinorLineWidth,const float gridMajorLineWidth,Image& dstImage)
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

	//Extract final image.
	dstImage.MatchSize(srcImage);
	glReadPixels(0,0,dstImage.width,dstImage.height,GL_RGB,GL_UNSIGNED_BYTE,&dstImage.data[0]);

	glDeleteFramebuffers(1,&fbo);
	glDeleteTextures(1,&outputTexture);
	glLineWidth(1.0f);
	glUseProgram(0);
}

void Painter::DrawNoise(const unsigned int width,const unsigned int height,const float noiseDelta)
{
	//Create a noise textures and use blending to apply them.
	Image addNoiseImage(width,height);
	Image subNoiseImage(width,height);
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
}

void Painter::DrawWarpedAndUnwarpedPuzzle(const Image& srcImage,const unsigned int frameBufferSize,const float perspectiveCornerRandomRadius,const float noiseDelta,Image& dstImage,const unsigned int dstImageSize)
{
	//Setup four corners with the render buffer where the srcImage will be rendered using a
	//perspective warp. Each corner is placed randomly within a circle that touches the respective
	//corner of the render buffer.
	// +---------
	// |/---\ <-
	// ||   | <- Corner point placed randomly in this circle.
	// |\---/ <-
	// |
	const float frameBufferSizef = frameBufferSize;
	const glm::vec2 p0 = glm::vec2(perspectiveCornerRandomRadius,perspectiveCornerRandomRadius) + glm::diskRand(perspectiveCornerRandomRadius);
	const glm::vec2 p1 = glm::vec2(frameBufferSizef - perspectiveCornerRandomRadius,perspectiveCornerRandomRadius) + glm::diskRand(perspectiveCornerRandomRadius);
	const glm::vec2 p2 = glm::vec2(frameBufferSizef - perspectiveCornerRandomRadius,frameBufferSizef - perspectiveCornerRandomRadius) + glm::diskRand(perspectiveCornerRandomRadius);
	const glm::vec2 p3 = glm::vec2(perspectiveCornerRandomRadius,frameBufferSizef - perspectiveCornerRandomRadius) + glm::diskRand(perspectiveCornerRandomRadius);
	const glm::mat3 generationMatrix = BuildPerspectiveMatrix(p0 * 2.0f / frameBufferSizef - glm::vec2(1.0f,1.0f),
															  p1 * 2.0f / frameBufferSizef - glm::vec2(1.0f,1.0f),
															  p2 * 2.0f / frameBufferSizef - glm::vec2(1.0f,1.0f),
															  p3 * 2.0f / frameBufferSizef - glm::vec2(1.0f,1.0f));
	const glm::mat3 extractionMatrix = BuildPerspectiveMatrix(p0 / frameBufferSizef,
															  p1 / frameBufferSizef,
															  p2 / frameBufferSizef,
															  p3 / frameBufferSizef);

	//Setup framebuffer to render perspective warp to.
	GLuint workingTexture;
	glGenTextures(1,&workingTexture);
	glBindTexture(GL_TEXTURE_2D,workingTexture);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,frameBufferSize,frameBufferSize,0,GL_RGB,GL_UNSIGNED_BYTE,nullptr);

	GLuint fbo;
	glGenFramebuffers(1,&fbo);
	glBindFramebuffer(GL_FRAMEBUFFER,fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,workingTexture,0);
	assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

	Viewport viewport(frameBufferSize,frameBufferSize);

	//Setup an overly fine mesh with perspective warp and render srcImage with it. The extra
	//quality isn't really necessary but it's an offline process so whatever.
	constexpr unsigned int GRID_SIZE = 80; //Number of lines, including ends.
	std::vector<GLfloat> vertices;
	BuildGrid(generationMatrix,GRID_SIZE,[&vertices](const float u,const float v,const float x,const float y) {
		vertices.push_back(x);
		vertices.push_back(y);
		vertices.push_back(0.0f);
		vertices.push_back(u);
		vertices.push_back(v);
	});

	std::vector<GLuint> indices;
	BuildGridIndices(GRID_SIZE,indices);

	DrawImagePrivate(imageProgram,srcImage,vertices.data(),vertices.size() / 5,indices.data(),indices.size());

	//Draw noise over the framebuffer to simulate noise from a camera.
	DrawNoise(frameBufferSize,frameBufferSize,noiseDelta);

	//Extract framebuffer as an image. This part could be skipped in favor of doing the rest of the
	//work directly on the GPU. But, this lets us re-use the ExtractImage() function which is used
	//to extract a puzzle from a video frame.
	Image renderBufferImage(frameBufferSize,frameBufferSize);
	glReadPixels(0,0,renderBufferImage.width,renderBufferImage.height,GL_RGB,GL_UNSIGNED_BYTE,&renderBufferImage.data[0]);

	//Clean-up.
	glDeleteFramebuffers(1,&fbo);
	glDeleteTextures(1,&workingTexture);
	glLineWidth(1.0f);
	glUseProgram(0);

	//Extract puzzle from framebuffer so similar noise and distortions are applied as if the puzzle
	//was pulled from a real image.
	std::vector<Point> extractionPoints;
	extractionPoints.reserve(4);
	BuildGrid(extractionMatrix,2,[&extractionPoints](const float u,const float v,const float x,const float y) {
			extractionPoints.push_back({x,y});
	});
	ExtractImage(renderBufferImage,
				 extractionPoints[0],
				 extractionPoints[1],
				 extractionPoints[2],
				 extractionPoints[3],
				 dstImage,
				 dstImageSize,
				 dstImageSize);
}

