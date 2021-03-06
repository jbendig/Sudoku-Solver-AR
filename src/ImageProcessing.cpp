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

#include "ImageProcessing.h"
#include <algorithm>
#include <numeric>
#include <stack>
#include <cmath>
#include <cstring>

template <class T>
static T Clamp(const T value,const T minimum,const T maximum)
{
	return std::min(std::max(value,minimum),maximum);
}

template <class T>
static unsigned char ClampToU8(const T value)
{
	return static_cast<unsigned char>(Clamp(value,static_cast<T>(0),static_cast<T>(255)));
}

static void Histogram(const Image& image,std::vector<float>& normalizedHistogram)
{
	//Note: Assumes image is greyscale.
	
	normalizedHistogram.resize(256);
	std::fill(normalizedHistogram.begin(),normalizedHistogram.end(),0.0f);

	const unsigned int pixelCount = image.width * image.height;
	if(pixelCount == 0)
		return;

	//Count the pixels.
	for(unsigned int x = 0;x < pixelCount;x++)
	{
		const unsigned int index = x * 3;
		normalizedHistogram[image.data[index]] += 1.0f;
	}

	//Normalize the histogram so the sum of all of the steps equal 1.0.
	const float divisor = 1.0f / static_cast<float>(pixelCount);
	for(float& value : normalizedHistogram)
	{
		value *= divisor;
	}
}

static unsigned char OtsusMethod(const std::vector<float>& normalizedHistogram)
{
	//Based on Digital Image Processing Third Edition. Chapter 10.3.3. Page 742.

	assert(normalizedHistogram.size() == 256);

	//Calculate P_1(k).
	std::vector<float> cumulativeSums(256,normalizedHistogram[0]);
	for(unsigned int x = 1;x < cumulativeSums.size();x++)
	{
		cumulativeSums[x] = cumulativeSums[x - 1] + normalizedHistogram[x];
	}
	
	//Calculate m(k).
	std::vector<float> cumulativeMeans(256,0.0f);
	for(unsigned int x = 1;x < cumulativeMeans.size();x++)
	{
		cumulativeMeans[x] = cumulativeMeans[x - 1] + normalizedHistogram[x] * static_cast<float>(x);
	}
	
	//Calculate m_G.
	const float globalIntensityMean = cumulativeMeans.back();
	
	//Calculate O^2_B(k) and find the index(s) of the maximum.
	std::vector<unsigned int> betweenClassVarianceIndexes;
	float betweenClassVarianceMax = 0.0f;
	for(unsigned int x = 0;x < 256;x++)
	{
		const float numerator = globalIntensityMean * cumulativeSums[x] - cumulativeMeans[x];
		const float numerator2 = numerator * numerator;
		const float denominator = cumulativeSums[x] * (1.0f - cumulativeSums[x]);

		const float betweenClassVariance = denominator == 0.0f ? 0.0f : numerator2 / denominator;
		if(betweenClassVariance > betweenClassVarianceMax)
		{
			betweenClassVarianceIndexes.clear();
			betweenClassVarianceIndexes.push_back(x);
			betweenClassVarianceMax = betweenClassVariance;
		}
		else if(betweenClassVariance == betweenClassVarianceMax)
			betweenClassVarianceIndexes.push_back(x);
	}

	return std::accumulate(betweenClassVarianceIndexes.begin(),betweenClassVarianceIndexes.end(),0) / betweenClassVarianceIndexes.size();
}

