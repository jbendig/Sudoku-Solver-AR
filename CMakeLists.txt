CMAKE_MINIMUM_REQUIRED(VERSION 3.6.2)

PROJECT(sudoku_solver_ar)

IF(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	SET(LINUX True)
	SET(CMAKE_CXX_FLAGS_RELEASE " -O2 -s")
ELSEIF(WIN32)
	SET(GLFW_INCLUDE_DIR "" CACHE PATH "GLFW include directory")
	SET(GLFW_LIBRARY_DIR "" CACHE PATH "GLFW library dictory")
	SET(GLES3_INCLUDE_DIR "" CACHE PATH "OpenGL ES 3 include directory")
	SET(GLEW_INCLUDE_DIR "" CACHE PATH "GLEW include directory")
	SET(GLEW_LIBRARY_DIR "" CACHE PATH "GLEW library directory")
	LINK_DIRECTORIES("${GLFW_LIBRARY_DIR}" "${GLEW_LIBRARY_DIR}")
	SET(GUI_TYPE WIN32)
ENDIF()

FIND_PACKAGE(Freetype REQUIRED)
INCLUDE_DIRECTORIES("${FREETYPE_INCLUDE_DIRS}")

FIND_PROGRAM(CCACHE_FOUND ccache)
IF(CCACHE_FOUND)
	SET_PROPERTY(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${CCACHE})
	SET_PROPERTY(GLOBAL PROPERTY RULE_LAUNCH_LINK ${CCACHE})
ENDIF()

SET(SOURCE_FILES
	src/sudoku_solver_ar.cpp
	src/Camera.cpp
	src/Game.cpp
	src/Geometry.cpp
	src/ImageProcessing.cpp
	src/Painter.cpp
	src/PuzzleFinder.cpp
	src/ShaderProgram.cpp
	src/Solve.cpp
)

ADD_EXECUTABLE(sudoku_solver_ar ${GUI_TYPE} ${SOURCE_FILES})
IF(LINUX)
	SET_TARGET_PROPERTIES(sudoku_solver_ar PROPERTIES COMPILE_FLAGS "-Wall -Wtype-limits -Woverloaded-virtual -msse2 -msse3 -std=c++1z")
	TARGET_LINK_LIBRARIES(sudoku_solver_ar glfw GL ${FREETYPE_LIBRARIES})
	ADD_CUSTOM_TARGET(run DEPENDS sudoku_solver_ar WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" COMMAND "${CMAKE_BINARY_DIR}/sudoku_solver_ar" || true)
	ADD_CUSTOM_TARGET(debug DEPENDS sudoku_solver_ar WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" COMMAND "gdb" "${CMAKE_BINARY_DIR}/sudoku_solver_ar" "-ex" "run" || true)
ELSEIF(WIN32)
	ADD_DEFINITIONS(-DNOMINMAX -DGLEW_STATIC -D_USE_MATH_DEFINES)
	SET_TARGET_PROPERTIES(sudoku_solver_ar PROPERTIES COMPILE_FLAGS "/std:c++latest")
	SET_TARGET_PROPERTIES(sudoku_solver_ar PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
	TARGET_LINK_LIBRARIES(sudoku_solver_ar glfw3 opengl32 glew32s mfplat mfreadwrite mf mfuuid)
	TARGET_INCLUDE_DIRECTORIES(sudoku_solver_ar PRIVATE "${GLFW_INCLUDE_DIR}" "${GLES3_INCLUDE_DIR}" "${GLEW_INCLUDE_DIR}")
ENDIF()

IF(LINUX)
	ADD_EXECUTABLE(sudoku_solver src/sudoku_solver.cpp src/Game.cpp src/Solve.cpp)
	SET_TARGET_PROPERTIES(sudoku_solver PROPERTIES COMPILE_FLAGS "-Wall -Wtype-limits -Woverloaded-virtual -msse2 -msse3 -std=c++1z")
ENDIF()

