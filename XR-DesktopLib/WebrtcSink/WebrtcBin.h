#ifndef WEBRTC_BIN_H
#define WEBRTC_BIN_H

#define GST_USE_UNSTABLE_API

#include "gst/gst.h"
#include "gst/app/gstappsrc.h"
#include "gst/app/gstappsink.h"
#include "gst/webrtc/webrtc.h"

#include "../Debug.h"

#include "Signalling.h"

#include <functional>
#include <string>
#include <mutex>
#include <vector>
#include <atomic>

class WebrtcBin
{
private:
	explicit WebrtcBin(Signalling* signal);
	~WebrtcBin();

private:
	guint busWatchId;
	std::atomic<int> refcount;

	bool isConnected;
	bool isClosePipeline;
	bool isFirstVid;
	bool isFirstAud;
	bool isPushVidSample;
	bool isPushAudSample;

	std::chrono::steady_clock::time_point timeOutPoint;  // 记录超时时间点
	std::chrono::steady_clock::time_point pingTimePoint; // 记录 ping 时间点

	Signalling* signalling;

	GstElement* pipeline;
	GstElement* webrtc;
	GstWebRTCDataChannel* dataChannel;

	GstElement* vidAppsrc;
	GstPad* videoSinkPad;
	std::chrono::steady_clock::time_point vidBeginTime;

	GstElement* audAppsrc;
	GstPad* audioSinkPad;
	GstClockTime audioPTS;
	std::chrono::steady_clock::time_point audBeginTime;

	std::function<void(WebrtcBin* ,std::string& msg)> channelMessageCb;
	std::function<void(WebrtcBin*)> channelOpenCb;
	std::function<void(WebrtcBin*)> channelCloseCb;

private:
	static void on_offer_created_cb(GstPromise* promise, gpointer user_data);
	static void on_negotiation_needed_cb(GstElement* webrtc, WebrtcBin* webrtcBin);
	static void on_ice_candidate_cb(G_GNUC_UNUSED GstElement* webrtc, guint mline_index, gchar* candidate, WebrtcBin* webrtcBin);
	// 内部数据通道事件回调
	static void data_channel_on_close(GstWebRTCDataChannel* self, WebrtcBin* webrtcBin);
	static void data_channel_on_error(GstWebRTCDataChannel* self, gchar* err, WebrtcBin* webrtcBin);
	static void data_channel_on_open(GstWebRTCDataChannel* self, WebrtcBin* webrtcBin);
	static void data_channel_on_message_string(GstWebRTCDataChannel* self, gchar* msg, WebrtcBin* webrtcBin);

	bool initialize(const std::string& vidType, const std::string& audType);
	bool linkPipeline(const std::string& mediaType);
	bool createDataChannel();
	void release();

	// 设置媒体描述，ice
	void setLocalDescription(GstWebRTCSessionDescription* offer);
	void setRemoteDescription(std::string& sdpStr);
	void addIceCandidate(gint sdpmlineindex, std::string& candidate);

	void sendDescription(std::string& sdp);
	void sendIceCandidate(gint sdpmlineindex, std::string& candidate);

	void sendPing();

public:
	static WebrtcBin* createWebrtcBin(Signalling* signal, const std::string& vidType, const std::string& audType);

	void addRef();
	void unRef();

	void sendChannelMessage(const std::string& msg);
	void onChannelMessage(std::function<void(WebrtcBin*, std::string&)>& cb);
	// 禁止在数据通道关闭时释放 WebrtcBin，否则会卡死
	void onChannelClose(std::function<void(WebrtcBin*)>& cb);
	void onChannelOpen(std::function<void(WebrtcBin*)>& cb);

	// 填充媒体数据
	GstFlowReturn pushVideoSample(GstSample* sample, bool is_sps_pps);
	GstFlowReturn pushAudioSample(GstSample* sample, GstClockTime frameSizeNano);

	void closePipeline();
	void closeSignalling();

	bool printError(const std::string& er);
};

#endif // !WEBRTC_BIN_H
