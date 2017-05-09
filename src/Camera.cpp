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

#include "Camera.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#ifdef __linux
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#elif defined _WIN32
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mfapi.h>
#endif
#include "Image.h"
#include "ImageProcessing.h"

#ifdef __linux
template <void(*ProcessFunc)(const std::vector<unsigned char>&,Image&)>
static bool CaptureAndProcessFrame(const int fd,const v4l2_format& format,std::vector<unsigned char>& buffer,Image& frame)
{
	assert(buffer.size() == format.fmt.pix.sizeimage);

	const ssize_t bytesRead = read(fd,&buffer[0],buffer.size());
	if(bytesRead == -1)
		return false;

	frame.width = format.fmt.pix.width;
	frame.height = format.fmt.pix.height;
	frame.data.resize(frame.width * frame.height * 3);

	ProcessFunc(buffer,frame);

	return true;
}
#elif defined _WIN32
template <void(*ProcessFunc)(const BYTE*,Image&)>
static bool CaptureAndProcessFrame(const ComPtr<IMFSourceReader>& sourceReader,const unsigned int frameWidth,const unsigned int frameHeight,Image& frame)
{
	ComPtr<IMFSample> sample = NullComPtr<IMFSample>();
	while(sample == nullptr)
	{
		IMFSample* ptr = nullptr;
		DWORD streamFlags = 0; //Most be passed to ReadSample or it'll fail.
		TRY_COM_BOOL(sourceReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM,0,nullptr,&streamFlags,nullptr,&ptr));
		sample = MakeComPtr(ptr);
	}

	ComPtr<IMFMediaBuffer> buffer = NullComPtr<IMFMediaBuffer>();
	{
		IMFMediaBuffer* ptr = nullptr;
		TRY_COM_BOOL(sample->GetBufferByIndex(0,&ptr));
		buffer = MakeComPtr(ptr);
	}

	BYTE* bufferData = nullptr;
	DWORD bufferDataLength = 0;
	TRY_COM_BOOL(buffer->Lock(&bufferData,nullptr,&bufferDataLength));

	frame.width = frameWidth;
	frame.height = frameHeight;
	frame.data.resize(frame.width * frame.height * 3);
	ProcessFunc(bufferData,frame);

	buffer->Unlock();

	return true;
}
#endif

Camera::Camera(Camera&& other)
#if defined _WIN32
	: sourceReader(std::move(other.sourceReader)),
	  frameWidth(other.frameWidth),
	  frameHeight(other.frameHeight)
#endif
{
#ifdef __linux
	fd = other.fd;
	format = other.format;
	other.fd = -1;
	std::swap(buffer,other.buffer);
#endif
}

Camera::~Camera()
{
#ifdef __linux
	if(fd != -1)
		close(fd);
#endif
}

#ifdef __linux
std::experimental::optional<Camera>
#elif defined _WIN32
std::optional<Camera>
#endif
Camera::Open(const std::string& devicePath)
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
#elif defined _WIN32
	//Find a camera to use.
	std::vector<ComPtr<IMFActivate>> devices;
	{
		IMFAttributes* attributes = nullptr;
		HRESULT hr = MFCreateAttributes(&attributes,1);
		hr = attributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
		assert(SUCCEEDED(hr));
		UINT32 deviceCount = 0;
		IMFActivate** devicesPtrs = nullptr;
		hr = MFEnumDeviceSources(attributes,&devicesPtrs,&deviceCount);
		for(UINT32 x = 0;x < deviceCount;x++)
		{
			devices.push_back(MakeComPtr(devicesPtrs[x]));
		}
		attributes->Release();

		if(deviceCount == 0 || hr != S_OK)
			return {};
	}
	ComPtr<IMFActivate>& activate = devices[0]; //TODO: Camera should be selectable.

	//Setup reader to read from camera.
	ComPtr<IMFMediaSource> mediaSource = NullComPtr<IMFMediaSource>();
	{
		IMFMediaSource* ptr = nullptr;
		TRY_COM(activate->ActivateObject(__uuidof(IMFMediaSource),reinterpret_cast<void**>(&ptr)));
		mediaSource = MakeComPtr(ptr);
	}

	ComPtr<IMFSourceReader> sourceReader = NullComPtr<IMFSourceReader>();
	{
		IMFSourceReader* ptr = nullptr;
		TRY_COM(MFCreateSourceReaderFromMediaSource(mediaSource.get(),nullptr,&ptr));
		sourceReader = MakeComPtr(ptr);
	}

	//Collect camera's default settings so we can query and re-use most of them.
	IMFMediaType* mediaType = nullptr;
	TRY_COM(sourceReader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,0,&mediaType));

	GUID nativeType; //Fetched for debug reasons but not used.
	TRY_COM(mediaType->GetGUID(MF_MT_SUBTYPE,&nativeType));
	UINT32 frameWidth = 0;
	UINT32 frameHeight = 0;
	TRY_COM(MFGetAttributeSize(mediaType,MF_MT_FRAME_SIZE,&frameWidth,&frameHeight));
	UINT32 frameRateNumerator = 0; //Fetched for debug reasons but not used.
	UINT32 frameRateDenominator = 0; //Fetched for debug reasons but not used.
	TRY_COM(MFGetAttributeRatio(mediaType,MF_MT_FRAME_RATE,&frameRateNumerator,&frameRateDenominator));

	//Try to force NV12 format.
	//TODO: Support other formats, this just happens to be what my camera uses.
	TRY_COM(mediaType->SetGUID(MF_MT_SUBTYPE,MFVideoFormat_NV12));
	TRY_COM(sourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,nullptr,mediaType));

	return Camera(std::move(sourceReader),frameWidth,frameHeight);
#endif
}

bool Camera::CaptureFrameRGB(Image& frame)
{
#ifdef __linux
	return CaptureAndProcessFrame<YUYVToRGB>(fd,format,buffer,frame);
#elif defined _WIN32
	return CaptureAndProcessFrame<NV12ToRGB>(sourceReader,frameWidth,frameHeight,frame);
#endif
}

bool Camera::CaptureFrameGreyscale(Image& frame)
{
#ifdef __linux
	return CaptureAndProcessFrame<YUYVToGreyscale>(fd,format,buffer,frame);
#elif defined _WIN32
	return CaptureAndProcessFrame<NV12ToGreyscale>(sourceReader,frameWidth,frameHeight,frame);
#endif
}

#ifdef __linux
Camera::Camera(const int fd,const v4l2_format& format)
	: fd(fd),
	  format(format),
	  buffer(format.fmt.pix.sizeimage)
{
}
#elif defined _WIN32
Camera::Camera(ComPtr<IMFSourceReader> sourceReader,const unsigned int frameWidth,const unsigned int frameHeight)
	: sourceReader(std::move(sourceReader)),
	  frameWidth(frameWidth),
	  frameHeight(frameHeight)
{
}
#endif

