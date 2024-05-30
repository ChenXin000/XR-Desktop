#ifndef WASAPI_CAPTURE_H
#define WASAPI_CAPTURE_H

#include <string>
#include <MMDeviceAPI.h>
#include <AudioClient.h>
#include <AudioPolicy.h>
#include <thread>

#include "Debug.h"

class WASAPICapture
{
public:
	explicit WASAPICapture();
	~WASAPICapture();

	bool Initialize();
	void Destroy();

	bool Start();
	void Stop();

	int CaptureFrame(BYTE** data);
	void ReleaseBuffer();
	int GetSamplesPerSec() const;
	// 16 bit
	int GetBitsPerSample() const;
	int GetChannels() const;

private:
	//bool IsInitialized;
	const int REFTIMES_PER_SEC = 200000;
	UINT32 BufferSize;
	UINT32 numFramesAvailable;

	IMMDevice* IMM_Device;
	IAudioClient* I_AudioClient;
	IAudioCaptureClient* I_AudioCapture;
	IMMDeviceEnumerator* IMM_DeviceEnumerator;
	WAVEFORMATEX* W_MixFormat;

	HANDLE AudioSamplesReadyEvent;

private:
	bool AdjustFormatTo16Bits(WAVEFORMATEX* pwfx);

	bool PrintError(const std::string& er);
};

#endif // !WASAPI_CAPTURE_H
