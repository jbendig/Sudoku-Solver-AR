#include "Camera.h"
#include <cassert>
#include <cstring>
#ifdef __linux
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif
#include "Image.h"

template <class T>
static T Clamp(const T value,const T minimum,const T maximum)
{
	return std::min(std::max(value,minimum),maximum);
}

static void YUYVToRGB(const std::vector<unsigned char>& yuyvData,Image& frame)
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

static void YUYVToGreyscale(const std::vector<unsigned char>& yuyvData,Image& frame)
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

#ifdef __linux
static bool CaptureAndPrepareFrame(const int fd,const v4l2_format& format,std::vector<unsigned char>& buffer,Image& frame)
{
	assert(buffer.size() == format.fmt.pix.sizeimage);

	const ssize_t bytesRead = read(fd,&buffer[0],buffer.size());
	if(bytesRead == -1)
		return false;

	frame.width = format.fmt.pix.width;
	frame.height = format.fmt.pix.height;
	frame.data.resize(frame.width * frame.height * 3);

	return true;
}
#endif

Camera::Camera(Camera&& other)
{
#ifdef __linux
	fd = other.fd;
	format = other.format;
	other.fd = -1;
#endif
	std::swap(buffer,other.buffer);
}

Camera::~Camera()
{
#ifdef __linux
	if(fd != -1)
		close(fd);
#endif
}

std::experimental::optional<Camera> Camera::Open(const std::string& devicePath)
{
#ifdef __linux
	const int fd = open(devicePath.c_str(),O_RDONLY);
	if(fd == -1)
		return {};

	v4l2_format format;
	memset(&format,0,sizeof(format));
	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	format.fmt.pix.width = 640;
	format.fmt.pix.height = 480;
	format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	format.fmt.pix.field = V4L2_FIELD_NONE;
	format.fmt.pix.bytesperline = format.fmt.pix.width * 2;
	const int formatSetupResult = ioctl(fd,VIDIOC_S_FMT,&format);
	assert(formatSetupResult == 0);
	assert(format.fmt.pix.width == 640);
	assert(format.fmt.pix.height == 480);
	assert(format.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV);
	assert(format.fmt.pix.bytesperline == format.fmt.pix.width * 2);

	return Camera(fd,format);
#endif
}

bool Camera::CaptureFrameRGB(Image& frame)
{
#ifdef __linux
	if(!CaptureAndPrepareFrame(fd,format,buffer,frame))
		return false;
#endif

	YUYVToRGB(buffer,frame);

	return true;
}

bool Camera::CaptureFrameGreyscale(Image& frame)
{
#ifdef __linux
	if(!CaptureAndPrepareFrame(fd,format,buffer,frame))
		return false;
#endif

	YUYVToGreyscale(buffer,frame);

	return true;
}

#ifdef __linux
Camera::Camera(const int fd,const v4l2_format& format)
	: fd(fd),
	  format(format),
	  buffer(format.fmt.pix.sizeimage)
{
}
#endif

