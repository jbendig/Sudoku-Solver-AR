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

	std::vector<unsigned char> roundedGradient(gradient.size(),0);
	for(unsigned int y = 1;y < height - 1;y++)
	{
		for(unsigned int x = 1;x < width - 1;x++)
		{
			const unsigned int index = (y * width + x) * 2;

			//Extract magnitude directly. In theory, the range is
			//[0.0f,sqrtf(255 * 255 + 255 * 255)] and should be mapped to [0,255]. However, not
			//doing helps find stronger lines.
			const unsigned char magnitude = ClampToU8(gradient[index + 0]);

			//Simplify direction from radians to vertical, horizontal, left diagonal, or right diagonal.
			float directionf = gradient[index + 1];
			directionf = directionf >= 0.0f ? directionf : directionf + M_PI;
			directionf = directionf * 4.0f / M_PI;
			const unsigned char direction = lroundf(directionf) % 4;

			roundedGradient[index + 0] = magnitude;
			roundedGradient[index + 1] = direction;
		}
	}

	nonMaximumSuppression.resize(width * height * 3);
	std::fill(nonMaximumSuppression.begin(),nonMaximumSuppression.end(),0);
	for(unsigned int y = 1;y < height - 1;y++)
	{
		for(unsigned int x = 1;x < width - 1;x++)
		{
			const unsigned int inputIndex = (y * width + x) * 2;
			const unsigned int outputIndex = (y * width + x) * 3;

			const unsigned char magnitude = roundedGradient[inputIndex + 0];
			const unsigned char direction = roundedGradient[inputIndex + 1];

			bool suppress = false;
			if(direction == 0)
				suppress = magnitude < roundedGradient[inputIndex - 2] ||
						   magnitude < roundedGradient[inputIndex + 2]; //Vertical line.
			else if(direction == 1)
				suppress = magnitude < roundedGradient[inputIndex - width * 2 - 2] ||
						   magnitude < roundedGradient[inputIndex + width * 2 + 2];
			else if(direction == 2)
				suppress = magnitude < roundedGradient[inputIndex - width * 2] ||
						   magnitude < roundedGradient[inputIndex + width * 2]; //Horizontal line.
			else if(direction == 3)
				suppress = magnitude < roundedGradient[inputIndex - width * 2 + 2] ||
						   magnitude < roundedGradient[inputIndex + width * 2 - 2];

			if(suppress || magnitude < lowThreshold)
			{
				nonMaximumSuppression[outputIndex + 0] = 0;
				nonMaximumSuppression[outputIndex + 1] = 0;
				nonMaximumSuppression[outputIndex + 2] = 0;
			}
			else
			{
				nonMaximumSuppression[outputIndex + 0] = magnitude >= highThreshold ? 255 : 0;
				nonMaximumSuppression[outputIndex + 1] = magnitude < highThreshold ? magnitude : 0;
				nonMaximumSuppression[outputIndex + 2] = 0;
			}
		}
	}

	//Perform hysterasis thresholding.
	//Channel 0: Strong edge pixels.
	//Channel 1: Weak edge pixels.
	//Channel 2: Pixel has been visited previously (strong edge only).
	//TODO: This is probably slower than a flood-fill-like search.
	bool found = false;
	while(!found)
	{
		found = false;

		for(unsigned int y = 1;y < height - 1;y++)
		{
			for(unsigned int x = 1;x < width - 1;x++)
			{
				const unsigned int index = (y * width + x) * 3;

				//Skip pixels that are not strong edges or have been previously visited.
				if(nonMaximumSuppression[index + 0] == 0 || nonMaximumSuppression[index + 2] == 255)
					continue;

				nonMaximumSuppression[index + 2] = 255;
				found = true;

				auto TryWeakEdge = [&nonMaximumSuppression,width](const unsigned int x,const unsigned int y)
				{
					const unsigned int index = (y * width + x) * 3;
					if(nonMaximumSuppression[index + 1] == 0)
						return;

					//Promote to strong edge.
					nonMaximumSuppression[index + 0] = 255;
					nonMaximumSuppression[index + 1] = 0;
				};
				TryWeakEdge(x - 1,y - 1);
                TryWeakEdge(x    ,y - 1);
                TryWeakEdge(x + 1,y - 1);
                TryWeakEdge(x - 1,y);
                TryWeakEdge(x + 1,y);
                TryWeakEdge(x - 1,y + 1);
                TryWeakEdge(x    ,y + 1);
                TryWeakEdge(x + 1,y + 1);
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
		auto GaussianCook = [](const float x,const float radius,const float sigma) {
			//From Real-Time Rendering page 470.
			if(x > radius)
				return 0.0f;

			const float x2 = x * x;
			const float radius2 = radius * radius;
			const float sigma2 = sigma * sigma;
			const float sigma2Times2 = sigma2 * 2.0f;
			return expf(-(x2 / sigma2Times2)) - expf(-(radius2 / sigma2Times2));
		};

		const float sigma = radius / 3.0f; //Somewhat arbitrary but dependent on radius.
		weightRadius = static_cast<unsigned int>(radius) + 1;
		weightCount = weightRadius * 2 + 1;

		std::vector<float> weights(weightCount,0.0f);
		float sum = 0.0f;
		for(unsigned int x = 0;x < weightCount;x++)
		{
			const float weight = GaussianCook(static_cast<float>(x) - static_cast<float>(weightRadius),radius,sigma);
			weights[x] = weight;
			sum += weight;
		}

		const float oneOverSum = 1.0f / sum;
		for(float& weight : weights)
		{
			weight = std::max(0.0f,weight * oneOverSum);
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
	for(unsigned int y = weightRadius;y < inputImage.height - weightRadius;y++)
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

void LineThinning(const Image& inputImage,Image& outputImage)
{
    //Based on Digital Image Processing Third Edition. Chapter 9.5.5. Page 649.
    //Only one pass is performed which is all that is required for Canny. Normally, this is run
    //repeatedly until it converges.

	std::vector<std::vector<unsigned char>> masks = {
        {0,0,0,2,1,2,1,1,1},
        {2,0,0,1,1,0,1,1,2},
        {1,2,0,1,1,0,1,2,0},
        {1,1,2,1,1,0,2,0,0},
        {1,1,1,2,1,2,0,0,0},
        {2,1,1,0,1,1,0,0,2},
        {0,2,1,0,1,1,0,2,1},
        {0,0,2,0,1,1,2,1,1},
    };

	outputImage.MatchSize(inputImage);
	std::copy(inputImage.data.begin(),inputImage.data.end(),outputImage.data.begin());

	for(auto mask : masks)
	{
		for(unsigned int y = 1;y < inputImage.height - 1;y++)
		{
			for(unsigned int x = 1;x < inputImage.width - 1;x++)
			{
				const unsigned int index = (y * inputImage.width + x) * 3;
                if(inputImage.data[index + 0] != 255)
                    continue;

				auto MatchMask = [&mask,&inputImage](const unsigned int maskIndex,const unsigned int x,const unsigned int y)
				{
					const unsigned char value = inputImage.data[(y * inputImage.width + x) * 3];
					if(mask[maskIndex] == 0)
						return value == 0;
					else if(mask[maskIndex] == 1)
						return value == 255;
					else
						return true;
				};

                if(MatchMask(0,x - 1,y - 1) && MatchMask(1,x,y - 1) && MatchMask(2,x + 1,y - 1) &&
				   MatchMask(3,x - 1,y    ) && MatchMask(4,x,y    ) && MatchMask(5,x + 1,y    ) &&
				   MatchMask(6,x - 1,y + 1) && MatchMask(7,x,y + 1) && MatchMask(8,x + 1,y + 1))
				{
                    outputImage.data[index + 0] = 0;
                    outputImage.data[index + 1] = 0;
                    outputImage.data[index + 2] = 0;
                }
            }
        }
    }
}

void HoughTransform(const Image& inputImage,Image& accumulationImage)
{
    //Based on Digital Image Processing Third Edition. Chapter 10.2.7. Page 733.
	//AccumulationImage is where the buckets for the hough transform are written to.
	//X Axis: Angle evenly split up across [0,pi).
	//Y Axis: Distance from origin split up across [0,diagonal length).
	//Each pixel is the accumulation of the related input pixel's chance of being part of the line.
	//The red channel and green channel are a machine native 16-bit unsigned integer total. The
	//blue channel is unused.

	if(accumulationImage.width == 0 || accumulationImage.height == 0)
	{
		//Sane defaults based off of Image Processing: The Fundamentals Chapter 5. Page 520.
		accumulationImage.width = 360;
		accumulationImage.height = std::min(inputImage.width,inputImage.height);
	}
	accumulationImage.data.resize(accumulationImage.width * accumulationImage.height * 3);
	std::fill(accumulationImage.data.begin(),accumulationImage.data.end(),0);

	//Pre-calculate as much as possible to improve performance.
	const float maxR = hypotf(inputImage.width,inputImage.height);
	const float angleFMultiplier = M_PI / static_cast<float>(accumulationImage.width);
	const float rMultiplier = static_cast<float>(accumulationImage.height) / 2.0f;
	const float rTerm1Multiplier = rMultiplier / maxR;

	std::vector<float> cosAngles(accumulationImage.width);
	std::vector<float> sinAngles(accumulationImage.width);
	for(unsigned int x = 0;x < accumulationImage.width;x++)
	{
		const float angleF = static_cast<float>(x) * angleFMultiplier;
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
				rf = rf * rTerm1Multiplier + rMultiplier;
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
	AutoLevels(gaussianImage,autoLevelsImage,static_cast<unsigned int>(gaussianBlurRadius) + 1);
	Sobel(autoLevelsImage,gradient);

	Histogram(autoLevelsImage,normalizedHistogram);
	const float highThreshold = OtsusMethod(normalizedHistogram);
	const float lowThreshold = highThreshold / 2;
	NonMaximumSuppression(gradient,inputImage.width,inputImage.height,nonMaximumSuppression.data,lowThreshold,highThreshold);

	nonMaximumSuppression.width = inputImage.width;
	nonMaximumSuppression.height = inputImage.height;
	LineThinning(nonMaximumSuppression,outputImage);
}

Canny::Canny(const float gaussianBlurRadius)
	: gaussianBlurRadius(gaussianBlurRadius)
{
}

