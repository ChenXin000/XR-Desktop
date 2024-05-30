#include "WASAPICapture.h"

WASAPICapture::WASAPICapture()
	: IMM_Device(nullptr)
	, I_AudioClient(nullptr)
	, I_AudioCapture(nullptr)
	, IMM_DeviceEnumerator(nullptr)
	, W_MixFormat(nullptr)
	, AudioSamplesReadyEvent(nullptr)
	, BufferSize(0)
	//, IsInitialized(false)
	, numFramesAvailable(0)
{}

WASAPICapture::~WASAPICapture()
{
	Destroy();
}

bool WASAPICapture::Initialize()
{
	HRESULT hr;
	hr = CoInitialize(NULL);
	if (FAILED(hr))
	{
		printf("Unable to initialize COM in render thread: %x\n", hr);
		return false;
	}

	hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&IMM_DeviceEnumerator));
	if (FAILED(hr))
		return PrintError("Unable to instantiate device enumerator");

	hr = IMM_DeviceEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &IMM_Device);
	if (FAILED(hr))
		return PrintError("Unable to get default device");

	hr = IMM_Device->Activate(__uuidof(IAudioClient), CLSCTX_INPROC_SERVER, NULL, reinterpret_cast<void**>(&I_AudioClient));
	if (FAILED(hr))
		return PrintError("Unable to activate audio client");

	hr = I_AudioClient->GetMixFormat(&W_MixFormat);
	if (FAILED(hr))
		return PrintError("Unable to get mix format on audio client");

	AdjustFormatTo16Bits(W_MixFormat);

	hr = I_AudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK, REFTIMES_PER_SEC, 0, W_MixFormat, NULL);
	if (FAILED(hr))
		return PrintError("Unable to initialize audio client");

	AudioSamplesReadyEvent = CreateEventEx(NULL, NULL, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
	if (!AudioSamplesReadyEvent)
		return PrintError("Unable to create samples ready event");

	hr = I_AudioClient->SetEventHandle(AudioSamplesReadyEvent);
	if (hr == AUDCLNT_E_EVENTHANDLE_NOT_EXPECTED)
		return PrintError("Unable to set ready event");
	AUDCLNT_E_EVENTHANDLE_NOT_EXPECTED;

	hr = I_AudioClient->GetService(IID_PPV_ARGS(&I_AudioCapture));
	if (FAILED(hr))
		return PrintError("Unable to get new capture client");

	return true;
}

bool WASAPICapture::Start()
{
	HRESULT hr = I_AudioClient->Start();
	if (FAILED(hr))
	{
		printf("Unable to start capture client: %x.\n", hr);
		return false;
	}
	return true;
}

void WASAPICapture::Stop()
{
	HRESULT hr = I_AudioClient->Stop();
	if (FAILED(hr))
	{
		printf("[WASAPICapture] Failed to stop audio client.\n");
	}
}

int WASAPICapture::GetSamplesPerSec() const
{
	return W_MixFormat->nSamplesPerSec;;
}

int WASAPICapture::GetBitsPerSample() const
{
	return W_MixFormat->wBitsPerSample;
}

int WASAPICapture::GetChannels() const
{
	return W_MixFormat->nChannels;
}

int WASAPICapture::CaptureFrame(BYTE** data)
{
	HRESULT hr;
	DWORD flags;
	uint32_t packetLength = 0;

	WaitForMultipleObjects(1, &AudioSamplesReadyEvent, FALSE, 20);

	hr = I_AudioCapture ->GetNextPacketSize(&packetLength);
	if (FAILED(hr))
	{
		printf("[WASAPICapture] Faild to get next data packet size.\n");
		return -1;
	}
	if (packetLength > 0)
	{
		hr = I_AudioCapture->GetBuffer(data, &numFramesAvailable, &flags, NULL, NULL);
		if (FAILED(hr))
		{
			printf("[WASAPICapture] Faild to get buffer.\n");
			return -1;
		}
		if (flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY)
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		
		return numFramesAvailable * W_MixFormat->nBlockAlign;
	}
	return 0;
}

void WASAPICapture::ReleaseBuffer()
{
	I_AudioCapture->ReleaseBuffer(numFramesAvailable);
	numFramesAvailable = 0;
}

bool WASAPICapture::AdjustFormatTo16Bits(WAVEFORMATEX* pwfx)
{
	if (pwfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
	{
		pwfx->wFormatTag = WAVE_FORMAT_PCM;
	}
	else if (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
	{
		PWAVEFORMATEXTENSIBLE pEx = reinterpret_cast<PWAVEFORMATEXTENSIBLE>(pwfx);
		if (IsEqualGUID(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, pEx->SubFormat))
		{
			pEx->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
			pEx->Samples.wValidBitsPerSample = 16;
		}
	}
	else
	{
		return false;
	}
	pwfx->nSamplesPerSec = 48000;
	pwfx->wBitsPerSample = 16;
	pwfx->nBlockAlign = pwfx->nChannels * pwfx->wBitsPerSample / 8;
	pwfx->nAvgBytesPerSec = pwfx->nBlockAlign * pwfx->nSamplesPerSec;
	return true;
}

void WASAPICapture::Destroy()
{
	if (IMM_Device)
	{
		IMM_Device->Release();
		IMM_Device = nullptr;
	}
	if (I_AudioClient)
	{
		I_AudioClient->Release();
		I_AudioClient = nullptr;
	}
	if (I_AudioCapture)
	{
		I_AudioCapture->Release();
		I_AudioCapture = nullptr;
	}
	if (IMM_DeviceEnumerator)
	{
		IMM_DeviceEnumerator->Release();
		IMM_DeviceEnumerator = nullptr;
	}
	if (W_MixFormat)
	{
		CoTaskMemFree(W_MixFormat);
		W_MixFormat = nullptr;
	}

	CoUninitialize();
}

bool WASAPICapture::PrintError(const std::string& er)
{
	Debug::PrintError("[WASAPICapture]: ",er);
	return false;
}
