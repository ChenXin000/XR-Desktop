#include "VideoEncoder.h"

static void startFeed(GstElement* source, guint size, VideoEncoder::CvLock* cvLock)
{
	if (cvLock->isPushSample)
		return;
	// 唤醒等待
	std::unique_lock<std::mutex> lock(cvLock->mutex_);
	cvLock->isPushSample = true;
	cvLock->cv_.notify_one();
}
static void stopFeed(GstElement* source, guint size, VideoEncoder::CvLock* cvLock)
{
	cvLock->isPushSample = false;
}

VideoEncoder::VideoEncoder()
	: Sink("video/x-raw")
	, appsrc(NULL)
	, convert(NULL)
	, convertCapsf(NULL)
	, encoder(NULL)
	, appsink(NULL)
	, encType(NONE)
	, isRunThread(false)
	, isInitialize(false)
	, mainThread(nullptr)
{
}

VideoEncoder::~VideoEncoder()
{
	stop();
}
// 设置比特率
void VideoEncoder::setBitrate(int bitrate)
{
	if (encType == NONE)
		return;
	if(encType == ENC_VP8)
		g_object_set(encoder, "target-bitrate", bitrate * 1000, NULL);
	else
		g_object_set(encoder, "bitrate", bitrate, NULL);
}
// 初始化编码器
bool VideoEncoder::initialize(GstElement* pipe, EncType enc, int bitrate)
{
	if (isInitialize) return true;

	appsrc = gst_element_factory_make("appsrc", "vid_appSrc");
	convert = gst_element_factory_make("videoconvert", "vid_convert");
	convertCapsf = gst_element_factory_make("capsfilter", "vid_convertCapsf");
	appsink = gst_element_factory_make("appsink", "vid_appSink");

	if (!appsrc)
	{
		printError("Failed to create appsrc");
		goto RETURN;
	}
	if (!convert)
	{
		printError("Failed to create convert");
		goto RETURN;
	}
	if (!convertCapsf)
	{
		printError("Failed to create capsfilter");
		goto RETURN;
	}
	if (!appsink)
	{
		printError("Failed to create appsink");
		goto RETURN;
	}
	g_object_set(appsrc,"format", GST_FORMAT_TIME, "max-buffers", 1, "block", false, "is-live", true, NULL);
	g_signal_connect(appsrc, "need-data", G_CALLBACK(startFeed), &cvLock);
	g_signal_connect(appsrc, "enough-data", G_CALLBACK(stopFeed), &cvLock);

	g_object_set(appsink, "max-buffers", 1, "drop", false, "sync", false, NULL);

	encoder = createEncoder(enc, bitrate);
	if (!encoder)
	{
		printError("Failed to create encoder");
		goto RETURN;
	}
	encType = enc;
	cvLock.isPushSample = false;

	gst_bin_add_many(GST_BIN(pipe), appsrc, convert, convertCapsf, encoder, appsink, NULL);
	if (gst_element_link_many(appsrc, convert, convertCapsf, encoder, appsink, NULL))
		return isInitialize = true;

	gst_bin_remove_many(GST_BIN(pipe), appsrc, convert, convertCapsf, encoder, appsink, NULL);

RETURN:

	releaseEncoedr();
	return false;
}
// 创建不同的编码器
GstElement* VideoEncoder::createEncoder(EncType enc, int bitrate)
{
	GstCaps* caps = NULL;
	GstElement* encoder = NULL;
	mediaType = "video/x-h264";
	switch (enc)
	{
	case ENC_X264:
		caps = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "NV12", NULL);
		g_object_set(convertCapsf, "caps", caps, NULL);
		gst_caps_unref(caps);

		caps = gst_caps_new_simple(
			"video/x-h264",
			"stream-format", G_TYPE_STRING, "byte-stream",
			"profile", G_TYPE_STRING, "high", NULL
		);
		g_object_set(appsink, "caps", caps, NULL);
		gst_caps_unref(caps);
		caps = NULL;

		encoder = gst_element_factory_make("x264enc", "vid_encoder");
		if (!encoder)
			goto RETURN;

		g_object_set(encoder,
			"threads", 8,
			"tune", 4,
			"speed-preset", 4,
			"aud", false,
			"b-adapt", false,
			"bframes", 0,
			"key-int-max", 25,
			"sliced-threads", true,
			"byte-stream", true,
			"bitrate", bitrate, NULL
		);
		break;

	case ENC_NVH264:
		caps = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "NV12", NULL);
		g_object_set(convertCapsf, "caps", caps, NULL);
		gst_caps_unref(caps);

		caps = gst_caps_new_simple("video/x-h264", "profile", G_TYPE_STRING, "high", NULL);
		g_object_set(appsink, "caps", caps, NULL);
		gst_caps_unref(caps);
		caps = NULL;

		encoder = gst_element_factory_make("nvh264enc", "vid_encoder");
		if (!encoder)
			goto RETURN;

		g_object_set(encoder,
			"rc-mode", 5, //2: cbr, 5: cbr-ld-hq
			//"rc-mode", 3, // vbr
			"gop-size", 25,
			"aud", false,
			"bframes", 0,
			"qos", true,
			"preset", 4, // low-latency-hq
			"bitrate", bitrate, NULL
		);
		break;

	case ENC_VP8:
		caps = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "I420", NULL);
		g_object_set(convertCapsf, "caps", caps, NULL);
		gst_caps_unref(caps);

		mediaType = "video/x-vp8";
		caps = gst_caps_new_empty_simple("video/x-vp8");
		g_object_set(appsink, "caps", caps, NULL);
		gst_caps_unref(caps);
		caps = NULL;

		encoder = gst_element_factory_make("vp8enc", "vid_encoder");
		if (!encoder)
			goto RETURN;

		g_object_set(encoder,
			"threads", 4,
			"cpu-used", 12,
			"deadline", 1,
			"error-resilient", 2, // partitions    - Allow partitions to be decoded independently
			"keyframe-max-dist", 30,
			"auto-alt-ref", true,
			"target-bitrate", bitrate * 1000, NULL
		);
		break;
	}
	return encoder;

