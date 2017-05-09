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

#include <algorithm>
#include <iostream>
#include <cassert>
#include <cmath>
#ifdef __linux
#include <GLES3/gl3.h>
#elif defined _WIN32
#include <GL/glew.h>
#endif
#include <GLFW/glfw3.h>
#include "Camera.h"
#include "Image.h"
#include "ImageProcessing.h"
#include "Painter.h"

void CheckGLError()
{
	const GLenum error = glGetError();
	if(error == GL_NO_ERROR)
		return;

	std::cout << "OpenGL Error: " << error << std::endl;
	std::abort();
}

void FindLines(const Image& houghTransformInputFrame,const Image& houghTransformFrame,std::vector<std::pair<float,float>>& lines)
{
	//Find peaks using a sliding window. A peak exists when all surrounding pixels within some
	//radius are lower than the center pixel.
	//TODO: Handle the situation where two equal peaks are right next to each other.

	constexpr int RADIUS = 5;
	constexpr unsigned short MINIMUM_VALUE = 200;
	auto ExtractValue = [&houghTransformFrame](const unsigned int x,const unsigned int y) -> unsigned short
	{
		if(x >= houghTransformFrame.width || y >= houghTransformFrame.height)
			return 0;

		const unsigned int index = (y * houghTransformFrame.width + x) * 3;
		return *reinterpret_cast<const unsigned short*>(&houghTransformFrame.data[index]);
	};

	lines.clear();

	for(unsigned int y = 0;y < houghTransformFrame.height;y++)
	{
		for(unsigned int x = 0;x < houghTransformFrame.width;x++)
		{
			const unsigned short value = ExtractValue(x,y);
			if(value < MINIMUM_VALUE)
				continue;

			bool peak = true;
			for(int ny = -RADIUS;ny < RADIUS + 1 && peak;ny++)
			{
				for(int nx = -RADIUS;nx < RADIUS + 1;nx++)
				{
					if(nx == 0 && ny == 0)
						continue;

					if(value < ExtractValue(static_cast<unsigned int>(static_cast<int>(x) + nx),
											static_cast<unsigned int>(static_cast<int>(y) + ny)))
					{
						peak = false;
						break;
					}
				}
			}

			if(peak)
			{
				//Convert peak into Hesse normal form theta and rho.
				//TODO: Maybe simplify how rho is stored...
				float theta = static_cast<float>(x) / static_cast<float>(houghTransformFrame.width) * M_PI;
				const float rMultiplier = static_cast<float>(houghTransformFrame.height) / 2.0f;
				const float maxR = hypotf(houghTransformInputFrame.width,houghTransformInputFrame.height);
				float rho = (static_cast<float>(y) - rMultiplier) * maxR / rMultiplier;

				//Make sure rho is always in a positive form to simplify working with the lines.
				if(rho < 0.0f)
				{
					theta = fmodf(theta + M_PI,2*M_PI);
					rho *= -1.0f;
				}

				lines.push_back({theta,rho});
			}
		}
	}
}

void DrawLines(Painter& painter,const float x,const float y,const float width,const float height,const std::vector<std::pair<float,float>>& lines)
{
	for(auto line : lines)
	{
		//Based on the equation x*cos(theta) + y*sin(theta) = rho.
		const float theta = line.first;
		const float rho = line.second;

		//Get a point on the line. The actual line is 90 degrees from theta at this point.
		const float cosTheta = cosf(theta);
		const float sinTheta = sinf(theta);
		const float xPoint = cosTheta * rho;
		const float yPoint = sinTheta * rho;

		//Vertical line. Draw and get out early to avoid divide by zeroes below.
		if(sinTheta == 0.0f)
		{
			painter.DrawLine(x + xPoint,
							 y,
							 x + xPoint,
							 y + height,
							 0,255,0);
			continue;
		}

		//Now to get the line equation: y = mx + b.
		//Let x1 = xPoint
		//    y1 = yPoint
		//    x2 = xPoint + cos(theta + PI/2)
		//    y2 = yPoint + sin(theta + PI/2).
		//Then m = (y2 - y1) / (x2 - x1) = sin(theta + PI/2) / cos(theta + Pi/2)
		//                               = tan(theta + PI/2)
		//                               = -(cosf(theta) / sinf(theta)).
		const float m = -(cosf(theta) / sinf(theta));
		const float b = -xPoint * m;


		//Spots where the line intersects the image.
		const float leftVertical = yPoint + b;
		const float topHorizontal = (-yPoint - b) / m;
		const float rightVertical = yPoint + b + width * m;
		const float bottomHorizontal = (height - yPoint - b) / m;

		//Clip line so that it fits in image.
		float x1 = 0.0f;
		float y1 = 0.0f;
		float x2 = 0.0f;
		float y2 = 0.0f;

		if(theta > 0.0f && theta <= M_PI/2) //Point is in the lower right quadrant.
		{
			x1 = leftVertical <= height ? 0.0f : bottomHorizontal;
			y1 = leftVertical <= height ? leftVertical : height;
			x2 = topHorizontal <= width ? topHorizontal : width;
			y2 = topHorizontal <= width ? 0.0f : rightVertical;
		}
		else if(theta >= M_PI/2 && theta <= M_PI) //Point is in the lower left quadrant.
		{
			if(leftVertical > height)
				continue; //Clip line, not within image.

			x1 = 0.0f;
			y1 = leftVertical;
			x2 = bottomHorizontal <= width ? bottomHorizontal : width;
			y2 = rightVertical <= height ? rightVertical : height;

		}
		else if(theta >= 3*M_PI/2) //Point is in the top right quadrant.
		{
			if(topHorizontal > width)
				continue; //Clip line, not within image.

			x1 = topHorizontal;
			y1 = 0.0f;
			x2 = bottomHorizontal <= width ? bottomHorizontal : width;
			y2 = bottomHorizontal <= width ? height : rightVertical;
		}
		//Line in the top left quadrant is never in the image.
		else
			continue;

		painter.DrawLine(x + x1,
						 y + y1,
						 x + x2,
						 y + y2,
						 0,255,255);
	}
}