static void NonMaximumSuppression(const std::vector<float>& gradient,const unsigned int width,const unsigned int height,std::vector<unsigned char>& nonMaximumSuppression,const unsigned char lowThreshold,const unsigned char highThreshold)
{
	//Based on Digital Image Processing Third Edition. Chapter 10.2. Page 721.

	assert(gradient.size() == width * height * 2);

	//Perform Non-Maximum Suppression and Hysterasis Thresholding.
	nonMaximumSuppression.resize(width * height * 3);
	std::fill(nonMaximumSuppression.begin(),nonMaximumSuppression.end(),0);
	for(unsigned int y = 1;y < height - 1;y++)
	{
		for(unsigned int x = 1;x < width - 1;x++)
		{
			const unsigned int inputIndex = (y * width + x) * 2;
			const unsigned int outputIndex = (y * width + x) * 3;

			const float magnitude = gradient[inputIndex + 0];

			//Discretize angle into one of four fixed steps to indicate which direction the edge is
			//running along: horizontal, vertical, left-to-right diagonal, or right-to-left
			//diagonal. The edge direction is 90 degrees from the gradient angle.
			float angle = gradient[inputIndex + 1];

			//The input angle is in the range of [-pi,pi] but negative angles represent the same
			//edge direction as angles 180 degrees apart.
			angle = angle >= 0.0f ? angle : angle + M_PI;

			//Scale from [0,pi] to [0,4] and round to an integer representing a direction. Each
			//direction is made up of 45 degree blocks. The rounding and modulus handle the
			//situation where the first and final 45/2 degrees are both part of the same direction.
			angle = angle * 4.0f / M_PI;
			const unsigned char direction = lroundf(angle) % 4;

			//Only mark pixels as edges when the gradients of the pixels immediately on either side
			//of the edge have smaller magnitudes. This keeps the edges thin.
			bool suppress = false;
			if(direction == 0) //Vertical edge.
				suppress = magnitude < gradient[inputIndex - 2] ||
						   magnitude < gradient[inputIndex + 2];
			else if(direction == 1) //Right-to-left diagonal edge.
				suppress = magnitude < gradient[inputIndex - width * 2 - 2] ||
						   magnitude < gradient[inputIndex + width * 2 + 2];
			else if(direction == 2) //Horizontal edge.
				suppress = magnitude < gradient[inputIndex - width * 2] ||
						   magnitude < gradient[inputIndex + width * 2];
			else if(direction == 3) //Left-to-right diagonal edge.
				suppress = magnitude < gradient[inputIndex - width * 2 + 2] ||
						   magnitude < gradient[inputIndex + width * 2 - 2];

			if(suppress || magnitude < lowThreshold)
			{
				nonMaximumSuppression[outputIndex + 0] = 0;
				nonMaximumSuppression[outputIndex + 1] = 0;
				nonMaximumSuppression[outputIndex + 2] = 0;
			}
			else
			{
				//Use thresholding to indicate strong and weak edges. Strong edges are assumed to be
				//valid edges. Connectivity analysis is used to check if a weak edge is connected to
				//a strong edge indiciating that the weak edge is also a valid edge.
				nonMaximumSuppression[outputIndex + 0] = magnitude >= highThreshold ? 255 : 0; //Strong
				nonMaximumSuppression[outputIndex + 1] = magnitude < highThreshold ? 255 : 0; //Weak
				nonMaximumSuppression[outputIndex + 2] = 0;
			}
		}
	}
}

static void ConnectivityAnalysis(Image& image)
{
	assert(image.width >= 1 && image.height >= 1);

	//Input image should be output of NonMaximumSuppression().
	//Channel 0: Strong edge pixels.
	//Channel 1: Weak edge pixels.
	//Channel 2: Should be set to 0. Will be used to indicate an edge that has been previously
	//           visited.
	//Output image will have all weak edges connected to strong edges set to strong edges
	//themselves. Meaning only the first channel will have useful data and the other remaining
	//channels should be ignored.

	//Keep track of coordinates that should be searched in case they are connected.
	std::stack<std::pair<unsigned int,unsigned int>> searchStack;

	//Add all 8 coordinates around (x,y) to be searched.
	auto PushSearchConnected = [&searchStack](const unsigned int x,const unsigned int y) {
		searchStack.push(std::make_pair(x - 1,y - 1));
		searchStack.push(std::make_pair(x    ,y - 1));
		searchStack.push(std::make_pair(x + 1,y - 1));
		searchStack.push(std::make_pair(x - 1,y));
		searchStack.push(std::make_pair(x    ,y));
		searchStack.push(std::make_pair(x + 1,y));
		searchStack.push(std::make_pair(x - 1,y + 1));
		searchStack.push(std::make_pair(x    ,y + 1));
		searchStack.push(std::make_pair(x + 1,y + 1));
	};

	//Search for strong edges and flood fill surrounding weak edges, promoting the weak to strong.
	for(unsigned int y = 1;y < image.height - 1;y++)
	{
		for(unsigned int x = 1;x < image.width - 1;x++)
		{
			const unsigned int index = (y * image.width + x) * 3;

			//Skip pixels that are not strong edges or have been previously visited.
			if(image.data[index + 0] == 0 || image.data[index + 2] == 255)
				continue;

			//Mark pixel as visited to save time flood filling later.
			image.data[index + 2] = 255;

			//Flood fill all connected weak edges.
			PushSearchConnected(x,y);
			while(!searchStack.empty())
			{
				const std::pair<unsigned int,unsigned int> coordinates = searchStack.top();
				searchStack.pop();

				//Skip pixels that are not weak edges.
				const unsigned int x = coordinates.first;
				const unsigned int y = coordinates.second;
				const unsigned int index = (y * image.width + x) * 3;
				if(image.data[index + 1] == 0)
					continue;

				//Promote to strong edge and mark visited to save time flood filling later.
				image.data[index + 0] = 255;
				image.data[index + 1] = 0;
				image.data[index + 2] = 255;

				//Search around this coordinate as well. This will waste time checking the previous
				//coordinate again but it's fast enough.
				PushSearchConnected(x,y);
			}
		}
	}
}

