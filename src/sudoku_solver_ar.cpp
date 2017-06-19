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
#include <random>
#include <tuple>
#include <cassert>
#include <cmath>
#include <ft2build.h>
#include FT_FREETYPE_H
#ifdef __linux
#include <GLES3/gl3.h>
#elif defined _WIN32
#include <GL/glew.h>
#endif
#include <GLFW/glfw3.h>
#include "CachedPuzzleSolver.h"
#include "Camera.h"
#include "Geometry.h"
#include "Image.h"
#include "ImageProcessing.h"
#include "NeuralNetwork.h"
#include "Painter.h"
#include "PuzzleFinder.h"

static constexpr unsigned int PUZZLE_IMAGE_WIDTH = 144;
static constexpr unsigned int PUZZLE_IMAGE_HEIGHT = PUZZLE_IMAGE_WIDTH;
static constexpr unsigned int PUZZLE_DISPLAY_WIDTH = 600;
static constexpr unsigned int PUZZLE_DISPLAY_HEIGHT = PUZZLE_DISPLAY_WIDTH;
#ifdef __linux
static constexpr char* PUZZLE_SOLUTION_FONT = "/usr/share/fonts/oxygen/Oxygen-Sans.ttf";
#elif defined _WIN32
static constexpr char* PUZZLE_SOLUTION_FONT = "C:/Windows/Fonts/times.ttf";
#else Platform not supported
#endif

static bool drawLines = false;
static bool drawLineClusters = false;
static bool drawPossiblePuzzleLineClusters = false;
static bool drawHoughTransform = false;

void CheckGLError()
{
	const GLenum error = glGetError();
	if(error == GL_NO_ERROR)
		return;

	std::cout << "OpenGL Error: " << error << std::endl;
	std::abort();
}

void DrawLines(Painter& painter,const float x,const float y,const float width,const float height,const std::vector<Line>& lines,const unsigned char red,const unsigned char green,const unsigned char blue)
{
	for(auto line : lines)
	{
		//Based on the equation x*cos(theta) + y*sin(theta) = rho.
		float theta = line.theta;
		float rho = line.rho;

		//Rho should be positive to simplify finding clipping points below.
		if(rho < 0.0f)
		{
			theta = fmodf(theta + M_PI,2.0f * M_PI);
			rho *= -1.0f;
		}

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
							 red,green,blue);
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
						 red,green,blue);
	}
}

