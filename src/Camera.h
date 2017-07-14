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

#ifndef CAMERA_H
#define CAMERA_H

#include <vector>
#include <optional>
#ifdef __linux
#include <linux/videodev2.h>
#elif defined _WIN32
#include "ComHelp.h"
struct IMFSourceReader;
#else
#error "Platform not supported"
#endif

struct Image;

class Camera
{
	public:
		Camera(Camera&& other);
		~Camera();

		static std::optional<Camera> Open(const std::string& devicePath);
		//TODO: Support camera enumeration.

		bool CaptureFrameRGB(Image& frame);
		bool CaptureFrameGreyscale(Image& frame);
	private:
#ifdef __linux
		int fd;
		v4l2_format format;
		std::vector<unsigned char> buffer;
#elif defined _WIN32
		ComPtr<IMFSourceReader> sourceReader;
		unsigned int frameWidth;
		unsigned int frameHeight;
#endif
		enum class VideoFormat
		{
			YUYV,
			NV12,
			RGB,
			BGR,
		};
		VideoFormat videoFormat;

#ifdef __linux
		Camera(const int fd,const v4l2_format& format,const VideoFormat videoFormat);
#elif defined _WIN32
		Camera(ComPtr<IMFSourceReader> sourceReader,const unsigned int frameWidth,const unsigned int frameHeight,const VideoFormat videoFormat);
#endif
		Camera(const Camera&)=delete;
		Camera& operator=(Camera&)=delete;
};

#endif

