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

#include <iostream>
#include <cassert>
#include <GLES3/gl3.h>
#include <GLFW/glfw3.h>
#include "Camera.h"
#include "Image.h"
#include "Painter.h"

//Old puzzle loading from file code that's kept around in case of debugging.
#if 0
static unsigned char AsciiToUChar(const char input)
{
	if(input >= '1' && input <= '9')
		return input - '0';
	return 0;
}

static bool LoadFromFile(const char* filePath,Game& game)
{
	game.Clear();

	std::ifstream file(filePath);
	if(!file.is_open())
	{
		std::cerr << "Could not open file." << std::endl;
		return false;
	}

	char buffer[Game::WIDTH + 1];
	for(unsigned int y = 0;y < Game::HEIGHT;y++)
	{
		file.getline(buffer,sizeof(buffer));
		for(unsigned int x = 0;x < Game::WIDTH;x++)
		{
			if(buffer[x] == '\0')
				break;
			game.Set(x,y,AsciiToUChar(buffer[x]));
		}
	}

	return true;
}
#endif

void CheckGLError()
{
	const GLenum error = glGetError();
	if(error == GL_NO_ERROR)
		return;

	std::cout << "OpenGL Error: " << error << std::endl;
	std::abort();
}

int main(int argc,char* argv[])
{
//More old command-line only puzzle loading and solving code kept around for debugging.
#if 0
	if(argc < 2)
	{
		std::cerr << "Usage: sudoku_solver <filename>" << std::endl;
		return 0;
	}

	Game game;
	if(!LoadFromFile(argv[1],game))
		return -1;

	if(!Solve(game))
		return -1;

	game.Print();
#endif
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API,GLFW_OPENGL_ES_API);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,0);
	glfwWindowHint(GLFW_RESIZABLE,GL_FALSE);

	GLFWwindow* window = glfwCreateWindow(800,600,"Sudoku Solver AR",nullptr,nullptr);
	assert(window != nullptr);
	glfwMakeContextCurrent(window);

	int width = 0;
	int height = 0;
	glfwGetFramebufferSize(window,&width,&height);
	glViewport(0,0,width,height);
	CheckGLError();

	Painter painter(width,height);
	Camera camera = Camera::Open("/dev/video0").value();
	Image frame;

	while(!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
		//TODO: Check for key presses to switch between debugging info.

		//Read frame.
		camera.CaptureFrameRGB(frame);

		//TODO: Process frame.

		//Draw frame (and any debugging info).
		painter.DrawImage(0,0,640,480,frame);

		CheckGLError();
		glfwSwapBuffers(window);
	}

	glfwTerminate();
	return 0;
}

