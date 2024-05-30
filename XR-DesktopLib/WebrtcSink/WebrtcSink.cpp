#include "WebrtcSink.h"

// webrtcSink 管道 bus 消息回调
static gboolean busCallback(GstBus* bus, GstMessage* message, gpointer data)
{
	switch (GST_MESSAGE_TYPE(message)) {
	case GST_MESSAGE_ERROR:
		GError* err;
		gchar* debug;
		gst_message_parse_error(message, &err, &debug);
		Debug::PrintError("Webrtc Sink Error: ", err->message);
		g_error_free(err);
		g_free(debug);

	case GST_MESSAGE_EOS:
		/* end-of-stream */
		g_main_loop_quit((GMainLoop*)data);
		break;
	default:
		return TRUE;
		/* unhandled message */
		break;
	}

	/* we want to be notified again the next time there is a message
	 * on the bus, so returning TRUE (FALSE means we want to stop watching
	 * for messages on the bus and our callback should not be called again)
	 */
	//return TRUE;
	return FALSE;
}

// 查找 nalu 头部，skip 是否跳过头部。
static guint8* find_nalu_head(guint8* begin, guint8* end, bool skip = false)
{
	if (end - begin > 4)
	{
		do {
			if (begin[0] || begin[1])
				continue;
			if (!begin[2] && begin[3] == 1)
				return skip ? begin + 4 : begin;
			if (begin[2] == 1)
				return skip ? begin + 3 : begin;

		} while (++begin != end);
	}
	return end;
}
// 提取第一帧中的 sps 和 pps。
static void extract_sps_pps(GstSample* sample, std::vector<guint8>& out_sps_pps)
{
	GstMapInfo map;
	GstBuffer* buffer = gst_sample_get_buffer(sample);
	gst_buffer_map(buffer, &map, GST_MAP_READ);
	guint8* end = map.data + map.size;

	guint8* sps_begin = NULL, * pps_end = NULL;
	guint8* offset = find_nalu_head(map.data, end, true);
	if (offset != end)
	{
		// 跳过 AUD 和 nalu 头
		if ((*offset & 0x1f) == 0x09)
			offset = find_nalu_head(offset, end, true);
		// 标记 sps
		if ((*offset & 0x1f) == 0x07)
		{
			sps_begin = offset - 4;
			if (*(offset - 3) == 1)
				sps_begin = offset - 3;
			offset = find_nalu_head(offset, end, true);
		}
		// 标记 pps
		if ((*offset & 0x1f) == 0x08)
			pps_end = find_nalu_head(offset, end, false);

		if (pps_end && sps_begin)
		{
			out_sps_pps.resize(pps_end - sps_begin);
			memcpy(&out_sps_pps[0], sps_begin, out_sps_pps.size());
		}
	}
	gst_buffer_unmap(buffer, &map);
}
// 给每个 I 帧添加 sps pps
static GstSample* insert_sps_pps(GstSample* sample, std::vector<guint8>& sps_pps, bool* isInsert)
{
	*isInsert = false;

	GstMapInfo map;
	GstBuffer* buffer = gst_sample_get_buffer(sample);
	gst_buffer_map(buffer, &map, GST_MAP_READ);
	guint8* end = map.data + map.size;

	guint8* offset = find_nalu_head(map.data, end, false);
	if (offset == end)
		goto RETURN;
	// 跳过 AUD
	if ((*(offset + 4) & 0x1f) == 0x09)
		offset = find_nalu_head(offset + 4, end, false);
	else if ((*(offset + 3) & 0x1f) == 0x09)
		offset = find_nalu_head(offset + 3, end, false);
	// I 帧
	if ((*(offset + 4) & 0x1f) == 0x05 || (*(offset + 3) & 0x1f) == 0x05)
	{
		GstMapInfo newMap;
		GstBuffer* newBuffer = gst_buffer_new_allocate(NULL, map.size + sps_pps.size(), NULL);
		gst_buffer_map(newBuffer, &newMap, GST_MAP_WRITE);

		size_t head_len = offset - map.data;
		memcpy(newMap.data, map.data, head_len);
		memcpy(newMap.data + head_len, &sps_pps[0], sps_pps.size()); // 添加 sps pps
		memcpy(newMap.data + head_len + sps_pps.size(), offset, end - offset);

		GstSample* newSample = gst_sample_new(newBuffer, gst_sample_get_caps(sample), NULL, NULL);
		gst_buffer_unmap(newBuffer, &newMap);
		gst_buffer_unref(newBuffer);

		gst_buffer_unmap(buffer, &map);
		gst_sample_unref(sample);

		*isInsert = true;
		return newSample;
	}
	if (((*(offset + 4)) & 0x1f) == 0x07 || (*(offset + 3) & 0x1f) == 0x07)
	{
		*isInsert = true;
	}

RETURN:
	gst_buffer_unmap(buffer, &map);
	return sample;
}

