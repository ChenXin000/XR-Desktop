#ifndef SCREEN_SOURCE_H
#define SCREEN_SOURCE_H

#include "gst/gst.h"
#include "../Source.h"

#include "../Capture/ScreenCapture/DXGI/DXGIScreenCapture.h"
#include "../Capture/ScreenCapture/ScreenCapture.h"

class VideoSource : public Source
{
public:
	explicit VideoSource();
	~VideoSource();

	bool initialize(int dispIndex);
	void destroy();

public:
	bool start();
	void stop();

	void setFramerate(int fps, bool isFixed);
	int GetRealFramerate() const;

private:
	bool isStarted;
	bool isFirstPull;
	bool isInitialized;
	ScreenCapture* capture;
	GstCaps* caps;

	std::chrono::steady_clock::time_point beginTime;

private:
	GstSample* pullSample() override;
};

#endif // ! SCREEN_SOURCE_H
