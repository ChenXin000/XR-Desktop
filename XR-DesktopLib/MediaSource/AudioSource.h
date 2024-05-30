#ifndef AUDIO_SOURCE_H
#define AUDIO_SOURCE_H

#include "gst/gst.h"
#include "../Capture/AudioCapture/WASAPICapture.h"
#include "../Source.h"

class AudioSource : public Source
{
public:
	explicit AudioSource();
	~AudioSource();
	
	bool initialize();
	void destroy();

	bool start();
	void stop();

private:
	bool isInitialized;
	bool isStarted;
	WASAPICapture capture;
	GstCaps* caps;

private:
	GstSample* pullSample() override;
};

#endif // !AUDIO_SOURCE_H