void YUYVToRGB(const unsigned char* yuyvData,Image& frame)
{
	for(unsigned int x = 0;x < frame.width * frame.height;x+=2)
	{
		const unsigned int inputIndex = x * 2;
		const unsigned int outputIndex = x * 3;

		const unsigned char y0 = yuyvData[inputIndex + 0];
		const unsigned char cb = yuyvData[inputIndex + 1];
		const unsigned char y1 = yuyvData[inputIndex + 2];
		const unsigned char cr = yuyvData[inputIndex + 3];

		//TODO: LOTS of optimization possibilities here.

		frame.data[outputIndex + 0] = Clamp(static_cast<int>(y0 + 1.402 * (cr - 128)),0,255);
		frame.data[outputIndex + 1] = Clamp(static_cast<int>(y0 - 0.344 * (cb - 128) - 0.714 * (cr - 128)),0,255);
		frame.data[outputIndex + 2] = Clamp(static_cast<int>(y0 + 1.772 * (cb - 128)),0,255);

		frame.data[outputIndex + 3] = Clamp(static_cast<int>(y1 + 1.402 * (cr - 128)),0,255);
		frame.data[outputIndex + 4] = Clamp(static_cast<int>(y1 - 0.344 * (cb - 128) - 0.714 * (cr - 128)),0,255);
		frame.data[outputIndex + 5] = Clamp(static_cast<int>(y1 + 1.772 * (cb - 128)),0,255);
	}
}

void YUYVToGreyscale(const unsigned char* yuyvData,Image& frame)
{
	for(unsigned int x = 0;x < frame.width * frame.height;x+=2)
	{
		const unsigned int inputIndex = x * 2;
		const unsigned int outputIndex = x * 3;

		const unsigned char y0 = yuyvData[inputIndex + 0];
		const unsigned char y1 = yuyvData[inputIndex + 2];

		frame.data[outputIndex + 0] = y0;
		frame.data[outputIndex + 1] = y0;
		frame.data[outputIndex + 2] = y0;

		frame.data[outputIndex + 3] = y1;
		frame.data[outputIndex + 4] = y1;
		frame.data[outputIndex + 5] = y1;
	}
}

void NV12ToRGB(const unsigned char* nv12Data,Image& frame)
{
	const unsigned int widthHalf = frame.width / 2;

	for(unsigned int y = 0;y < frame.height;y++)
	{
		const unsigned int yEven = y & 0xFFFFFFFE;
		for(unsigned int x = 0;x < frame.width;x++)
		{
			const unsigned int xEven = x & 0xFFFFFFFE;

			const unsigned int yIndex = y * frame.width + x;
			const unsigned int cIndex = frame.width * frame.height + yEven * widthHalf + xEven;
			const unsigned int outputIndex = (y * frame.width + x) * 3;

			const unsigned char y = nv12Data[yIndex];
			const unsigned char cb = nv12Data[cIndex + 0];
			const unsigned char cr = nv12Data[cIndex + 1];

			//TODO: LOTS of optimization possibilities here.

			frame.data[outputIndex + 0] = Clamp(static_cast<int>(y + 1.402 * (cr - 128)),0,255);
			frame.data[outputIndex + 1] = Clamp(static_cast<int>(y - 0.344 * (cb - 128) - 0.714 * (cr - 128)),0,255);
			frame.data[outputIndex + 2] = Clamp(static_cast<int>(y + 1.772 * (cb - 128)),0,255);
		}
	}
}

