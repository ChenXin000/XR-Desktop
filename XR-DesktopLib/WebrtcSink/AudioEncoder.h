#ifndef AUDIO_ENCODER_H
#define AUDIO_ENCODER_H

#include <string>
#include <thread>

#include "../Sink.h"
#include "gst/gst.h"
#include "gst/app/gstappsrc.h"
#include "gst/app/gstappsink.h"
#include <mutex>
#include <condition_variable>

#include "Debug.h"

class AudioEncoder : public Sink
{
public:
	struct CvLock
	{
		bool isPushSample = false;
		std::mutex mutex_;
		std::condition_variable cv_;
	};

public:
	explicit AudioEncoder();
	~AudioEncoder();

	void setBitrate(int bitrate);

private:
	bool isInitialize;
	bool isRunThread;
	int frameSize;

	GstElement* appsrc;
	GstElement* encoder;
	GstElement* appsink;

	std::thread* mainThread;

	std::string mediaType;
	CvLock cvLock;

protected:
	bool initialize(GstElement* pipe, int bitrate);

	GstSample* pullSample();
	const std::string& getMediaType() const;
	const int getFrameSizeMs() const;

	void start();
	void stop();

private:
	void releaseEncoedr();
	void mainLoop();
	bool printError(const std::string& er);
};

#endif // !AUDIO_ENCODER_H

