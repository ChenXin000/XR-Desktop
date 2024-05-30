#ifndef WEBRTC_SINK_H
#define WEBRTC_SINK_H

#include "../DLinkList.h"
#include "VideoEncoder.h"
#include "AudioEncoder.h"
#include "WebrtcBin.h"

class WebrtcSink : public AudioEncoder , public VideoEncoder
{
public:
	explicit WebrtcSink();
	~WebrtcSink();

	bool initialize();
	void destroy();

private:
	guint busWatchId;
	bool isStarted;
	bool isRunVidLoop;
	bool isRunAudLoop;

	GMainLoop* mainLoop;
	std::thread* mainThread;
	GstElement* pipeline;

	std::thread* videoThread;
	DLinkList<WebrtcBin*> vidBinList;
	VideoEncoder::CvLock vidCvLock;

	std::thread* audioThread;
	DLinkList<WebrtcBin*> audBinList;
	AudioEncoder::CvLock audCvLock;

	std::function<void(WebrtcBin*, std::string& msg)> channelMessageCb;
	std::function<void(WebrtcBin*)> channelOpenCb;
	std::function<void(WebrtcBin*)> channelCloseCb;

public:
	int newConnect(Signalling* signal, bool vidFlag = true, bool audFlag = true);

	bool linkVideoSource(Source& vidSrc, EncType enc = ENC_X264, int bitrate = 4000);
	bool linkAudioSource(Source& audSrc, int bitrate = 256);

	void onChannelMessage(std::function<void(WebrtcBin*, std::string&)> cb);
	// 禁止在数据通道关闭时释放 WebrtcBin，否则会卡死
	void onChannelClose(std::function<void(WebrtcBin*)> cb);
	void onChannelOpen(std::function<void(WebrtcBin*)> cb);

	bool start();
	void stop();

private:
	void addVideoBin(WebrtcBin* bin);
	void addAudioBin(WebrtcBin* bin);

	static gboolean delWebrtcbin(WebrtcBin* webrtcBin);
	void delAllWebrtcBin();

	void pushVidLoop();
	void pushAudLoop();

	bool printError(const std::string& er);
};

#endif // !WEBRTC_SINK_H

