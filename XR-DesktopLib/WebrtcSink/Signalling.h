#ifndef SIGNALLING_H
#define SIGNALLING_H

#include "gst/gst.h"
#include <functional>
#include <string>
#include <atomic>

class Signalling
{
public:
	explicit Signalling() 
		: refcount(1)
	{};
	virtual ~Signalling() {};

private:
	std::atomic<int> refcount;

	// 读取事件回调
	std::function<void(std::string& sdp)> readDescriptionCb;
	std::function<void(gint sdpmlineindex, std::string& candidate)> readIceCandidateCb;

public:
	virtual bool sendDescription(std::string& sdp) = 0;
	virtual bool sendIceCandidate(gint sdpmlineindex, std::string& candidate) = 0;
	virtual void close() = 0;

	// 监听读取事件
	void onReadDescription(std::function<void(std::string&)>& cb) { 
		readDescriptionCb = cb; 
	};
	void onReadIceCandidate(std::function<void(gint, std::string&)>& cb) { 
		readIceCandidateCb = cb;
	};

	virtual void addRef() {
		refcount.fetch_add(1, std::memory_order_relaxed);
	};

	virtual void unRef() {
		if (refcount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
			delete this; // 必须是 new 分配的内存
		}
	};

protected:
	// 触发读取事件
	void emitReadDescription(std::string& sdp) {
		if (readDescriptionCb) readDescriptionCb(sdp);
	};
	void emitReadIceCandidate(gint sdpmlineindex, std::string& candidate) {
		if (readIceCandidateCb) readIceCandidateCb(sdpmlineindex, candidate);
	};
};

#endif // !SIGNALLING_H
