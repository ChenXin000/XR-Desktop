#ifndef VIDEO_ENCODER_H
#define VIDEO_ENCODER_H

#include "gst/gst.h"
#include "gst/app/gstappsrc.h"
#include "gst/app/gstappsink.h"
#include "../Sink.h"
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "Debug.h"

enum EncType
{
	NONE,
	ENC_X264,
	ENC_NVH264,
	ENC_VAH264,
	ENC_VP8,
	//ENC_VP9,
};

class VideoEncoder : public Sink
{
public:
	struct CvLock
	{
		bool isPushSample = false;
		std::mutex mutex_;
		std::condition_variable cv_;
	};

public:
	explicit VideoEncoder();
	~VideoEncoder();

	void setBitrate(int bitrate);

private:
	GstElement* appsrc;
	GstElement* convert;
	GstElement* convertCapsf;
	GstElement* encoder;
	GstElement* appsink;

	std::thread* mainThread;

	EncType encType;

	bool isInitialize;
	bool isRunThread;
	std::string mediaType;
	CvLock cvLock;

protected:
	bool initialize(GstElement* pipe, EncType enc, int bitrate);

	GstSample* pullSample();
	const std::string& getMediaType() const;
	EncType getEncoderType() const;

	void start();
	void stop();

private:
	GstElement* createEncoder(EncType enc, int bitrate);
	void releaseEncoedr();
	void mainLoop();
	bool printError(const std::string& er);
};

#endif // !VIDEO_ENCODER_H
