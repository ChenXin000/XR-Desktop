#include "VideoSource.h"

VideoSource::VideoSource()
	: Source("video/x-raw")
	, caps(NULL)
	, isStarted(false)
	, isFirstPull(true)
	, capture(nullptr)
	, isInitialized(false)
{}

VideoSource::~VideoSource()
{
	destroy();
}

bool VideoSource::initialize(int dispIndex)
{
	capture = new DXGIScreenCapture();

	if (!capture->Initialize(dispIndex))
	{
		return false;
	}

	caps = gst_caps_new_simple(
		"video/x-raw",
		"width", G_TYPE_INT, capture->GetWidth(),
		"height", G_TYPE_INT, capture->GetHeight(),
		"format", G_TYPE_STRING, capture->GetPixelFormat().c_str(),
		"framerate", GST_TYPE_FRACTION, 30, 1, NULL
	);
	return isInitialized = true;
}

void VideoSource::destroy()
{
	stop();

	if (capture)
	{
		delete capture;
		capture = nullptr;
	}
	if (caps)
	{
		gst_caps_unref(caps);
		caps = nullptr;
	}
}
// 开始截屏
bool VideoSource::start()
{
	if (!isInitialized) return false;
	if (isStarted) return true;

	isStarted = true;
	isFirstPull = true;
	return true;
}
// 停止截屏
void VideoSource::stop()
{
	isStarted = false;
}
// 设置截屏目标帧率
void VideoSource::setFramerate(int fps, bool isFixed)
{
	if (isStarted)
		return;
	capture->SetFramerate(fps, isFixed);
	gst_caps_set_simple(caps, "framerate", GST_TYPE_FRACTION, fps, 1, NULL);
}
// 获取实时截屏帧率
int VideoSource::GetRealFramerate() const
{
	return capture->GetRealFramerate();
}
// 拉取截屏数据
GstSample* VideoSource::pullSample()
{
	GstSample* sample = NULL;
	GstMapInfo map;
	RETURN_RET ret = RETURN_SUCCESS;
	GstBuffer* buffer = gst_buffer_new_allocate(NULL, capture->GetImageSize(), NULL);
	gst_buffer_map(buffer, &map, GST_MAP_WRITE);

	ret = capture->CaptureFrame(map.data, map.size, 15);
	//if (ret == RETURN_ERROR_UNEXPECTED)
	//	goto RETURN;

	if (isFirstPull)
	{
		isFirstPull = false;
		GST_BUFFER_PTS(buffer) = 0;
		beginTime = std::chrono::steady_clock::now();
	}
	else
	{
		GST_BUFFER_PTS(buffer) = (std::chrono::steady_clock::now() - beginTime).count();
	}
	sample = gst_sample_new(buffer, caps, NULL, NULL);

	gst_buffer_unmap(buffer, &map);
	gst_buffer_unref(buffer);
	return sample;
}