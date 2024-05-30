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
	bool IsFixed_;			// �Ƿ�̶�֡��
	int RealFramerate;		// ʵʱ֡��
	int CountFr;			// ֡������
	std::chrono::nanoseconds FrameIntervalTime;	// ��֮֡��ļ��ʱ�䣨���룩
	std::chrono::steady_clock::time_point PrevFrameTime;	// ��һ֡��ʱ���
	std::chrono::steady_clock::time_point BeginTime;		// ��¼ÿ��֡�ʵĿ�ʼʱ��
	std::chrono::nanoseconds WaitTimeError; // �ȴ�ʱ�����
protected:
	void FramerateCtrl();
};

// ֡�ʿ��ƣ�����Ŀ��֡���򲹳��֡������Ŀ��֡�����������֡��
inline void ScreenCapture::FramerateCtrl()
{
	// ����ʵʱ֡��
	++CountFr;
	auto duration = std::chrono::steady_clock::now() - BeginTime;
	if (duration >= std::chrono::nanoseconds(1000000000))
	{
		RealFramerate = (int)(((CountFr * 10000000000ll) / duration.count() + 5) / 10); // ��������
		CountFr = 0;
		BeginTime = std::chrono::steady_clock::now();
	}

	if (!IsFixed_) return;

	auto NextFrameTime = PrevFrameTime + FrameIntervalTime; // ��һ֡��ʱ���
	if (NextFrameTime > std::chrono::steady_clock::now())	// �Ƿ�������һ֡��ʱ��㣬��������ȴ���
	{
		auto sleepTimePoint = NextFrameTime - WaitTimeError;
		std::this_thread::sleep_until(sleepTimePoint);	// �ȴ���ֱ����һ֡��ʱ��㡣
		WaitTimeError = std::chrono::steady_clock::now() - sleepTimePoint;  // ����ȴ����
	}

	PrevFrameTime = std::chrono::steady_clock::now();
}

#endif