void NV12ToGreyscale(const unsigned char* nv12Data,Image& frame)
{
	for(unsigned int x = 0;x < frame.width * frame.height;x++)
	{
		const unsigned int outputIndex = x * 3;
		const unsigned char y = nv12Data[x];

		frame.data[outputIndex + 0] = y;
		frame.data[outputIndex + 1] = y;
		frame.data[outputIndex + 2] = y;
	}
}

void RGBToRGB(const unsigned char* rgbData,Image& frame)
{
	memcpy(&frame.data[0],rgbData,frame.width * frame.height * 3);
}

void RGBToGreyscale(const unsigned char* rgbData,Image& frame)
{
	for(unsigned int x = 0;x < frame.width * frame.height;x++)
	{
		const unsigned int index = x * 3;

		//RGB to luma (BT.601 Y'UV).
		const float lumaf = 0.299f * rgbData[index + 0] +
							0.587f * rgbData[index + 1] +
							0.114f * rgbData[index + 2];
		const unsigned char luma = ClampToU8(lumaf);

		frame.data[index + 0] = luma;
		frame.data[index + 1] = luma;
		frame.data[index + 2] = luma;
	}
}

void BGRVerticalMirroredToRGB(const unsigned char* bgrData,Image& frame)
{
	for(unsigned int y = 0;y < frame.height;y++)
	{
		for(unsigned int x = 0;x < frame.width;x++)
		{
			const unsigned int inputIndex = ((frame.height - y - 1) * frame.width + x) * 3;
			const unsigned int outputIndex = (y * frame.width + x) * 3;

			frame.data[outputIndex + 0] = bgrData[inputIndex + 2];
			frame.data[outputIndex + 1] = bgrData[inputIndex + 1];
			frame.data[outputIndex + 2] = bgrData[inputIndex + 0];
		}
	}
}

void BlendAdd(const Image& image1,const Image& image2,Image& outputImage)
{
	assert(image1.width == image2.width && image1.height == image2.height);
	
	outputImage.MatchSize(image1);

	for(unsigned int x = 0;x < image1.width * image1.height * 3;x++)
	{
		outputImage.data[x] = ClampToU8(static_cast<unsigned int>(image1.data[x]) + static_cast<unsigned int>(image2.data[x]));
	}
}

void AutoLevels(const Image& inputImage,Image& outputImage,const unsigned int ignorePadding)
{
	if(inputImage.width < ignorePadding * 2 || inputImage.height < ignorePadding * 2)
		return;

	outputImage.MatchSize(inputImage);

	//Find the lows and highs of the histogram.
	unsigned char minValue = 255;
	unsigned char maxValue = 0;
	for(unsigned int y = ignorePadding;y < inputImage.height - ignorePadding;y++)
	{
		for(unsigned int x = ignorePadding;x < inputImage.width - ignorePadding;x++)
		{
			const unsigned int index = (y * inputImage.width + x) * 3;
			const unsigned char value = inputImage.data[index];

			minValue = std::min(minValue,value);
			maxValue = std::max(maxValue,value);
		}
	}

	//Rescale so the brightest and darkest parts are clipped by CLIPPING percent.
	constexpr float CLIPPING = 0.1;
	const float delta = static_cast<float>(maxValue - minValue) / 255.0f - (CLIPPING * 2.0f);
	if(delta <= 0.0f)
		return;
	for(unsigned int x = 0;x < inputImage.width * inputImage.height;x++)
	{
		const unsigned int index = x * 3;

		const float valuef = (static_cast<float>(inputImage.data[index]) - static_cast<float>(minValue)) / delta;
		const unsigned char value = ClampToU8(valuef);

		outputImage.data[index + 0] = value;
		outputImage.data[index + 1] = value;
		outputImage.data[index + 2] = value;
	}
}

