#include "AudioEncoder.h"

static void startFeed(GstElement* source, guint size, AudioEncoder::CvLock * cvLock)
{
	if (cvLock->isPushSample)
		return;
	// 唤醒等待
	std::unique_lock<std::mutex> lock(cvLock->mutex_);
	cvLock->isPushSample = true;
	cvLock->cv_.notify_one();
}
static void stopFeed(GstElement* source, guint size, AudioEncoder::CvLock* cvLock)
{
	cvLock->isPushSample = false;
}

AudioEncoder::AudioEncoder()
	: Sink("audio/x-raw")
	, appsrc(NULL)
	, encoder(NULL)
	, appsink(NULL)
	, isRunThread(false)
	, isInitialize(false)
	, frameSize(20)
	, mainThread(nullptr)
{}

AudioEncoder::~AudioEncoder()
{
	stop();
}

bool AudioEncoder::initialize(GstElement* pipe, int bitrate)
{
	if (isInitialize) return true;

	appsrc = gst_element_factory_make("appsrc", "aud_appSrc");
	encoder = gst_element_factory_make("opusenc", "aud_opusenc");
	appsink = gst_element_factory_make("appsink", "aud_appSink");
	if (!appsrc)
	{
		printError("Failed to create appsrc");
		goto RETURN;
	}
	if (!encoder)
	{
		printError("Failed to create opusenc");
		goto RETURN;
	}
	if (!appsink)
	{
		printError("Failed to create appsink");
		goto RETURN;
	}

	g_object_set(appsrc, "format", GST_FORMAT_TIME, "max-buffers", 1, "block", false, "is-live", true, NULL);
	g_signal_connect(appsrc, "need-data", G_CALLBACK(startFeed), &cvLock);
	g_signal_connect(appsrc, "enough-data", G_CALLBACK(stopFeed), &cvLock);

	g_object_set(encoder, "bandwidth", 1105 /*fullband*/, "dtx", true, "frame-size", frameSize, "audio-type", 2051, "bitrate", bitrate * 1000, NULL);

	g_object_set(appsink, "max-buffers", 1, "drop", false, "sync", false, NULL);

	mediaType = "audio/x-opus";
	cvLock.isPushSample = false;

	gst_bin_add_many(GST_BIN(pipe), appsrc, encoder, appsink, NULL);
	if (gst_element_link_many(appsrc, encoder, appsink, NULL))
		return isInitialize = true;

	gst_bin_remove_many(GST_BIN(pipe), appsrc, encoder, appsink, NULL);

RETURN:
	releaseEncoedr();
	return false;
}

void AudioEncoder::setBitrate(int bitrate)
{
	if (encoder)
	{
		g_object_set(encoder, "bitrate", bitrate * 1000, NULL);
	}
}

GstSample* AudioEncoder::pullSample()
{
	return gst_app_sink_pull_sample(GST_APP_SINK(appsink));
}

const std::string& AudioEncoder::getMediaType() const
{
	return mediaType;
}

const int AudioEncoder::getFrameSizeMs() const
{
	return frameSize;
}

void AudioEncoder::start()
{
	if (isRunThread)
		return;
	
	stop();

	if (!mainThread)
	{
		isRunThread = true;
		mainThread = new std::thread([this] { mainLoop(); });
	}
}

void AudioEncoder::stop()
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

void AudioEncoder::releaseEncoedr()
{
	mediaType = "";
	if (appsrc)
	{
		gst_object_unref(appsrc);
		appsrc = NULL;
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

void AudioEncoder::mainLoop()
{
	GstFlowReturn ret = GST_FLOW_OK;
	GstSample* sample = NULL;

	while (isRunThread)
	{
		if (cvLock.isPushSample)
		{
			GstSample* sample = Sink::source->pullSample();

			ret = gst_app_src_push_sample(GST_APP_SRC(appsrc), sample);
			gst_sample_unref(sample);

			if (ret != GST_FLOW_OK) break;
		}
		else // 不需要数据时等待
		{
			std::unique_lock<std::mutex> lock(cvLock.mutex_);
			while (!cvLock.isPushSample && isRunThread)
				cvLock.cv_.wait(lock);
		}
	}

	isRunThread = false;
}

bool AudioEncoder::printError(const std::string& er)
{
	Debug::PrintError("[AudioEncoder]: ", er);
	return false;
}