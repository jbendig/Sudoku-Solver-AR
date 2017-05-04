#ifndef PAINTER_H
#define PAINTER_H

#include "ShaderProgram.h"

class Image;

class Painter
{
	public:
		Painter(const unsigned int windowWidth,const unsigned int windowHeight);

		void DrawImage(const float x,float y,float width,float height,const Image& image);
		void DrawLine(float x1,float y1,float x2,float y2,const unsigned char red,const unsigned char green,const unsigned char blue);
	private:
		float windowWidth;
		float windowHeight;
		ShaderProgram imageProgram;
		ShaderProgram lineProgram;

		Painter(Painter&& other)=delete;
		Painter(const Painter&)=delete;
		Painter& operator=(Painter&)=delete;
};

#endif

