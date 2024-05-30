#include "WebrtcBin.h"

static gboolean busCallback(GstBus* bus, GstMessage* message, gpointer data)
{
	WebrtcBin* webrtcBin = (WebrtcBin*)data;
	if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR)
	{
		GError* err;
		gchar* debug;

		gst_message_parse_error(message, &err, &debug);
		Debug::PrintError("WebrtcBin Error: ", err->message);
		Debug::PrintError("WebrtcBin Debug: ", debug);
		g_error_free(err);
		g_free(debug);

		webrtcBin->closePipeline();
	}
	else if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_EOS)
	{
		Debug::PrintError("WebrtcBin Error: ", "GST_MESSAGE_EOS");
		webrtcBin->closePipeline();
	}
	// return false 时自动移除管道 bus 消息回调
	return TRUE;
}

// 开始填充媒体数据
static void startFeed(GstElement* source, guint size, bool* isPushSimple)
{
	*isPushSimple = true;
}
// 停止填充媒体数据
static void stopFeed(GstElement* source, guint size, bool* isPushSimple)
{
	*isPushSimple = false;
}


WebrtcBin::WebrtcBin(Signalling* signal)
	: refcount(1)
	, isConnected(false)
	, isClosePipeline(true)
	, isPushVidSample(false)
	, isPushAudSample(false)
	, isFirstVid(true)
	, isFirstAud(true)
	, signalling(signal)
	, busWatchId(0)
	, pipeline(NULL)
	, webrtc(NULL)
	, videoSinkPad(NULL)
	, audioSinkPad(NULL)
	, vidAppsrc(NULL)
	, audAppsrc(NULL)
	, audioPTS(0)
	, dataChannel(nullptr)
	, channelMessageCb(nullptr)
	, channelOpenCb(nullptr)
	, channelCloseCb(nullptr)
{
	// 注册接收信令事件回调
	std::function<void(std::string&)> setDescription(std::bind(&WebrtcBin::setRemoteDescription, this, std::placeholders::_1));
	std::function<void(gint, std::string&)> addIceCandidate(std::bind(&WebrtcBin::addIceCandidate, this, std::placeholders::_1, std::placeholders::_2));
	signalling->onReadDescription(setDescription);
	signalling->onReadIceCandidate(addIceCandidate);
}

WebrtcBin::~WebrtcBin()
{
	release();
}

