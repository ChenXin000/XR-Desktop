#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "DXGIScreenCapture.h"

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")

DXGIScreenCapture::DXGIScreenCapture()
	: ImageWidth(0)
	, ImageHeight(0)
	, ImageSize(0)
	, Monitor()
	, OutDevice(nullptr)
	, OutContext(nullptr)
	, SharedTexture(nullptr)
	, BgraTexture(nullptr)
{
	memset(&PtrInfo, 0, sizeof(PTR_INFO));
	memset(&CurrentData, 0, sizeof(FRAME_DATA));
}

DXGIScreenCapture::~DXGIScreenCapture()
{
	Destroy();
}

bool DXGIScreenCapture::Initialize(int DisplayIndex)
{
	DXGI_OUTPUT_DESC OutputDesc;
	std::vector<DX::Monitor> monitors = DX::GetMonitors();
	if (monitors.size() < (size_t)(DisplayIndex + 1))
		return false;
	Monitor = monitors[DisplayIndex];

	if (!CreateDevice(&OutDevice, &OutContext))
		return false;
	if (!GetOutputDesc(OutDevice, DisplayIndex, &OutputDesc))
		return false;
	if (!CreateTexture(OutDevice, &OutputDesc))
		return false;

	ImageWidth = OutputDesc.DesktopCoordinates.right - OutputDesc.DesktopCoordinates.left;
	ImageHeight = OutputDesc.DesktopCoordinates.bottom - OutputDesc.DesktopCoordinates.top;

	ImageSize = ImageWidth * ImageHeight * GetPixelSize();

	if (!DuplicationMgr.InitDuplication(OutDevice, DisplayIndex))
		return false;

	if (!DisplayMgr.InitD3D(OutDevice, OutContext, DuplicationMgr.GetOutputDesc()))
		return false;

	return true;
}
//
// Create Device
//
bool DXGIScreenCapture::CreateDevice(_Out_ ID3D11Device** Device, _Out_ ID3D11DeviceContext** Context)
{
	HRESULT hr = S_OK;

	// Driver types supported
	D3D_DRIVER_TYPE DriverTypes[] =
	{
		D3D_DRIVER_TYPE_HARDWARE,
		D3D_DRIVER_TYPE_WARP,
		D3D_DRIVER_TYPE_REFERENCE,
	};
	UINT NumDriverTypes = ARRAYSIZE(DriverTypes);

	// Feature levels supported
	D3D_FEATURE_LEVEL FeatureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL_9_1
	};
	UINT NumFeatureLevels = ARRAYSIZE(FeatureLevels);

	D3D_FEATURE_LEVEL FeatureLevel;

	// Create Device
	for (UINT i = 0; i < NumDriverTypes; ++i)
	{
		hr = D3D11CreateDevice(nullptr, DriverTypes[i], nullptr, 0, FeatureLevels, NumFeatureLevels,
			D3D11_SDK_VERSION, Device, &FeatureLevel, Context);
		if (SUCCEEDED(hr))
			break;  // Device creation success, no need to loop anymore
	}
	if (FAILED(hr))
		return PrintError("Failed to create Device in CreateDevice");

	return true;
}
// 获取输出描述
bool DXGIScreenCapture::GetOutputDesc(_In_ ID3D11Device* Device, UINT DesplayIndex, _Out_ DXGI_OUTPUT_DESC* Desc)
{
	// Get DXGI device
	IDXGIDevice* DxgiDevice = nullptr;
	HRESULT hr = Device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&DxgiDevice));
	if (FAILED(hr))
		return PrintError("Failed to QI for DXGI Device");

	// Get DXGI adapter
	IDXGIAdapter* DxgiAdapter = nullptr;
	hr = DxgiDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(&DxgiAdapter));
	DxgiDevice->Release();
	if (FAILED(hr))
		return PrintError("Failed to get parent DXGI Adapter");

	// Get output
	IDXGIOutput* DxgiOutput = nullptr;
	hr = DxgiAdapter->EnumOutputs(DesplayIndex, &DxgiOutput);
	DxgiAdapter->Release();
	if (FAILED(hr))
		return PrintError("Failed to get specified output in DuplicationManager");

	hr = DxgiOutput->GetDesc(Desc);
	DxgiOutput->Release();
	if (FAILED(hr))
		return false;

	return true;
}
//
//	Create Texture
//
bool DXGIScreenCapture::CreateTexture(_In_ ID3D11Device* Device, _In_ DXGI_OUTPUT_DESC *OutputDesc)
{
	HRESULT hr = S_OK;
	// Create shared texture for all duplication threads to draw into
	D3D11_TEXTURE2D_DESC Desc;
	memset(&Desc,0 ,sizeof(Desc));
	RECT DeskBounds = OutputDesc->DesktopCoordinates;

	Desc.Width = DeskBounds.right - DeskBounds.left;
	Desc.Height = DeskBounds.bottom - DeskBounds.top;
	Desc.MipLevels = 1;
	Desc.ArraySize = 1;
	Desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	Desc.SampleDesc.Count = 1;
	Desc.Usage = D3D11_USAGE_DEFAULT;
	Desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	Desc.CPUAccessFlags = 0;
	Desc.MiscFlags = 0;

	hr = Device->CreateTexture2D(&Desc, nullptr, &SharedTexture);
	if (FAILED(hr))
		return PrintError("Failed to create SharedTexture.");

	Desc.Usage = D3D11_USAGE_STAGING;
	Desc.BindFlags = 0;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	Desc.MiscFlags = 0;

	hr = Device->CreateTexture2D(&Desc, nullptr, &BgraTexture);
	if (FAILED(hr))
		return PrintError("Failed to create BgraTexture.");

	return true;
}

