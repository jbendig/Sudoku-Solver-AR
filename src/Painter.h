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
		Painter();

		void DrawImage(const float x,float y,float width,float height,const Image& image);
		void DrawImage(const std::vector<Point>& targetPoints,const Image& image);
		void DrawLine(float x1,float y1,float x2,float y2,const unsigned char red,const unsigned char green,const unsigned char blue);

		//These draw to an alternative framebuffer.
		void ExtractImage(const Image& srcImage,const std::vector<Point>& srcPoints,const float srcPointScaleX,const float srcPointScaleY,Image& dstImage,const unsigned int dstImageWidth,const unsigned int dstImageHeight);
		void ScaleImage(const Image& srcImage,Image& dstImage,const unsigned int dstImageWidth,const unsigned int dstImageHeight);
		void DrawPuzzleOverlay(const Image& srcImage,const float borderLineWidth,const float gridMinorLineWidth,const float gridMajorLineWidth,const float noiseDelta,Image& dstImage);
	private:
		ShaderProgram imageProgram;
		ShaderProgram lineProgram;

		Painter(Painter&& other)=delete;
		Painter(const Painter&)=delete;
		Painter& operator=(Painter&)=delete;
};

#endif