// 创建 offer
void WebrtcBin::on_offer_created_cb(GstPromise* promise, gpointer user_data)
{
	GstWebRTCSessionDescription* offer = NULL;
	WebrtcBin* webrtcBin = (WebrtcBin*)user_data;

	const GstStructure* reply = gst_promise_get_reply(promise);
	gst_structure_get(reply, "offer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &offer, NULL);
	gst_promise_unref(promise);
	if (!offer) return;

	webrtcBin->setLocalDescription(offer);

	gchar* sdp_string = gst_sdp_message_as_text(offer->sdp);
	std::string sdpStr(sdp_string);
	g_free(sdp_string);
	gst_webrtc_session_description_free(offer);

	size_t len = sdpStr.find("apt=");
	if (len != std::string::npos)
	{
		len = sdpStr.find('\r', len);
		sdpStr.insert(len, ";rtx-time=125");
	}

	len = sdpStr.find("H264");
	if (len != std::string::npos)
	{
		len = sdpStr.find("packetization-mode");
		if (len != std::string::npos)
		{
			len = sdpStr.find(';', len);
			sdpStr.insert(len + 1, "level-asymmetry-allowed=1;profile-level-id=42e01f;");
		}
	}

	webrtcBin->sendDescription(sdpStr);
}
// 开始创建 offer
void WebrtcBin::on_negotiation_needed_cb(GstElement* webrtc, WebrtcBin* webrtcBin)
{
	//需要等待一下，否则sdp 没有编码器描述（sprop-parameter-sets=Z2QAKqwrQDwBE/LgLUBAQFAAAD6AAB1MCEA=,aO48sA==）
	std::this_thread::sleep_for(std::chrono::milliseconds(1));
	GstPromise* promise = gst_promise_new_with_change_func(on_offer_created_cb, webrtcBin, NULL);
	g_signal_emit_by_name(G_OBJECT(webrtc), "create-offer", NULL, promise);
}
// 本地ICE
void WebrtcBin::on_ice_candidate_cb(G_GNUC_UNUSED GstElement* webrtc, guint mline_index, gchar* candidate, WebrtcBin* webrtcBin)
{
	std::string c_str(candidate);
	webrtcBin->sendIceCandidate(mline_index, c_str);
}
// 数据通道关闭事件回调
void WebrtcBin::data_channel_on_close(GstWebRTCDataChannel* self, WebrtcBin* webrtcBin)
{
	webrtcBin->closePipeline();
	if (webrtcBin->channelCloseCb) 
		webrtcBin->channelCloseCb(webrtcBin);
	//std::cout << "data_channel_on_close " << std::endl;
}
// 数据通道错误事件回调
void WebrtcBin::data_channel_on_error(GstWebRTCDataChannel* self, gchar* err, WebrtcBin* webrtcBin)
{
	data_channel_on_close(self, webrtcBin);
	//std::cout << "data_channel_on_error" << std::endl;
}
// 数据通道开启事件回调
void WebrtcBin::data_channel_on_open(GstWebRTCDataChannel* self, WebrtcBin* webrtcBin)
{
	webrtcBin->isConnected = true;
	webrtcBin->closeSignalling();

	if (webrtcBin->channelOpenCb)
		webrtcBin->channelOpenCb(webrtcBin);
	//std::cout << "data_channel_on_open " << std::endl;
	// 更新超时时间点
	webrtcBin->timeOutPoint = std::chrono::steady_clock::now() + std::chrono::seconds(4);
}
// 数据通道字符消息事件回调
void WebrtcBin::data_channel_on_message_string(GstWebRTCDataChannel* self, gchar* msg, WebrtcBin* webrtcBin)
{
	std::string str(msg);
	if (str == "pong")
	{
		// ping 响应，重置超时时间点
		webrtcBin->timeOutPoint = std::chrono::steady_clock::now() + std::chrono::seconds(4);
		return;
	}

	if (webrtcBin->channelMessageCb) 
		webrtcBin->channelMessageCb(webrtcBin, str);
}
// 创建 webrtcBin
WebrtcBin* WebrtcBin::createWebrtcBin(Signalling* signal, const std::string& vidType, const std::string& audType)
{
	WebrtcBin* webrtcBin = new WebrtcBin(signal);
	if (webrtcBin->initialize(vidType, audType))
		return webrtcBin;

	delete webrtcBin;
	return nullptr;
}

bool WebrtcBin::initialize(const std::string& vidType, const std::string& audType)
{
	if (vidType == "" && audType == "") return false;
	GArray* transceivers;
	bool vidFlag = false;

	pipeline = gst_pipeline_new("webrtcBinPipeline");
	webrtc = gst_element_factory_make("webrtcbin", "webrtcBin");

	if (!pipeline)
		return printError("Failed to create pipeline.");
	if (!webrtc)
		return printError("Failed to create webrtcbin.");
	if (!gst_bin_add(GST_BIN(pipeline), webrtc))
	{
		gst_object_unref(webrtc);
		return printError("Failed to add pipeline webrtcbin.");
	}

	if (vidType != "")
	{
		if(!linkPipeline(vidType))
			return printError("Failed to link Video webrtcbin.");
		vidFlag = true;
	}
	if (audType != "")
	{
		if (!linkPipeline(audType))
			return printError("Failed to link Audio webrtcbin.");
	}

	GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
	busWatchId = gst_bus_add_watch(bus, busCallback, this);
	gst_object_unref(bus);

	g_signal_connect(webrtc, "on-negotiation-needed", G_CALLBACK(on_negotiation_needed_cb), this);
	g_signal_connect(webrtc, "on-ice-candidate", G_CALLBACK(on_ice_candidate_cb), this);

	g_object_set(webrtc, "stun-server", "stun://stun.l.google.com:19302", "latency", 1, "bundle-policy", GST_WEBRTC_BUNDLE_POLICY_MAX_COMPAT, NULL);
	g_signal_emit_by_name(webrtc, "get-transceivers", &transceivers);
	if (!transceivers) return printError("Failed to get transceivers.");

	for (guint i = 0; i < transceivers->len; i++)
	{
		GstWebRTCRTPTransceiver* trans = g_array_index(transceivers, GstWebRTCRTPTransceiver*, i);;
		bool isNack = (!i && vidFlag);
		// 给视频传输添加 nack
		g_object_set(trans, "direction", GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY, "do-nack", isNack, NULL);

		if (i) continue; // 一个 webrtc 只创建一条数据通道
		if (!createDataChannel())
		{
			g_array_unref(transceivers);
			return printError("Failed to create data channel.");
		}
	}
	g_array_unref(transceivers);

	if (gst_element_set_state(pipeline, GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE)
	{
		isClosePipeline = false;
		pingTimePoint = timeOutPoint = std::chrono::steady_clock::now() + std::chrono::milliseconds(1500);
		return true;
	}
	return false;
}
// 连接管道
bool WebrtcBin::linkPipeline(const std::string& mediaType)
{
	GstPad* srcPad = NULL;
	GstCaps* caps = NULL;
	GstElement* appsrc = NULL;
	GstElement* rtppay = NULL;
	GstElement* rtpCapsFilter = NULL;
	bool* isPushSimple = nullptr;

	rtpCapsFilter = gst_element_factory_make("capsfilter", (mediaType + "_caps").c_str());
	appsrc = gst_element_factory_make("appsrc", (mediaType + "_appsrc").c_str());

	caps = gst_caps_new_simple(
		"application/x-rtp",
		"media", G_TYPE_STRING, "video",
		"encoding-name", G_TYPE_STRING, "H264",
		"payload", G_TYPE_INT, 123, NULL
	);

	if (mediaType == "video/x-h264")
	{
		gst_caps_set_simple(caps,
			"rtcp-fd-nack-pli", G_TYPE_BOOLEAN, true,
			"rtcp-fd-ccm-fir", G_TYPE_BOOLEAN, true,
			"rtcp-fb-x-gstreamer-fir-as-repair", G_TYPE_BOOLEAN, true, NULL
		);
		rtppay = gst_element_factory_make("rtph264pay", "rtph264pay_0");
		g_object_set(rtppay, /*"config-interval", -1,*/ "aggregate-mode", 1, NULL);
	}
	else if (mediaType == "video/x-vp8")
	{
		gst_caps_set_simple(caps, "encoding-name", G_TYPE_STRING, "VP8", NULL);
		rtppay = gst_element_factory_make("rtpvp8pay", "rtpvp8pay_0");
	}
	else if (mediaType == "audio/x-opus")
	{
		gst_caps_set_simple(caps,
			"media", G_TYPE_STRING, "audio",
			"encoding-name", G_TYPE_STRING, "OPUS",
			"payload", G_TYPE_INT, 96, NULL
		);
		rtppay = gst_element_factory_make("rtpopuspay", "rtpopuspay_0");
		g_object_set(rtppay, "dtx", true, NULL);
	}

	if (!appsrc || !rtpCapsFilter || !rtppay)
		goto RETURN;

	g_object_set(rtpCapsFilter, "caps", caps, NULL);
	gst_caps_unref(caps);

	gst_bin_add_many(GST_BIN(pipeline), appsrc, rtppay, rtpCapsFilter, NULL);
	if (!gst_element_link_many(appsrc, rtppay, rtpCapsFilter, NULL))
		return false;

	srcPad = gst_element_get_static_pad(rtpCapsFilter, "src");
	if (!mediaType.compare(0, 5, "video")) // video
	{
		videoSinkPad = gst_element_request_pad_simple(webrtc, "sink_%u");
		if (gst_pad_link(srcPad, videoSinkPad) != GST_PAD_LINK_OK)
		{
			gst_object_unref(srcPad);
			return false;
		}
		vidAppsrc = appsrc;
		isPushSimple = &isPushVidSample;
	}
	else // audio
	{
		audioSinkPad = gst_element_request_pad_simple(webrtc, "sink_%u");
		if (gst_pad_link(srcPad, audioSinkPad) != GST_PAD_LINK_OK)
		{
			gst_object_unref(srcPad);
			return false;
		}
		audAppsrc = appsrc;
		isPushSimple = &isPushAudSample;
	}
	gst_object_unref(srcPad);

	g_object_set(appsrc, "format", GST_FORMAT_TIME, "max-buffers", 1, "block", false, "is-live", true, NULL);
	g_signal_connect(appsrc, "need-data", G_CALLBACK(startFeed), isPushSimple);
	g_signal_connect(appsrc, "enough-data", G_CALLBACK(stopFeed), isPushSimple);

	return true;

RETURN:
	if (caps)
	{
		gst_caps_unref(caps);
		caps = NULL;
	}
	if (rtpCapsFilter)
	{
		gst_object_unref(rtpCapsFilter);
		rtpCapsFilter = NULL;
	}
	if (appsrc)
	{
		gst_object_unref(appsrc);
		appsrc = NULL;
	}
	if (rtppay)
	{
		gst_object_unref(rtppay);
		rtppay = NULL;
	}
	if (srcPad)
	{
		gst_object_unref(srcPad);
		srcPad = NULL;
	}
	return false;
}
// 创建数据通道
bool WebrtcBin::createDataChannel()
{
	gst_element_set_state(pipeline, GST_STATE_READY);

	GstStructure* opt = gst_structure_new_empty("application/data-channel");
	GValue val = G_VALUE_INIT;

	g_value_init(&val, G_TYPE_BOOLEAN);
	g_value_set_boolean(&val, TRUE);
	gst_structure_set_value(opt, "ordered", &val);
	g_value_unset(&val);

	g_value_init(&val, G_TYPE_INT);
	g_value_set_int(&val, 0);
	gst_structure_set_value(opt, "max-retransmits", &val);

	g_signal_emit_by_name(webrtc, "create-data-channel", "data-channel", opt, &dataChannel);
	gst_structure_free(opt);

	if (!dataChannel) return false;

	g_signal_connect(dataChannel, "on-error", G_CALLBACK(data_channel_on_error), this);
	g_signal_connect(dataChannel, "on-open", G_CALLBACK(data_channel_on_open), this);
	g_signal_connect(dataChannel, "on-close", G_CALLBACK(data_channel_on_close), this);
	g_signal_connect(dataChannel, "on-message-string", G_CALLBACK(data_channel_on_message_string), this);

	return true;
}
// 设置本地媒体描述
void WebrtcBin::setLocalDescription(GstWebRTCSessionDescription* offer)
{
	GstPromise* local_desc_promise = gst_promise_new();
	g_signal_emit_by_name(webrtc, "set-local-description", offer, local_desc_promise);
	gst_promise_interrupt(local_desc_promise);
	gst_promise_unref(local_desc_promise);
}
// 设置远程媒体描述
void WebrtcBin::setRemoteDescription(std::string& sdpStr)
{
	GstSDPMessage* sdp = NULL;
	GstPromise* promise = NULL;
	GstWebRTCSessionDescription* answer = NULL;
	if (gst_sdp_message_new(&sdp) != GST_SDP_OK)
		goto RETURN;
	if (gst_sdp_message_parse_buffer((const guint8*)sdpStr.c_str(), (guint)sdpStr.length(), sdp) != GST_SDP_OK)
		goto RETURN;

	answer = gst_webrtc_session_description_new(GST_WEBRTC_SDP_TYPE_ANSWER, sdp);
	if (!answer) goto RETURN;

	promise = gst_promise_new();
	g_signal_emit_by_name(webrtc, "set-remote-description", answer, promise);
	gst_promise_interrupt(promise);
	gst_promise_unref(promise);
	gst_webrtc_session_description_free(answer);
	return ;

RETURN:
	if (sdp)
	{
		gst_sdp_message_free(sdp);
	}
	closePipeline();
}
// 添加 ICE 候选
void WebrtcBin::addIceCandidate(gint sdpmlineindex, std::string& candidate)
{
	g_signal_emit_by_name(webrtc, "add-ice-candidate", sdpmlineindex, candidate.c_str());
}
// 发送本地媒体描述
void WebrtcBin::sendDescription(std::string& sdp)
{
	if (signalling)
	{
		if (signalling->sendDescription(sdp))
			return;
	}
	//std::cout << "sendDescription close" << std::endl;
	closePipeline();
}
// 发送 ICE 候选
void WebrtcBin::sendIceCandidate(gint sdpmlineindex, std::string& candidate)
{
	if (signalling)
	{
		if (signalling->sendIceCandidate(sdpmlineindex, candidate) || isConnected)
			return;
	}
	//std::cout << "sendIceCandidate close" << std::endl;
	closePipeline();
}
// 发送 ping 测试连接状态
void WebrtcBin::sendPing()
{
	auto bt = std::chrono::steady_clock::now();
	if (bt > pingTimePoint)
	{
		int ms = (int)(timeOutPoint - pingTimePoint - std::chrono::seconds(3)).count() / 1000000;
		pingTimePoint = bt + std::chrono::seconds(1);
		if (isConnected)
		{
			sendChannelMessage("{\"type\":\"ping\",\"data\":" + std::to_string(ms) + '}');
			if (bt > timeOutPoint) closePipeline();
		}
	}
}
// 添加引用
void WebrtcBin::addRef()
{
	refcount.fetch_add(1, std::memory_order_relaxed);
}
// 减少引用
void WebrtcBin::unRef()
{
	if (refcount.fetch_sub(1, std::memory_order_acq_rel) == 1)
	{
		delete this;
	}
}
// 发送通道消息
void WebrtcBin::sendChannelMessage(const std::string& msg)
{
	gst_webrtc_data_channel_send_string(dataChannel, msg.c_str());
}
// 设置数据通道消息事件回调
void WebrtcBin::onChannelMessage(std::function<void(WebrtcBin*, std::string& msg)>& cb)
{
	channelMessageCb = cb;
}
// 设置数据通道关闭事件回调
void WebrtcBin::onChannelClose(std::function<void(WebrtcBin*)>& cb)
{
	channelCloseCb = cb;
}
// 设置数据通道打开事件回调
void WebrtcBin::onChannelOpen(std::function<void(WebrtcBin*)>& cb)
{
	channelOpenCb = cb;
}
// 填充视频样本
GstFlowReturn WebrtcBin::pushVideoSample(GstSample* sample, bool is_sps_pps)
{
	if (isClosePipeline) return GST_FLOW_FLUSHING;
	if (!isPushVidSample)
	{
		if (isConnected) return GST_FLOW_OK;
		// 是否超时
		if (std::chrono::steady_clock::now() > timeOutPoint)
			return GST_FLOW_FLUSHING;

		return GST_FLOW_OK;
	}

	GstFlowReturn ret = GST_FLOW_OK;
	GstBuffer* buffer = gst_sample_get_buffer(sample);
	GstBuffer* newBuffer = NULL;
	if (isFirstVid)
	{
		// 如果是H264 第一帧必须是带有 sps和pps 的 I 帧
		if (!is_sps_pps)
			return ret;

		isFirstVid = false;
		newBuffer = gst_buffer_copy(buffer);
		GST_BUFFER_PTS(newBuffer) = GST_BUFFER_DTS(newBuffer) = 0;
		// 开始时间戳
		vidBeginTime = std::chrono::steady_clock::now();
	}
	else
	{
		newBuffer = gst_buffer_copy(buffer);
		GST_BUFFER_DTS(newBuffer) = (std::chrono::steady_clock::now() - vidBeginTime).count();
		GST_BUFFER_PTS(newBuffer) = GST_BUFFER_DTS(newBuffer);
	}
	GstSample* newSample = gst_sample_new(newBuffer, gst_sample_get_caps(sample), NULL, NULL);
	gst_buffer_unref(newBuffer);

	ret = gst_app_src_push_sample(GST_APP_SRC(vidAppsrc), newSample);
	gst_sample_unref(newSample);

	sendPing();
	return ret;
}
// 填充音频样本，frameSizeNano 音频采样长度（纳秒）
GstFlowReturn WebrtcBin::pushAudioSample(GstSample* sample, GstClockTime frameSizeNano)
{
	if (isClosePipeline) return GST_FLOW_FLUSHING;
	if (!isPushAudSample)
	{
		if (isConnected) return GST_FLOW_OK;
		// 是否超时
		if (std::chrono::steady_clock::now() > timeOutPoint)
			return GST_FLOW_FLUSHING;

		return GST_FLOW_OK;
	}

	GstFlowReturn ret = GST_FLOW_OK;
	GstBuffer* buffer = gst_buffer_copy(gst_sample_get_buffer(sample));
	if (isFirstAud)
	{
		isFirstAud = false;
		audBeginTime = std::chrono::steady_clock::now();
	}
	// 计算时间戳 必须与 frameSizeNano 对齐
	GST_BUFFER_DTS(buffer) = (std::chrono::steady_clock::now() - audBeginTime).count() / frameSizeNano * frameSizeNano;
	// 防止相同时间戳
	if (audioPTS >= GST_BUFFER_DTS(buffer)) {
		GST_BUFFER_DTS(buffer) = audioPTS + frameSizeNano;
	}
	audioPTS = GST_BUFFER_PTS(buffer) = GST_BUFFER_DTS(buffer);
	//audioPTS += frameSizeNano;

	GstSample* newSample = gst_sample_new(buffer, gst_sample_get_caps(sample), NULL, NULL);
	gst_buffer_unref(buffer);

	ret = gst_app_src_push_sample(GST_APP_SRC(audAppsrc), newSample);
	gst_sample_unref(newSample);

	sendPing();
	return ret;
}
// 释放管道
void WebrtcBin::release()
{
	closePipeline();

	if (busWatchId)
	{
		g_source_remove(busWatchId);
		busWatchId = 0;
	}
	if (videoSinkPad)
	{
		gst_element_release_request_pad(webrtc, videoSinkPad);
		gst_object_unref(videoSinkPad);
		videoSinkPad = NULL;
	}
	if (audioSinkPad)
	{
		gst_element_release_request_pad(webrtc, audioSinkPad);
		gst_object_unref(audioSinkPad);
		audioSinkPad = NULL;
	}
	if (dataChannel)
	{
		gst_webrtc_data_channel_close(dataChannel);
		g_object_unref(dataChannel);
		dataChannel = NULL;
	}
	if (pipeline)
	{
		gst_element_set_state(pipeline, GST_STATE_NULL);
		gst_object_unref(pipeline);
		pipeline = NULL;
	}
	if (signalling)
	{
		signalling->unRef();
		signalling = nullptr;
	}
}
// 设置管道为关闭状态
void WebrtcBin::closePipeline()
{
	isClosePipeline = true;
	isConnected = false;
	closeSignalling();
}
// 关闭信令传输器
void WebrtcBin::closeSignalling()
{
	if (signalling) signalling->close();
}

bool WebrtcBin::printError(const std::string& er)
{
	Debug::PrintError("[WebrtcBin]: ", er);
	return false;
}