void Gaussian(const Image& inputImage,Image& outputImage,const float radius)
{
	outputImage.MatchSize(inputImage);

	auto GaussianKernel = [](const float radius,unsigned int& weightRadius,unsigned int& weightCount) {
		auto Gaussian = [](const float x,const float sigma) {
			const float x2 = x * x;
			const float sigma2 = sigma * sigma;
			return expf(-x2 / (2.0f * sigma2));
		};

		const float sigma = radius / 3.0f; //Somewhat arbitrary but dependent on radius.
		weightRadius = static_cast<unsigned int>(radius) + 1;
		weightCount = weightRadius * 2 + 1;

		std::vector<float> weights(weightCount,0.0f);
		float sum = 0.0f;
		for(unsigned int x = 0;x < weightCount;x++)
		{
			const float weight = Gaussian(static_cast<float>(x) - static_cast<float>(weightRadius),sigma);
			weights[x] = weight;
			sum += weight;
		}

		const float oneOverSum = 1.0f / sum;
		for(float& weight : weights)
		{
			weight *= oneOverSum;
		}

		return weights;
	};

	unsigned int weightRadius = 0;
	unsigned int weightCount = 0;
	const std::vector<float> weights = GaussianKernel(radius,weightRadius,weightCount);

	//TODO: Make caller provide a temporary buffer to reduce large allocations.
	std::vector<unsigned char> tempBuffer;
	tempBuffer.resize(outputImage.data.size());

	//Blur horizontally.
	//TODO: Support edges or document the current behavior.
	for(unsigned int y = 0;y < inputImage.height;y++)
	{
		for(unsigned int x = weightRadius;x < inputImage.width - weightRadius;x++)
		{
			float sum[3] = {0.0f,0.0f,0.0f};
			for(unsigned int w = 0;w < weightCount;w++)
			{
				const unsigned int inputIndex = (y * inputImage.width + x + w - weightRadius) * 3;
				sum[0] += static_cast<float>(inputImage.data[inputIndex + 0]) * weights[w];
				sum[1] += static_cast<float>(inputImage.data[inputIndex + 1]) * weights[w];
				sum[2] += static_cast<float>(inputImage.data[inputIndex + 2]) * weights[w];
			}

			const unsigned int outputIndex = (y * outputImage.width + x) * 3;
			tempBuffer[outputIndex + 0] = ClampToU8(sum[0]);
			tempBuffer[outputIndex + 1] = ClampToU8(sum[1]);
			tempBuffer[outputIndex + 2] = ClampToU8(sum[2]);
		}
	}

	//Blur vertically.
	//TODO: Might be faster to rotate 90 degrees, blur horizontally, and then rotate 90 degrees back.
	for(unsigned int y = weightRadius;y < inputImage.height - weightRadius;y++)
	{
		for(unsigned int x = weightRadius;x < inputImage.width - weightRadius;x++)
		{
			float sum[3] = {0.0f,0.0f,0.0f};
			for(unsigned int w = 0;w < weightCount;w++)
			{
				const unsigned int inputIndex = ((y + w - weightRadius) * inputImage.width + x) * 3;
				sum[0] += static_cast<float>(tempBuffer[inputIndex + 0]) * weights[w];
				sum[1] += static_cast<float>(tempBuffer[inputIndex + 1]) * weights[w];
				sum[2] += static_cast<float>(tempBuffer[inputIndex + 2]) * weights[w];
			}

			const unsigned int outputIndex = (y * outputImage.width + x) * 3;
			outputImage.data[outputIndex + 0] = ClampToU8(sum[0]);
			outputImage.data[outputIndex + 1] = ClampToU8(sum[1]);
			outputImage.data[outputIndex + 2] = ClampToU8(sum[2]);
		}
	}
}

void Sobel(const Image& image,std::vector<float>& gradient)
{
	gradient.resize(image.width * image.height * 2);

	if(image.width == 0 || image.height == 0)
		return;

	const unsigned int rowSpan = image.width * 3;
	for(unsigned int y = 1;y < image.height - 1;y++)
	{
		for(unsigned int x = 1;x < image.width - 1;x++)
		{
			const unsigned int inputIndex = (y * image.width + x) * 3;
			const float horizontalSum = static_cast<float>(image.data[inputIndex - rowSpan - 3]) * -1.0f + static_cast<float>(image.data[inputIndex - rowSpan + 3]) +
										static_cast<float>(image.data[inputIndex - 3])           * -2.0f + static_cast<float>(image.data[inputIndex + 3])       * 2.0f +
										static_cast<float>(image.data[inputIndex + rowSpan - 3]) * -1.0f + static_cast<float>(image.data[inputIndex + rowSpan + 3]);
			const float verticalSum = static_cast<float>(image.data[inputIndex - rowSpan - 3]) * -1.0f + static_cast<float>(image.data[inputIndex - rowSpan]) * -2.0f + static_cast<float>(image.data[inputIndex - rowSpan + 3]) * -1.0f +
									  static_cast<float>(image.data[inputIndex + rowSpan - 3])         + static_cast<float>(image.data[inputIndex + rowSpan]) *  2.0f + static_cast<float>(image.data[inputIndex + rowSpan + 3]);

			const float magnitude = hypotf(horizontalSum,verticalSum);
			const float angle = atan2f(verticalSum,horizontalSum);

			const unsigned int outputIndex = (y * image.width + x) * 2;
			gradient[outputIndex + 0] = magnitude;
			gradient[outputIndex + 1] = angle;
		}
	}
}