WebrtcSink::WebrtcSink()
	: busWatchId(0)
	, isStarted(false)
	, isRunVidLoop(false)
	, isRunAudLoop(false)
	, mainLoop(NULL)
	, pipeline(NULL)
	, mainThread(nullptr)
	, videoThread(nullptr)
	, audioThread(nullptr)
	, channelMessageCb(nullptr)
	, channelOpenCb(nullptr)
	, channelCloseCb(nullptr)
{}

WebrtcSink::~WebrtcSink()
{
	destroy();
}

bool WebrtcSink::initialize()
{
	pipeline = gst_pipeline_new("webrtcSinkPipeline");
	if (!pipeline)
		return printError("Failed to create pipeline");

	mainLoop = g_main_loop_new(NULL, false);
	GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
	busWatchId = gst_bus_add_watch(bus, busCallback, mainLoop);
	gst_object_unref(bus);

	mainThread = new std::thread(g_main_loop_run, mainLoop);

	while (!g_main_loop_is_running(mainLoop)) {};

	return true;
}
// 销毁
void WebrtcSink::destroy()
{
	stop();

	delAllWebrtcBin();

	if (busWatchId)
	{
		g_source_remove(busWatchId);
		busWatchId = 0;
	}
	if (mainLoop)
	{
		g_main_loop_quit(mainLoop);
		g_main_loop_unref(mainLoop);
		mainLoop = NULL;
	}
	if (mainThread)
	{
		mainThread->join();
		delete mainThread;
		mainThread = nullptr;
	}
	if (pipeline)
	{
		gst_element_set_state(pipeline, GST_STATE_NULL);
		gst_object_unref(pipeline);
		pipeline = NULL;
	}
}
// 新建一个连接
int WebrtcSink::newConnect(Signalling* signal, bool vidFlag, bool audFlag)
{
	std::string vidType = "";
	std::string audType = "";
	if (VideoEncoder::isLinkSource() && vidFlag) // 添加到视频列表
		vidType = VideoEncoder::getMediaType();

	if (AudioEncoder::isLinkSource() && audFlag) // 添加到音频列表
		audType = AudioEncoder::getMediaType();

	WebrtcBin* webrtcBin = WebrtcBin::createWebrtcBin(signal, vidType, audType);

	if (!webrtcBin) return -1;

	webrtcBin->onChannelMessage(channelMessageCb);
	webrtcBin->onChannelOpen(channelOpenCb);
	webrtcBin->onChannelClose(channelCloseCb);

	if (vidType != "")
	{
		addVideoBin(webrtcBin);
	}
	if (audType != "")
	{
		addAudioBin(webrtcBin);
	}

	webrtcBin->unRef();

	return 0;
}
// 添加 webrtcBin 到视频填充列表
void WebrtcSink::addVideoBin(WebrtcBin* bin)
{
	bin->addRef();
	vidBinList.pushBack(bin);
	vidCvLock.mutex_.lock();
	vidCvLock.cv_.notify_one();
	vidCvLock.mutex_.unlock();
}
// 添加 webrtcBin 到音频填充列表
void WebrtcSink::addAudioBin(WebrtcBin* bin)
{
	bin->addRef();
	audBinList.pushBack(bin);
	audCvLock.mutex_.lock();
	audCvLock.cv_.notify_one();
	audCvLock.mutex_.unlock();
}
// 连接视频输入源
bool WebrtcSink::linkVideoSource(Source& vidSrc, EncType enc, int bitrate)
{
	if (!VideoEncoder::initialize(pipeline, enc, bitrate))
		return printError("Failed to init VideoEncoder");
	return VideoEncoder::linkSource(vidSrc);
}
// 连接音频输入源
bool WebrtcSink::linkAudioSource(Source& audSrc, int bitrate)
{
	if (!AudioEncoder::initialize(pipeline, bitrate))
		return printError("Failed to init AudioEncoder");
	return AudioEncoder::linkSource(audSrc);
}

void WebrtcSink::onChannelMessage(std::function<void(WebrtcBin*, std::string&)> cb)
{
	channelMessageCb = cb;
}
void WebrtcSink::onChannelClose(std::function<void(WebrtcBin*)> cb)
{
	channelCloseCb = cb;
}
void WebrtcSink::onChannelOpen(std::function<void(WebrtcBin*)> cb)
{
	channelOpenCb = cb;
}
// 回收 webrtcBin
gboolean WebrtcSink::delWebrtcbin(WebrtcBin* webrtcBin)
{
	if (!webrtcBin)
		return FALSE;

	webrtcBin->unRef();

	return FALSE;
}
//删除所有 webrtcBin
void WebrtcSink::delAllWebrtcBin()
{
	auto item = vidBinList.getBegin();
	auto end = vidBinList.getEnd();
	while (item != end)
	{
		item->data->unRef();
		item = vidBinList.remove(item);
	}

	item = audBinList.getBegin();
	end = audBinList.getEnd();
	while (item != end)
	{
		item->data->unRef();
		item = vidBinList.remove(item);
	}
}