RETURN:
	if (caps)
	{
		gst_caps_unref(caps);
		caps = NULL;
	}
	return nullptr;
}
// 释放编码器
void VideoEncoder::releaseEncoedr()
{
	mediaType = "";
	encType = NONE;
	if (appsrc)
	{
		gst_object_unref(appsrc);
		appsrc = NULL;
	}
	if (convert)
	{
		gst_object_unref(convert);
		convert = NULL;
	}
	if (convertCapsf)
	{
		gst_object_unref(convertCapsf);
		convertCapsf = NULL;
	}
	if (encoder)
	{
		gst_object_unref(encoder);
		encoder = NULL;
	}
	if (appsink)
	{
		gst_object_unref(appsink);
		appsink = NULL;
	}
}

void VideoEncoder::start()
{
	if (isRunThread)
		return;
	
	stop();
	isRunThread = true;
	mainThread = new std::thread([this] { mainLoop(); });
}
void VideoEncoder::stop()
{
	isRunThread = false;
	if (mainThread)
	{
		cvLock.mutex_.lock();
		cvLock.cv_.notify_one();
		cvLock.mutex_.unlock();

		mainThread->join();
		delete mainThread;
		mainThread = nullptr;
	}
}
// 拉取编码后的数据
GstSample* VideoEncoder::pullSample()
{
	return gst_app_sink_pull_sample(GST_APP_SINK(appsink));
}
// 获取编码输出类型
const std::string& VideoEncoder::getMediaType() const
{
	return mediaType;
}
EncType VideoEncoder::getEncoderType() const
{
	return encType;
}
// 主循环
void VideoEncoder::mainLoop()
{
	GstFlowReturn ret = GST_FLOW_OK;
	GstSample* sample = NULL;

	while (isRunThread)
	{
		if (cvLock.isPushSample)
		{
			sample = Sink::source->pullSample();

			ret = gst_app_src_push_sample(GST_APP_SRC(appsrc), sample);
			gst_sample_unref(sample);

			if (ret != GST_FLOW_OK) break;
		}
		else  // 不需要数据时等待
		{
			std::unique_lock<std::mutex> lock(cvLock.mutex_);
			while (!cvLock.isPushSample && isRunThread)
				cvLock.cv_.wait(lock);
		}
	}
	isRunThread = false;
}

bool VideoEncoder::printError(const std::string& er)
{
	Debug::PrintError("[VideoEncoder]: ", er);
	return false;
}