void DrawLineClusters(Painter& painter,const float x,const float y,const float width,const float height,const std::vector<std::vector<Line>>& lineClusters)
{
	//Random set of colors to alternate through so clusters can be told apart.
	std::vector<std::tuple<unsigned char,unsigned char,unsigned char>> clusterColors;
	clusterColors.push_back({255,0,0});
	clusterColors.push_back({128,0,255});
	clusterColors.push_back({0,255,0});
	clusterColors.push_back({255,255,0});
	clusterColors.push_back({0,255,255});
	clusterColors.push_back({128,255,255});
	clusterColors.push_back({255,0,255});

	for(unsigned int x = 0;x < lineClusters.size();x++)
	{
		const auto clusterColor = clusterColors[x % clusterColors.size()];
		const unsigned char red = std::get<0>(clusterColor);
		const unsigned char green = std::get<1>(clusterColor);
		const unsigned char blue = std::get<2>(clusterColor);
		DrawLines(painter,x,y,width,height,lineClusters[x],red,green,blue);
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

void FitImage(const unsigned int windowWidth,const unsigned int windowHeight,const Image& image,unsigned int& x,unsigned int& y,unsigned int& width,unsigned int& height)
{
	const float hRatio = static_cast<float>(image.width) / static_cast<float>(windowWidth);
	const float vRatio = static_cast<float>(image.height) / static_cast<float>(windowHeight);
	const float scale = 1.0f / std::max(hRatio,vRatio);

	width = image.width * scale;
	height = image.height * scale;
	x = abs(static_cast<int>(windowWidth) - static_cast<int>(width)) / 2;
	y = abs(static_cast<int>(windowHeight) - static_cast<int>(height)) / 2;
}

void GeneratePlaceholderAnswerImage(Image& image)
{
	constexpr unsigned int IMAGE_WIDTH = 600;
	constexpr unsigned int IMAGE_HEIGHT = 600;
	constexpr unsigned int BOX_WIDTH = 33;
	constexpr unsigned int BOX_HEIGHT = 33;
	constexpr float DX = ((IMAGE_WIDTH / 9.0f) - BOX_WIDTH) / 2.0f;
	constexpr float DY = ((IMAGE_HEIGHT / 9.0f) - BOX_HEIGHT) / 2.0f;

	image.width = IMAGE_WIDTH;
	image.height = IMAGE_HEIGHT;
	image.data.resize(image.width * image.height * 3);
	std::fill(image.data.begin(),image.data.end(),255);

	auto DrawBox = [&](const unsigned int x,const unsigned int y,const unsigned int width,const unsigned int height)
	{
		for(unsigned int v = y;v < y + height;v++)
		{
			for(unsigned int h = x;h < x + width;h++)
			{
				const unsigned int index = (v * image.width + h) * 3;
				image.data[index + 0] = 16;
				image.data[index + 1] = 16;
				image.data[index + 2] = 16;
			}
		}
	};

	for(unsigned int y = 0;y < 9;y++)
	{
		for(unsigned int x = 0;x < 9;x++)
		{
			DrawBox(lround(DX + 2 * x * DX + x * BOX_WIDTH),lround(DY + 2 * y * DY + y * BOX_HEIGHT),BOX_WIDTH,BOX_HEIGHT);
		}
	}
}

static void RenderPuzzle(Painter& painter,const std::string& font,const unsigned int fontSize,const std::vector<unsigned char>& digits,Image& image)
{
	auto DrawBitmapCentered = [&image](unsigned int offsetX,unsigned int offsetY,const unsigned int targetWidth,const unsigned int targetHeight,FT_Bitmap& bitmap)
	{
		offsetX += (targetWidth - bitmap.width) / 2;
		offsetY += (targetHeight - bitmap.rows) / 2;

		for(unsigned int y = 0;y < bitmap.rows;y++)
		{
			for(unsigned int x = 0;x < bitmap.width;x++)
			{
				const unsigned int inputIndex = (y * bitmap.pitch) + x;
				const unsigned int outputIndex = ((y + offsetY) * image.width + x + offsetX) * 3;
				image.data[outputIndex + 0] = 255 - bitmap.buffer[inputIndex];
				image.data[outputIndex + 1] = 255 - bitmap.buffer[inputIndex];
				image.data[outputIndex + 2] = 255 - bitmap.buffer[inputIndex];
			}
		}
	};

	assert(digits.size() == 9*9);
	image.width = 600;
	image.height = 600;
	image.data.resize(image.width * image.height * 3);
	std::fill(image.data.begin(),image.data.end(),255);

	FT_Library ftLibrary;
	if(FT_Init_FreeType(&ftLibrary) != FT_Err_Ok)
		std::abort();

	FT_Face face;
	if(FT_New_Face(ftLibrary,font.c_str(),0,&face) != FT_Err_Ok)
		std::abort();
	if(FT_Set_Pixel_Sizes(face,0,fontSize) != FT_Err_Ok)
		std::abort();

	const float dx = image.width / 9.0f;
	const float dy = image.height / 9.0f;
	for(unsigned int y = 0;y < 9;y++)
	{
		for(unsigned int x = 0;x < 9;x++)
		{
			const unsigned int digitIndex = y * 9 + x;
			if(digits[digitIndex] == 0)
				continue;

			if(FT_Load_Char(face,digits[digitIndex] + '0',FT_LOAD_RENDER) != FT_Err_Ok)
				std::abort();

			DrawBitmapCentered(lrintf(x * dx),lrintf(y * dy),dx,dy,face->glyph->bitmap);
		}
	}

	FT_Done_FreeType(ftLibrary);
}

static void ExtractPuzzleTiles(const Image& image,std::vector<Image>& tiles)
{
	auto ExtractImage = [&image](const unsigned int x,const unsigned int y,unsigned int width,unsigned int height)
	{
		Image extractedImage(width,height);
		std::fill(extractedImage.data.begin(),extractedImage.data.end(),255);

		assert(x < image.width);
		assert(y < image.height);
		width = std::min(width,image.width - x);
		height = std::min(height,image.height - y);

		const unsigned int span = width * 3;
		for(unsigned int row = 0;row < height;row++)
		{
			const unsigned int inputIndex = (((row + y) * image.width) + x) * 3;
			const unsigned int outputIndex = row * extractedImage.width * 3;
			memcpy(&extractedImage.data[outputIndex],&image.data[inputIndex],span);
		}

		return extractedImage;
	};

	tiles.clear();
	if(image.width == 0 || image.height == 0)
		return;

	const unsigned int dx = lrintf(image.width / 9.0f);
	const unsigned int dy = lrintf(image.height / 9.0f);
	for(unsigned int y = 0;y < 9;y++)
	{
		for(unsigned int x = 0;x < 9;x++)
		{
			const Image image = ExtractImage(x * dx,y * dy,dx,dy);
			tiles.push_back(image);
		}
	}
}

static void PreprocessNeuralNetworkImage(Image& image,const float a,const unsigned char binaryHigh)
{
	if(image.width == 0 || image.height == 0)
		return;

	//Compute the global mean.
	float globalMean = 0.0f;
	for(unsigned int x = 0;x < image.width * image.height;x++)
	{
		const unsigned int index = x * 3;
		globalMean += image.data[index];
	}
	globalMean /= static_cast<float>(image.width * image.height);

	//Helper function to get the (greyscale) pixel from an image with the edges clamped to the
	//nearest pixel.
	auto GetPixel = [&image](int x,int y)
	{
		x = std::min(std::max(0,x),static_cast<int>(image.width) - 1);
		y = std::min(std::max(0,y),static_cast<int>(image.height) - 1);
		const unsigned int index = (y * image.width + x) * 3;
		return image.data[index];
	};

	//Localized thresholding using local standard deviation and global mean. It works well for
	//solid backgrounds like our digits. The "a" and "b" factors are found through experimentation.
	Image outputImage;
	outputImage.MatchSize(image);
	float pixels[9];
	constexpr float b = 0.95f;
	for(int y = 0;y < static_cast<int>(image.height);y++)
	{
		for(int x = 0;x < static_cast<int>(image.width);x++)
		{
			pixels[0] = GetPixel(x - 1,y - 1);
			pixels[1] = GetPixel(x,y - 1);
			pixels[2] = GetPixel(x + 1,y - 1);
			pixels[3] = GetPixel(x - 1,y);
			pixels[4] = GetPixel(x,y);
			pixels[5] = GetPixel(x + 1,y);
			pixels[6] = GetPixel(x - 1,y + 1);
			pixels[7] = GetPixel(x,y + 1);
			pixels[8] = GetPixel(x + 1,y + 1);

			const float localMean = (pixels[0] + pixels[1] + pixels[2] + pixels[3] + pixels[4] + pixels[5] + pixels[6] + pixels[7] + pixels[8]) / 9.0f;
			float localVar = 0.0f;
			for(unsigned int x = 0;x < 9;x++)
			{
				const float toSquareTerm = pixels[4] - localMean;
				localVar += toSquareTerm * toSquareTerm;
			}
			localVar /= 9.0f;
			const float localStdDev = sqrtf(localVar);

			const unsigned int index = (y * image.width + x) * 3;
			if(pixels[4] > a * localStdDev && pixels[4] > b * globalMean)
			{
				outputImage.data[index + 0] = binaryHigh;
				outputImage.data[index + 1] = binaryHigh;
				outputImage.data[index + 2] = binaryHigh;
			}
			else
			{
				outputImage.data[index + 0] = 0;
				outputImage.data[index + 1] = 0;
				outputImage.data[index + 2] = 0;
			}
		}
	}
	image = outputImage;
}

static void ShuffleEdgePixels(std::mt19937& randomNumberGenerator,Image& image,const unsigned char binaryHigh)
{
	//Each edge pixel is given a random number using keepPixelDist(). If it's greater than the
	//randomly selected V, the pixel will be copied to a random neighbor's pixel and then the
	//original pixel is inverted.
	std::uniform_real_distribution<> keepPixelDist(0.0f,1.0f);
	std::uniform_real_distribution<> noiseDist(0.95,0.99f);
	const float V = noiseDist(randomNumberGenerator);

	//Used for selecting a random neighbor when a pixel is shuffled. A value of 0 means left or
	//above and a value of 1 means right or below.
	std::uniform_int_distribution<> newPosDist(0,1);

	//Threshold used to determine if a pixel is an edge. Generally an edge is anything not zero.
	const float laplaceThreshold = 0.1f * static_cast<float>(binaryHigh);

	Image tempImage = image;
	const unsigned int rowSpan = tempImage.width * 3;
	for(unsigned int y = 1;y < tempImage.height - 1;y++)
	{
		for(unsigned int x = 1;x < tempImage.width - 1;x++)
		{
			const unsigned int inputIndex = (y * tempImage.width + x) * 3;
			const float laplace = static_cast<float>(tempImage.data[inputIndex - rowSpan]) +
								  static_cast<float>(tempImage.data[inputIndex - 3])       + static_cast<float>(tempImage.data[inputIndex + 3]) * -4.0f + static_cast<float>(tempImage.data[inputIndex + 3]) +
								  static_cast<float>(tempImage.data[inputIndex + rowSpan]);
			if(fabsf(laplace) > laplaceThreshold)
			{
				if(keepPixelDist(randomNumberGenerator) > V)
				{
					//Move pixel value to a neighbor spot.
					const int newX = x + -1 + 2 * newPosDist(randomNumberGenerator);
					const int newY = y + -1 + 2 * newPosDist(randomNumberGenerator);
					const unsigned int otherIndex = (newY * tempImage.width + newX) * 3;
					image.data[otherIndex + 0] = image.data[inputIndex + 0];
					image.data[otherIndex + 1] = image.data[inputIndex + 1];
					image.data[otherIndex + 2] = image.data[inputIndex + 2];

					//Invert pixel.
					image.data[inputIndex + 0] = abs(static_cast<int>(binaryHigh) - static_cast<int>(image.data[inputIndex + 0]));
					image.data[inputIndex + 1] = abs(static_cast<int>(binaryHigh) - static_cast<int>(image.data[inputIndex + 1]));
					image.data[inputIndex + 2] = abs(static_cast<int>(binaryHigh) - static_cast<int>(image.data[inputIndex + 2]));
				}
			}
		}
	}
}

std::vector<unsigned char> ImageToData(const Image& image)
{
	//Assumes image is greyscale and just copies red-channel to data.
	std::vector<unsigned char> data(image.width * image.height);
	for(unsigned int x = 0;x < image.width * image.height;x++)
	{
		data[x] = image.data[x * 3];
	}

	return data;
}

static void ExtractDigits(NeuralNetwork& nn,const Image& puzzleImage,std::vector<unsigned char>& digits)
{
	digits.clear();

	std::vector<Image> puzzleTiles;
	ExtractPuzzleTiles(puzzleImage,puzzleTiles);
	for(unsigned int x = 0;x < puzzleTiles.size();x++)
	{
		PreprocessNeuralNetworkImage(puzzleTiles[x],2.0f,1);
		const unsigned char digit = nn.Run(ImageToData(puzzleTiles[x]));
		digits.push_back(digit);
	}
}

static void GenerateRandomPuzzle(Painter& painter,std::mt19937& randomNumberGenerator,Image& puzzleImage,std::vector<unsigned char>& digits,const unsigned int binaryHigh)
{
	//Select a random font.
	const std::vector<std::string> fonts = {
#ifdef __linux
		"/usr/share/fonts/oxygen/Oxygen-Sans.ttf",
		"/usr/share/fonts/oxygen/OxygenMono-Regular.ttf",
		"/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
		"/usr/share/fonts/liberation/LiberationMono-Regular.ttf",
		"/usr/share/fonts/google-droid/DroidSans.ttf",
		"/usr/share/fonts/dejavu/DejaVuSans.ttf",
		"/usr/share/fonts/dejavu/DejaVuSerif.ttf",
#elif defined _WIN32
		"C:/Windows/Fonts/arial.ttf",
		"C:/Windows/Fonts/calibri.ttf",
		"C:/Windows/Fonts/cambria.ttc",
		"C:/Windows/Fonts/Candara.ttf",
		"C:/Windows/Fonts/constan.ttf",
		"C:/Windows/Fonts/couri.ttf",
		"C:/Windows/Fonts/Gabriola.ttf",
		"C:/Windows/Fonts/times.ttf",
#else
#error Platform not supported
#endif
	};

	std::uniform_int_distribution<> fontDist(0,fonts.size() - 1);
	const std::string& font = fonts[fontDist(randomNumberGenerator)];

	//Select a random digit for each box.
	std::uniform_int_distribution<> digitDist(0,9);
	digits.clear();
	for(unsigned int x = 0;x < 81;x++)
	{
		digits.push_back(digitDist(randomNumberGenerator));
	}

	//Render the puzzle digits.
	std::uniform_int_distribution<> fontSizeDist(48,64);
	RenderPuzzle(painter,font,fontSizeDist(randomNumberGenerator),digits,puzzleImage);

	//Draw a border and grid.
	Image srcImage = puzzleImage;
	painter.DrawPuzzleGrid(srcImage,
						   16.0f, //Border line width (px).
						   4.0f, //Grid minor line width (px).
						   8.0f, //Grid major line width (px).
						   puzzleImage);

	//Add some noise and perform a random slight perspective warp.
	srcImage = puzzleImage;
	painter.DrawWarpedAndUnwarpedPuzzle(srcImage,
										1024, //Framebuffer width and height.
										200.0f, //Perspective warp corner random radius.
										0.15f, //Noise delta.
										puzzleImage, //Destination image.
										144); //Destination image with and height.

	//Preprocess the puzzle as a binary image to improve training speed and accuracy.
	std::uniform_real_distribution<> aDist(2.0f,4.0f);
	const float a = aDist(randomNumberGenerator);
	PreprocessNeuralNetworkImage(puzzleImage,a,binaryHigh);

	//Randomize edge pixels to help make the model more general.
	ShuffleEdgePixels(randomNumberGenerator,puzzleImage,binaryHigh);
}

static NeuralNetwork PrepareOCRNeuralNetwork(Painter& painter)
{
	std::random_device randomDevice;
	std::mt19937 randomNumberGenerator(randomDevice());

	//Render a bunch of random puzzles using the following fonts. Each puzzle is processed with
	//noise to help improve training results.
	auto GenerateSampleData = [&painter,&randomNumberGenerator]()
	{
		std::vector<std::pair<std::vector<unsigned char>,unsigned char>> data;
		for(unsigned int x = 0;x < 3000;x++)
		{
			Image puzzleImage;
			std::vector<unsigned char> digits;
			GenerateRandomPuzzle(painter,randomNumberGenerator,puzzleImage,digits,1);

			std::vector<Image> puzzleTiles;
			ExtractPuzzleTiles(puzzleImage,puzzleTiles);
			for(unsigned int x = 0;x < puzzleTiles.size();x++)
			{
				data.push_back({ImageToData(puzzleTiles[x]),digits[x]});
			}
		}

		return data;
	};

	//Train neural network. It will likely take along time unless a pre-trained network is detected
	//and loaded from file.
	NeuralNetwork nn = NeuralNetwork::Train([&GenerateSampleData,&randomNumberGenerator](std::vector<std::pair<std::vector<unsigned char>,unsigned char>>& trainingData) {
		trainingData = GenerateSampleData();

		//Randomize puzzle tiles so NN doesn't over train to a specific font.
		std::shuffle(trainingData.begin(),trainingData.end(),randomNumberGenerator);
	});

	//Measure how accurate the NN is.
	const std::vector<std::pair<std::vector<unsigned char>,unsigned char>> testData = GenerateSampleData();
	unsigned int correct = 0;
	for(const auto& td : testData)
	{
		if(nn.Run(td.first) == td.second)
			correct += 1;
	}
	std::cout << "Identified " << correct << " out of " << testData.size() << std::endl;

	return nn;
}

void OnKey(GLFWwindow* window,int key,int scancode,int action,int mode)
{
	if(action != GLFW_PRESS)
		return;

	if(key == GLFW_KEY_ESCAPE)
		glfwSetWindowShouldClose(window,GL_TRUE);
	else if(key == GLFW_KEY_0)
		drawHoughTransform = !drawHoughTransform;
	else if(key == GLFW_KEY_1)
		drawLines = !drawLines;
	else if(key == GLFW_KEY_2)
		drawLineClusters = !drawLineClusters;
	else if(key == GLFW_KEY_3)
		drawPossiblePuzzleLineClusters = !drawPossiblePuzzleLineClusters;
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

	GLFWwindow* window = glfwCreateWindow(800 + PUZZLE_DISPLAY_WIDTH,600,"Sudoku Solver AR",nullptr,nullptr);
	assert(window != nullptr);
	glfwMakeContextCurrent(window);
	glfwSetKeyCallback(window,OnKey);

#ifdef _WIN32
	glewInit();
#endif

	int windowWidth = 0;
	int windowHeight = 0;
	glfwGetFramebufferSize(window,&windowWidth,&windowHeight);

	Painter painter;
	NeuralNetwork nn = PrepareOCRNeuralNetwork(painter);
	Camera camera = Camera::Open("/dev/video0").value();
	Image frame;
	Image downscaledFrame;
	Image* inputFrame = &frame;
	Image greyscaleFrame;
	Image cannyFrame;
	Canny canny = Canny::WithRadius(5.0f);
	Image mergedFrame;
	Image houghTransformFrame;
	Image puzzleFrame;
	Image displayPuzzleFrame;
	Image solutionImage;
	PuzzleFinder puzzleFinder;
	CachedPuzzleSolver puzzleSolver;

	while(!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		//Read frame.
		camera.CaptureFrameRGB(frame);

		//Figure out how to draw image so that it fits window.
		unsigned int drawImageX = 0;
		unsigned int drawImageY = 0;
		unsigned int drawImageWidth = 0;
		unsigned int drawImageHeight = 0;
		FitImage(windowWidth - PUZZLE_DISPLAY_WIDTH,windowHeight,frame,drawImageX,drawImageY,drawImageWidth,drawImageHeight);

		if(frame.width * frame.height > drawImageWidth * drawImageHeight)
		{
			painter.ScaleImage(frame,downscaledFrame,drawImageWidth,drawImageHeight);
			inputFrame = &downscaledFrame;
		}
		else
			inputFrame = &frame;

		//Process frame.
		greyscaleFrame.MatchSize(*inputFrame);
		RGBToGreyscale(&inputFrame->data[0],greyscaleFrame);
		canny.Process(greyscaleFrame,cannyFrame);
		BlendAdd(*inputFrame,cannyFrame,mergedFrame);

		HoughTransform(cannyFrame,houghTransformFrame);

		std::vector<Point> puzzlePoints;
		if(puzzleFinder.Find(drawImageWidth,drawImageHeight,houghTransformFrame,puzzlePoints))
		{
			const Point scalerPoint = {1.0f / drawImageWidth,1.0f / drawImageHeight};
			painter.ExtractImage(greyscaleFrame,
								 puzzlePoints[0] * scalerPoint,
								 puzzlePoints[1] * scalerPoint,
								 puzzlePoints[2] * scalerPoint,
								 puzzlePoints[3] * scalerPoint,
								 puzzleFrame,
								 PUZZLE_IMAGE_WIDTH,
								 PUZZLE_IMAGE_HEIGHT);
			displayPuzzleFrame = puzzleFrame;
			PreprocessNeuralNetworkImage(displayPuzzleFrame,2.0f,255);
		}

		//Draw frame and extracted puzzle if available.
		glViewport(0,0,windowWidth,windowHeight);
		painter.DrawImage(drawImageX,drawImageY,drawImageWidth,drawImageHeight,mergedFrame);
		painter.DrawImage(800,0,PUZZLE_DISPLAY_WIDTH,PUZZLE_DISPLAY_HEIGHT,displayPuzzleFrame);

		//Draw solution composite over puzzle if available.
		if(!puzzlePoints.empty())
		{
			//Cut puzzle into 9x9 chunks and run neural network on each to extract the respective
			//digit.
			std::vector<unsigned char> digits;
			ExtractDigits(nn,puzzleFrame,digits);

			//Render the solution puzzle to a texture. It might fail if the puzzle doesn't have a
			//solution or if the neural network made a mistake reading the digits. Then the last
			//successful solution is used instead in hope that it's still correct.
			std::vector<unsigned char> solution;
			if(puzzleSolver.Solve(digits,solution) || puzzleSolver.GetLastUsedSolution(solution))
			{
				//Replace digits in the solution with zeros so the resulting texture doesn't draw
				//over the original digits.
				assert(digits.size() == solution.size());
				for(unsigned int x = 0;x < digits.size();x++)
				{
					if(solution[x] == digits[x])
						solution[x] = 0;
				}

				RenderPuzzle(painter,PUZZLE_SOLUTION_FONT,48,solution,solutionImage);
			}
			else
				//Draw a placeholder to indicate that a puzzle was found even if it couldn't be
				//used.
				GeneratePlaceholderAnswerImage(solutionImage);

			//Preprocess they greyscale image (with black text on a white background) so the
			//numbers are green. This is part of a trick where we draw the image twice using
			//blending so we don't have to add an alpha channel.
			for(unsigned int x = 0;x < solutionImage.width * solutionImage.height;x++)
			{
				const unsigned int index = x * 3;
				const unsigned char value = 255 - solutionImage.data[index];
				const unsigned char invertedValue = 255 - value;
				solutionImage.data[index + 0] = invertedValue;
				solutionImage.data[index + 1] = value;
				solutionImage.data[index + 2] = invertedValue;
			}

			//Render the solution texture right over the original puzzle.
			glEnable(GL_BLEND);
			for(Point& point : puzzlePoints)
			{
				point.x += drawImageX;
				point.y += drawImageY;
			}
			assert(puzzlePoints.size() == 4);

			glColorMask(GL_FALSE,GL_TRUE,GL_FALSE,GL_FALSE);
			glBlendEquationSeparate(GL_MAX,GL_MAX);
			glBlendFuncSeparate(GL_ONE,GL_ONE,GL_ONE,GL_ZERO);
			painter.DrawImage(puzzlePoints[0],puzzlePoints[1],puzzlePoints[2],puzzlePoints[3],solutionImage);

			glColorMask(GL_TRUE,GL_FALSE,GL_TRUE,GL_FALSE);
			glBlendEquationSeparate(GL_MIN,GL_MAX);
			glBlendFuncSeparate(GL_ONE,GL_ONE,GL_ONE,GL_ZERO);
			painter.DrawImage(puzzlePoints[0],puzzlePoints[1],puzzlePoints[2],puzzlePoints[3],solutionImage);

			glDisable(GL_BLEND);
			glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
		}

		//Draw debug info.
		if(drawLines)
			DrawLines(painter,drawImageX,drawImageY,drawImageWidth,drawImageHeight,puzzleFinder.lines,10,10,10);
		if(drawLineClusters)
		{
			std::sort(puzzleFinder.lineClusters.begin(),puzzleFinder.lineClusters.end(),[](const auto& lhs,const auto& rhs) {
				return MeanTheta(lhs) < MeanTheta(rhs);
			});
			DrawLineClusters(painter,drawImageX,drawImageY,drawImageWidth,drawImageHeight,puzzleFinder.lineClusters);
		}
		if(drawPossiblePuzzleLineClusters)
		{
			std::sort(puzzleFinder.possiblePuzzleLineClusters.begin(),puzzleFinder.possiblePuzzleLineClusters.end(),[](const auto& lhs,const auto& rhs) {
				return MeanTheta(lhs) < MeanTheta(rhs);
			});
			DrawLineClusters(painter,drawImageX,drawImageY,drawImageWidth,drawImageHeight,puzzleFinder.possiblePuzzleLineClusters);
		}
		if(drawHoughTransform)
			DrawHoughTransform(painter,windowWidth - 600,windowHeight,houghTransformFrame,0.75f);

		CheckGLError();
		glfwSwapBuffers(window);
	}

	glfwTerminate();
	return 0;
}

