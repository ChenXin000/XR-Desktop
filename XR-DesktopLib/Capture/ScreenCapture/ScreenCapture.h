#ifndef SCREEN_CAPTURE_H
#define SCREEN_CAPTURE_H

#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <iostream>

typedef enum
{
	RETURN_SUCCESS = 0,
	RETURN_WAIT_TIMEOUT = 1,
	RETURN_ERROR_EXPECTED = 2,
	RETURN_ERROR_UNEXPECTED = 3
} RETURN_RET;

class ScreenCapture
{
public:
	ScreenCapture & operator=(const ScreenCapture &) = delete;
	ScreenCapture(const ScreenCapture &) = delete;
	ScreenCapture() 
		: RealFramerate(0)
		, CountFr(0)
		, IsFixed_(false)
		, FrameIntervalTime(1000000000 / 30)
		, PrevFrameTime(std::chrono::steady_clock::now())
		, BeginTime(std::chrono::steady_clock::now())
		, WaitTimeError(0)
	{}
	virtual ~ScreenCapture() {}

	virtual bool Initialize(int DispIndex) = 0;
	virtual void Destroy() = 0;

	virtual RETURN_RET CaptureFrame(std::vector<uint8_t>& image, int timeOutMs) = 0;
	virtual RETURN_RET CaptureFrame(unsigned char *buf, size_t len, int timeOutMs) = 0;

	virtual uint32_t GetWidth()  const = 0;
	virtual uint32_t GetHeight() const = 0;
	virtual const std::string GetPixelFormat() const = 0;
	virtual int GetPixelSize() const = 0;
	virtual uint32_t GetImageSize() const = 0;

	virtual void SetFramerate(int framerate, bool isFixed) 
	{ 
		IsFixed_ = isFixed;
		FrameIntervalTime = std::chrono::nanoseconds(1000000000 / framerate);
	};
	virtual int GetRealFramerate() const
	{
		auto duration = std::chrono::steady_clock::now() - BeginTime;
		if (duration >= std::chrono::nanoseconds(1500000000))
			return 0;
		return RealFramerate;
	};
	virtual bool IsFixed() const { return IsFixed_; };

private:
	bool IsFixed_;			// 是否固定帧率
	int RealFramerate;		// 实时帧率
	int CountFr;			// 帧计数器
	std::chrono::nanoseconds FrameIntervalTime;	// 两帧之间的间隔时间（纳秒）
	std::chrono::steady_clock::time_point PrevFrameTime;	// 上一帧的时间点
	std::chrono::steady_clock::time_point BeginTime;		// 记录每秒帧率的开始时间
	std::chrono::nanoseconds WaitTimeError; // 等待时间误差
protected:
	void FramerateCtrl();
};

// 帧率控制（不足目标帧率则补充旧帧，超过目标帧率则丢弃多余的帧）
inline void ScreenCapture::FramerateCtrl()
{
	// 计算实时帧率
	++CountFr;
	auto duration = std::chrono::steady_clock::now() - BeginTime;
	if (duration >= std::chrono::nanoseconds(1000000000))
	{
		RealFramerate = (int)(((CountFr * 10000000000ll) / duration.count() + 5) / 10); // 四舍五入
		CountFr = 0;
		BeginTime = std::chrono::steady_clock::now();
	}

	if (!IsFixed_) return;

	auto NextFrameTime = PrevFrameTime + FrameIntervalTime; // 下一帧的时间点
	if (NextFrameTime > std::chrono::steady_clock::now())	// 是否满足下一帧的时间点，不满足则等待。
	{
		auto sleepTimePoint = NextFrameTime - WaitTimeError;
		std::this_thread::sleep_until(sleepTimePoint);	// 等待，直到下一帧的时间点。
		WaitTimeError = std::chrono::steady_clock::now() - sleepTimePoint;  // 计算等待误差
	}

	PrevFrameTime = std::chrono::steady_clock::now();
}

#endif