void HoughTransform(const Image& inputImage,Image& accumulationImage)
{
    //Based on Digital Image Processing Third Edition. Chapter 10.2.7. Page 733.
	//AccumulationImage is where the buckets for the hough transform are written to.
	//X Axis: Angle evenly split up across [-pi/2,pi).
	//Y Axis: Distance from origin split up across [0,diagonal length).
	//Each pixel is the accumulation of the related input pixel's chance of being part of the line.
	//The red channel and green channel are a machine native 16-bit unsigned integer total. The
	//blue channel is unused.
	//The X axis interval was chosen so that rho can represent all lines with a positive value and
	//so we don't have to worry about angles being wrapped.

	if(accumulationImage.width == 0 || accumulationImage.height == 0)
	{
		//Sane defaults based off of Image Processing: The Fundamentals Chapter 5. Page 520.
		accumulationImage.width = 360 * 2;
		accumulationImage.height = std::min(inputImage.width,inputImage.height) * 2;
	}
	accumulationImage.data.resize(accumulationImage.width * accumulationImage.height * 3);
	std::fill(accumulationImage.data.begin(),accumulationImage.data.end(),0);

	//Pre-calculate as much as possible to improve performance.
	const float maxR = hypotf(inputImage.width,inputImage.height);
	const float angleFMultiplier = (3.0f * M_PI / 2.0f) / static_cast<float>(accumulationImage.width);
	const float rMultiplier = static_cast<float>(accumulationImage.height) / maxR;

	std::vector<float> cosAngles(accumulationImage.width);
	std::vector<float> sinAngles(accumulationImage.width);
	for(unsigned int x = 0;x < accumulationImage.width;x++)
	{
		const float angleF = static_cast<float>(x) * angleFMultiplier - M_PI / 2.0f;
		cosAngles[x] = cosf(angleF);
		sinAngles[x] = sinf(angleF);
	}

	constexpr unsigned int IGNORE_PADDING = 10; //How much of edges to ignore so blurred edges are not counted as an edge.
	for(unsigned int y = IGNORE_PADDING;y < inputImage.height - IGNORE_PADDING;y++)
	{
		for(unsigned int x = IGNORE_PADDING;x < inputImage.width - IGNORE_PADDING;x++)
		{
			const unsigned int inputIndex = (y * inputImage.width + x) * 3;
			if(inputImage.data[inputIndex + 0] == 0)
				continue;

			for(unsigned int z = 0;z < accumulationImage.width;z++)
			{
				float rf = static_cast<float>(x) * cosAngles[z] + static_cast<float>(y) * sinAngles[z];
				if(rf < 0.0f)
					continue;
				rf *= rMultiplier;
				const unsigned int r = Clamp(static_cast<unsigned int>(rf),0u,accumulationImage.height - 1);

				const unsigned int outputIndex = (r * accumulationImage.width + z) * 3;
				unsigned short* value = reinterpret_cast<unsigned short*>(&accumulationImage.data[outputIndex]);
				if(*value < 0xFFFF)
					*value += 1;
			}
		}
	}
}

Canny Canny::WithRadius(const float gaussianBlurRadius)
{
	return Canny(gaussianBlurRadius);
}

void Canny::Process(const Image& inputImage,Image& outputImage)
{
	Gaussian(inputImage,gaussianImage,gaussianBlurRadius);
	Sobel(gaussianImage,gradient);

	Histogram(gaussianImage,normalizedHistogram);
	const float highThreshold = OtsusMethod(normalizedHistogram);
	const float lowThreshold = highThreshold / 2;
	outputImage.MatchSize(inputImage);
	NonMaximumSuppression(gradient,inputImage.width,inputImage.height,outputImage.data,lowThreshold,highThreshold);

	ConnectivityAnalysis(outputImage);
}

Canny::Canny(const float gaussianBlurRadius)
	: gaussianBlurRadius(gaussianBlurRadius)
{
}