void DrawHoughTransform(Painter& painter,const float windowWidth,const float windowHeight,const Image& houghTransformFrame,const float scale)
{
	//Find maximum hough transform value.
	unsigned short maximumValue = 0;
	for(unsigned int x = 0;x < houghTransformFrame.width * houghTransformFrame.height;x++)
	{
		const unsigned int index = x * 3;
		const unsigned short value = *reinterpret_cast<const unsigned short*>(&houghTransformFrame.data[index]);
		maximumValue = std::max(maximumValue,value);
	}

	Image modifiedHTF = houghTransformFrame;

	//Rescale hough transform into a 0-255 greyscale image so it can be displayed.
	const float multiplier = 255.0f / static_cast<float>(maximumValue);
	for(unsigned int x = 0;x < houghTransformFrame.width * houghTransformFrame.height;x++)
	{
		const unsigned int index = x * 3;
		const unsigned char value = static_cast<float>(*reinterpret_cast<const unsigned short*>(&houghTransformFrame.data[index])) * multiplier;

		modifiedHTF.data[index + 0] = value;
		modifiedHTF.data[index + 1] = value;
		modifiedHTF.data[index + 2] = value;
	}

	//Draw hough transform in the lower right corner of window.
	painter.DrawImage(windowWidth - houghTransformFrame.width * scale,
					  windowHeight - houghTransformFrame.height * scale,
					  houghTransformFrame.width * scale,
					  houghTransformFrame.height * scale,
					  modifiedHTF);
}

#ifdef __linux
int main(int argc,char* argv[])
#elif defined _WIN32
int __stdcall WinMain(void*,void*,void*,int)
#endif
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API,GLFW_OPENGL_ES_API);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,0);
	glfwWindowHint(GLFW_RESIZABLE,GL_FALSE);

	GLFWwindow* window = glfwCreateWindow(800,600,"Sudoku Solver AR",nullptr,nullptr);
	assert(window != nullptr);
	glfwMakeContextCurrent(window);

#ifdef _WIN32
	glewInit();
#endif

	int width = 0;
	int height = 0;
	glfwGetFramebufferSize(window,&width,&height);
	glViewport(0,0,width,height);
	CheckGLError();

	Painter painter(width,height);
	Camera camera = Camera::Open("/dev/video0").value();
	Image frame;
	Image greyscaleFrame;
	Image cannyFrame;
	Canny canny = Canny::WithRadius(5.0f);
	Image mergedFrame;
	Image houghTransformFrame;

	while(!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
		//TODO: Check for key presses to switch between debugging info.

		//Read frame.
		camera.CaptureFrameRGB(frame);

		//Process frame.
		RGBToGreyscale(frame,greyscaleFrame);
		canny.Process(greyscaleFrame,cannyFrame);
		BlendAdd(frame,cannyFrame,mergedFrame);

		HoughTransform(cannyFrame,houghTransformFrame);

		//Draw frame (and any debugging info).
		//TODO: Fit each frame into a box based on number of frames being drawn.
#ifdef __linux
		painter.DrawImage(50,50,640,480,mergedFrame);
#elif defined _WIN32
		painter.DrawImage(0,0,640,360,frame);
#endif
		DrawHoughTransform(painter,width,height,houghTransformFrame,0.75f);

		std::vector<std::pair<float,float>> lines;
		FindLines(cannyFrame,houghTransformFrame,lines);
		DrawLines(painter,50,50,640,480,lines);

		CheckGLError();
		glfwSwapBuffers(window);
	}

	glfwTerminate();
	return 0;
}

