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
#ifdef __linux
#include <GLES3/gl3.h>
#elif defined _WIN32
#include <GL/glew.h>
#endif
#include <GLFW/glfw3.h>
#include "Camera.h"
#include "Image.h"
#include "Painter.h"

void CheckGLError()
{
	const GLenum error = glGetError();
	if(error == GL_NO_ERROR)
		return;

	std::cout << "OpenGL Error: " << error << std::endl;
	std::abort();
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

	while(!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
		//TODO: Check for key presses to switch between debugging info.

		//Read frame.
		camera.CaptureFrameRGB(frame);

		//TODO: Process frame.

		//Draw frame (and any debugging info).
		//TODO: Fit each frame into a box based on number of frames being drawn.
#ifdef __linux
		painter.DrawImage(0,0,640,480,frame);
#elif defined _WIN32
		painter.DrawImage(0,0,640,360,frame);
#endif

		CheckGLError();
		glfwSwapBuffers(window);
	}

	glfwTerminate();
	return 0;
}

