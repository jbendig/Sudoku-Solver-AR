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

#ifndef PAINTER_H
#define PAINTER_H

#include <vector>
#include "Geometry.h"
#include "ShaderProgram.h"

struct Image;

class Painter
{
	public:
		Painter(const unsigned int windowWidth,const unsigned int windowHeight);

		void DrawImage(const float x,float y,float width,float height,const Image& image);
		void DrawImage(const std::vector<Point>& targetPoints,const Image& image);
		void DrawLine(float x1,float y1,float x2,float y2,const unsigned char red,const unsigned char green,const unsigned char blue);
		void DrawPoints(const std::vector<std::pair<float,float>>& points,const unsigned char red,const unsigned char green,const unsigned char blue);

		void ExtractImage(const Image& srcImage,const std::vector<Point>& srcPoints,const float srcPointScaleX,const float srcPointScaleY,Image& dstImage,const unsigned int dstImageWidth,const unsigned int dstImageHeight); //Note: This modifies glViewport.
	private:
		float windowWidth;
		float windowHeight;
		ShaderProgram imageProgram;
		ShaderProgram lineProgram;
		ShaderProgram pointProgram;

		Painter(Painter&& other)=delete;
		Painter(const Painter&)=delete;
		Painter& operator=(Painter&)=delete;
};

#endif

