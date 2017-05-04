#ifndef CAMERA_H
#define CAMERA_H

#include <vector>
#include <experimental/optional>
#ifdef __linux
#include <linux/videodev2.h>
#else
#error "Platform not supported"
#endif

class Image;

class Camera
{
	public:
		Camera(Camera&& other);
		~Camera();

		static std::experimental::optional<Camera> Open(const std::string& devicePath);
		//TODO: Support camera enumeration.

		bool CaptureFrameRGB(Image& frame);
		bool CaptureFrameGreyscale(Image& frame);
	private:
#ifdef __linux
		int fd;
		v4l2_format format;
#endif
		std::vector<unsigned char> buffer;

#ifdef __linux
		Camera(const int fd,const v4l2_format& format);
#endif
		Camera(const Camera&)=delete;
		Camera& operator=(Camera&)=delete;
};

#endif

