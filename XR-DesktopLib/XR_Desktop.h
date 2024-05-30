#ifndef XR_DESKTOP_H
#define XR_DESKTOP_H

#include "MediaSource/VideoSource.h"
#include "MediaSource/AudioSource.h"
#include "WebrtcSink/WebrtcSink.h"
#include "Input/InputManager.h"

class XR_Desktop
	: public AudioSource
	, public VideoSource
	, public WebrtcSink
{
public:
	explicit XR_Desktop(int dispIndex = 0);
	virtual ~XR_Desktop();

	bool initialize(EncType enc = ENC_X264, int vidBitrate = 4000, int audBitrate = 256);
	void destroy();

	bool start();
	void stop();

	static int getDispSum();

private:
	int dispIndex;
	bool isInitialize;
};
// 屏幕id
inline XR_Desktop::XR_Desktop(int dispIndex)
	: WebrtcSink()
	, dispIndex(dispIndex)
	, isInitialize(false)
{}

inline XR_Desktop::~XR_Desktop()
{
	destroy();
}
// 初始化 编码器类型、视频比特率、音频比特率
inline bool XR_Desktop::initialize(EncType enc, int vidBitrate, int audBitrate)
{
	if (!VideoSource::initialize(dispIndex))
		return false;
	if (!AudioSource::initialize())
		return false;
	if (!WebrtcSink::initialize())
		return false;

	if (!WebrtcSink::linkVideoSource(VideoSource::getSource(), enc, vidBitrate))
		return false;
	if (!WebrtcSink::linkAudioSource(AudioSource::getSource(), audBitrate))
		return false;

	return isInitialize = true;
}

inline void XR_Desktop::destroy()
{
	stop();
}

inline bool XR_Desktop::start()
{
	if (!isInitialize) return false;

	if (!VideoSource::start())
		return false;
	if (!AudioSource::start())
		return false;
	if (!WebrtcSink::start())
		return false;

	return true;
}

inline void XR_Desktop::stop()
{
	if (isInitialize)
	{
		WebrtcSink::stop();
		VideoSource::stop();
		AudioSource::stop();
	}
}

inline int XR_Desktop::getDispSum()
{
	return DX::GetMonitors().size();
}

#endif // !XR_DESKTOP_H