bool WebrtcSink::start()
{
	if (isStarted)
		return true;

	if (gst_element_set_state(pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
		return false;

	if (VideoEncoder::isLinkSource())
	{
		VideoEncoder::start();
		if (!videoThread)
		{
			isRunVidLoop = true;
			videoThread = new std::thread([this] { pushVidLoop(); });
		}
	}
	if (AudioEncoder::isLinkSource())
	{
		AudioEncoder::start();
		if (!audioThread)
		{
			isRunAudLoop = true;
			audioThread = new std::thread([this] { pushAudLoop(); });
		}
	}

	return isStarted = true;
}

void WebrtcSink::stop()
{
	if(pipeline)
		gst_element_set_state(pipeline, GST_STATE_NULL);

	if (videoThread)
	{
		vidCvLock.mutex_.lock();
		vidCvLock.cv_.notify_one();
		vidCvLock.mutex_.unlock();

		isRunVidLoop = false;
		videoThread->join();
		delete videoThread;
		videoThread = nullptr;
	}
	if (audioThread)
	{
		audCvLock.mutex_.lock(); 
		audCvLock.cv_.notify_one();
		audCvLock.mutex_.unlock();

		isRunAudLoop = false;
		audioThread->join();
		delete audioThread;
		audioThread = nullptr;
	}
	VideoEncoder::stop();
	AudioEncoder::stop();

	isStarted = false;
}
//给所有webrtcBin添加视频样本_
void WebrtcSink::pushVidLoop()
{
	bool is_sps_pps = true;
	std::vector<guint8> sps_pps;
	GstSample* sample = NULL;
	bool isH264 = VideoEncoder::getMediaType() == "video/x-h264";

	if (isH264)
	{
		sample = VideoEncoder::pullSample();
		if (!sample) return;

		extract_sps_pps(sample, sps_pps);
		gst_sample_unref(sample);
	}

	while (isRunVidLoop)
	{
		auto item = vidBinList.getBegin();
		auto end = vidBinList.getEnd();
		//  等待添加 WebrtcBin
		if (item == end)
		{
			std::unique_lock<std::mutex> lock(vidCvLock.mutex_);
			while (item == end)
			{
				if (!isRunVidLoop)
					return ;
				vidCvLock.cv_.wait(lock);
				item = vidBinList.getBegin();
				end = vidBinList.getEnd();
			}
		}
		//auto begin = std::chrono::steady_clock::now();

		sample = VideoEncoder::pullSample();
		if (!sample) break;

		if (isH264) {
			sample = insert_sps_pps(sample, sps_pps, &is_sps_pps);
		}

		while (item != end)	// 给已添加的 webrtcBin 填充数据
		{
			WebrtcBin* webrtcBin = item->data;

			if (webrtcBin->pushVideoSample(sample, is_sps_pps) != GST_FLOW_OK)
			{
				item = vidBinList.remove(item);
				g_idle_add((GSourceFunc)WebrtcSink::delWebrtcbin, webrtcBin);
				//std::cout << "1\n";
				//std::cout << "remove vid rtc" << std::endl;
			}
			else item = item->next;
		}
		gst_sample_unref(sample);
		//Debug::PrintError("dd: ", std::to_string((std::chrono::steady_clock::now() - begin).count() / 1000 / 1000));
	}

}

//给所有webrtcbin添加音频样本
void WebrtcSink::pushAudLoop()
{
	// 帧大小 纳秒
	GstClockTime frameSizeNano = (GstClockTime)AudioEncoder::getFrameSizeMs() * 1000000;

	while (isRunAudLoop)
	{
		auto item = audBinList.getBegin();
		auto end = audBinList.getEnd();

		if (item == end)	//  等待添加 WebrtcBin
		{
			std::unique_lock<std::mutex> lock(audCvLock.mutex_);
			while (item == end)
			{
				if (!isRunAudLoop)
					return;
				audCvLock.cv_.wait(lock);
				item = audBinList.getBegin();
				end = audBinList.getEnd();
			}
		}

		GstSample* sample = AudioEncoder::pullSample();
		if (!sample) break;

		while (item != end)	// 给已添加的 webrtcBin 填充数据
		{
			WebrtcBin* webrtcBin = item->data;
			
			if (webrtcBin->pushAudioSample(sample, frameSizeNano) != GST_FLOW_OK)
			{
				item = audBinList.remove(item);
				g_idle_add((GSourceFunc)WebrtcSink::delWebrtcbin, webrtcBin);
				//std::cout << "2\n";
				//std::cout << "remove aud rtc" << std::endl;
			}
			else item = item->next;
		}
		gst_sample_unref(sample);
	}
}

bool WebrtcSink::printError(const std::string& er)
{
	Debug::PrintError("[WebrtcSink]: ", er);
	return false;
}