void DXGIScreenCapture::Destroy()
{
	if (SharedTexture)
	{
		SharedTexture->Release();
		SharedTexture = nullptr;
	}
	if (BgraTexture)
	{
		BgraTexture->Release();
		BgraTexture = nullptr;
	}
	if (OutContext)
	{
		OutContext->Release();
		OutContext = nullptr;
	}
	if (OutDevice)
	{
		OutDevice->Release();
		OutDevice = nullptr;
	}
	if (PtrInfo.PtrShapeBuffer)
	{
		delete[] PtrInfo.PtrShapeBuffer;
		PtrInfo.PtrShapeBuffer = nullptr;
	}
	DisplayMgr.Destroy();
	DuplicationMgr.Destroy();
}

bool DXGIScreenCapture::PrintError(const std::string& er)
{
	Debug::PrintError("[DXGIScreenCapture]: ",er);
	return false;
}

RETURN_RET DXGIScreenCapture::CaptureFrame(std::vector<uint8_t>& BgraImage, int timeOutMs)
{
	if (BgraImage.size() < ImageSize) 
		BgraImage.resize(ImageSize);

	return CaptureFrame(&BgraImage[0], BgraImage.size(), timeOutMs);
}

RETURN_RET DXGIScreenCapture::CaptureFrame(unsigned char* buf, size_t len, int timeOutMs)
{
	bool IsTimeOut = false;
	INT OffsetX = Monitor.left;
	INT OffsetY = Monitor.top;
	HRESULT hr;
	D3D11_MAPPED_SUBRESOURCE dsec = { 0 };

	hr = DuplicationMgr.GetFrame(&CurrentData, IsFixed() ? 8 : timeOutMs);
	if (FAILED(hr))
	{
		if (hr == DXGI_ERROR_WAIT_TIMEOUT)
		{
			IsTimeOut = true;
			goto RETURN;
		}
		// 创建新的 IDXGIOutputDuplication（全屏问题，系统session切换）
		if (hr == DXGI_ERROR_ACCESS_LOST || hr == DXGI_ERROR_INVALID_CALL)
		{
			DuplicationMgr.ReleaseFrame();
			DuplicationMgr.ResetDuplication();
			return RETURN_ERROR_EXPECTED;
		}
		goto EXIT;
	}

	hr = DuplicationMgr.GetMouse(&PtrInfo, &(CurrentData.FrameInfo), OffsetX, OffsetY);
	if (FAILED(hr))
	{
		DuplicationMgr.ReleaseFrame();
		return RETURN_ERROR_EXPECTED;
	}
	// Process new frame
	if (!DisplayMgr.ProcessFrame(&CurrentData, OffsetX, OffsetY))
		goto EXIT;

	if (!DisplayMgr.DrawFrame(SharedTexture))
		goto EXIT;

	if (PtrInfo.Visible)
	{
		if (!DisplayMgr.DrawMouse(&PtrInfo, SharedTexture))
			goto EXIT;
	}
	DuplicationMgr.ReleaseFrame();
	OutContext->CopyResource(BgraTexture, SharedTexture);

RETURN:
	hr = OutContext->Map(BgraTexture, 0, D3D11_MAP_READ, 0, &dsec);
	if (FAILED(hr))
		return RETURN_ERROR_UNEXPECTED;

	memcpy(buf, dsec.pData, (len > ImageSize ? ImageSize : len));
	OutContext->Unmap(BgraTexture, 0);

	ScreenCapture::FramerateCtrl(); 
	return IsTimeOut ? RETURN_WAIT_TIMEOUT : RETURN_SUCCESS;

EXIT:
	DuplicationMgr.ReleaseFrame();
	return RETURN_ERROR_UNEXPECTED;
}