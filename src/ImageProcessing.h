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

#ifndef IMAGEPROCESSING_H
#define IMAGEPROCESSING_H

#include <vector>
#include "Image.h"

//Color conversion operations.
void YUYVToRGB(const unsigned char* yuyvData,Image& frame);
void YUYVToGreyscale(const unsigned char* yuyvData,Image& frame);
void NV12ToRGB(const unsigned char* nv12Data,Image& frame);
void NV12ToGreyscale(const unsigned char* nv12Data,Image& frame);
void RGBToRGB(const unsigned char* rgbData,Image& frame);
void RGBToGreyscale(const unsigned char* rgbData,Image& frame);
void BGRVerticalMirroredToRGB(const unsigned char* bgrData,Image& frame);

//RGB operations.
void BlendAdd(const Image& image1,const Image& image2,Image& outputImage);
void Gaussian(const Image& inputImage,Image& outputImage,const float radius);

//Greyscale operations.
void AutoLevels(const Image& inputImage,Image& outputImage,const unsigned int ignorePadding);
void Sobel(const Image& image,std::vector<float>& gradient); //Gradient is magnitude and angle interleaved.
void LineThinning(const Image& inputImage,Image& outputImage);
void HoughTransform(const Image& inputImage,Image& accumulationImage);

class Canny
{
	public:
		Canny(Canny&& other)=default;
		static Canny WithRadius(const float gaussianBlurRadius);

		void Process(const Image& inputImage,Image& outputImage);

		//Internal use only variables kept around to avoid large repeated allocations. Made public
		//to ease debugging.
		Image gaussianImage;
		Image autoLevelsImage;
		std::vector<float> normalizedHistogram;
		std::vector<float> gradient;
		Image nonMaximumSuppression;
	private:
		float gaussianBlurRadius;

		Canny(const float gaussianBlurRadius);
		Canny(const Canny&)=delete;
		Canny& operator=(Canny&)=delete;
};

#endif

