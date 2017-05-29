# Sudoku Solver AR

**Sudoku Solver AR** is an augmented reality sudoku solver. It captures video using a camera, finds sudoku puzzles in the video, and then displays answers to those puzzles in such a way that they look like they are part of the original scene.

All of the image processing and computer vision code is written from scratch and does not use any external libraries.

## Progress

* [x] Capture video from webcam
* [x] Preprocess image processing
* [x] Line detection
* [x] Puzzle detection
* [x] Puzzle dewarp
* [ ] OCR numbers
* [x] Solve puzzle
* [x] Render puzzle solution
* [x] Composite solution over original puzzle

## Building and Running

### Linux

Requirements

* [CMake](https://cmake.org/) >= 3.6.2
* GCC >= 6.3.1 OR clang >= 3.9.1
* [GLFW](http://www.glfw.org/) >= 3.2
* mesa-libGL
* mesa-libGLES

Clone the source directory using:

`git clone https://github.com/jbendig/Sudoku-Solver-AR.git`

Change to the new directory and create a build directory:

```
cd Sudoku-Solver-AR
mkdir build
cd build
```

Configure the build:

`cmake ../`

Build:

`make`

Run (Make sure to connect a webcam first):

`make run`

### Windows

Requirements

* [CMake](https://cmake.org/) >= 3.6.2
* [Visual Studio](https://www.visualstudio.com/) >= 2017 (Older versions are not supported because C++17 support is required)
* [GLFW](http://www.glfw.org/) >= 3.2
* [GLEW](http://glew.sourceforge.net/) >= 2.0
* [OpenGL ES 3.0 Headers](https://www.khronos.org/registry/OpenGL/index_es.php#headers3) (Make sure to get KHR/khrplatform.h on the same page)

Clone or download source code from:

`https://github.com/jbendig/Sudoku-Solver-AR.git`

Run CMake in the resulting directory and configure the following variables:

* **GLFW_INCLUDE_DIR**: GLFW include directory
* **GLFW_LIBRARY_DIR**: GLFW library directory
* **GLES3_INCLUDE_DIR**: OpenGL ES 3 include directory
* **GLEW_INCLUDE_DIR**: GLEW include directory
* **GLEW_LIBRARY_DIR**: GLEW library directory

Open the `sudoku_solver_ar.sln` file in the build directory you selected with CMake. Then build and run using Visual Studio. Make sure to connect a supported webcam before running or else the program will fail to start.

## Camera Support

Camera support is currently extremely limited. Only cameras that output the following formats will work.

| **Linux** | **Windows** |
|-----------|-------------|
| YUV 4:2:2 | NV12        |

## Licensing

Sudoku Solver AR is dual licensed under both the MIT license and the Apache License (Version 2.0). Pick the license that is more convenient.

See [LICENSE-MIT](LICENSE-MIT) and [LICENSE-APACHE](LICENSE-APACHE).
