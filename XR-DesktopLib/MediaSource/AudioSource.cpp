#include "AudioSource.h"


AudioSource::AudioSource() 
	: Source("audio/x-raw")
	, caps(NULL)
	, isStarted(false)
	, isInitialized(false)
{}

AudioSource::~AudioSource()
{
	destroy();
}

bool AudioSource::initialize()
{
	if (!capture.Initialize())
	{
		return false;
	}

	caps = gst_caps_new_simple(
		"audio/x-raw",
		"format", G_TYPE_STRING, "S16LE",
		"layout", G_TYPE_STRING, "interleaved",
		"rate", G_TYPE_INT, capture.GetSamplesPerSec(),
		"channels", G_TYPE_INT, capture.GetChannels(), NULL
	);
	return isInitialized = true;
}

void AudioSource::destroy()
{
	stop();

	if (caps)
	{
		gst_caps_unref(caps);
		caps = nullptr;
	}
}

bool AudioSource::start()
{
	if (!isInitialized) return false;
	if (isStarted) return true;

	isStarted = true;
	return capture.Start();
}

void AudioSource::stop()
{
	if(isStarted) capture.Stop();
	isStarted = false;
}

GstSample* AudioSource::pullSample()
{
	GstBuffer* buffer = NULL;
	GstSample* sample = NULL;
	GstMapInfo map;
	BYTE* frame = nullptr;

	int len = capture.CaptureFrame(&frame);

	if (len > 0)
	{
		buffer = gst_buffer_new_allocate(NULL, len, NULL);
		gst_buffer_map(buffer, &map, GST_MAP_WRITE);
		memcpy(map.data, frame, map.size);
		capture.ReleaseBuffer();

		sample = gst_sample_new(buffer, caps, NULL, NULL);
	}
	else // Ä¬ÈÏÌî³ä¾²ÒôÊý¾Ý
	{
		buffer = gst_buffer_new_allocate(NULL, 1920, NULL);
		gst_buffer_map(buffer, &map, GST_MAP_WRITE);
		memset(map.data, 0, map.size);

		sample = gst_sample_new(buffer, caps, NULL, NULL);
	}

	GST_BUFFER_PTS(buffer) = 0;
	gst_buffer_unmap(buffer, &map);
	gst_buffer_unref(buffer);

	return sample;
}