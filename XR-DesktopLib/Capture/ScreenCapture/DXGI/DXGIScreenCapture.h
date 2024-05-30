// PHZ
// 2020-11-20

#ifndef DXGI_SCREEN_CAPTURE_H
#define DXGI_SCREEN_CAPTURE_H

#include "CommonTypes.h"
#include "../ScreenCapture.h"
#include "../WindowHelper.h"

#include "DisplayManager.h"
#include "DuplicationManager.h"

//#include <atomic>
//#include <cstdio>
//#include <cstdint>
#include <string>
#include <mutex>
#include <thread>
//#include <memory>
#include <vector>
#include <wrl.h>
#include <dxgi.h>
#include <d3d11.h>
#include <dxgi1_2.h>

#include "Debug.h"

#define PIXEL_FORMAT	"BGRA"
#define PIXEL_SIZE		4

class DXGIScreenCapture : public ScreenCapture
{
public:
	DXGIScreenCapture();
	virtual ~DXGIScreenCapture();

	bool Initialize(int DispIndex) override;
	void Destroy() override;

private:
	uint32_t GetWidth() const override { return ImageWidth; }
	uint32_t GetHeight() const override { return ImageHeight; }
	const std::string GetPixelFormat() const override { return PIXEL_FORMAT; }
	int GetPixelSize() const override { return PIXEL_SIZE; }
	uint32_t GetImageSize() const override { return ImageSize; }

	RETURN_RET CaptureFrame(std::vector<uint8_t>& BgraImage, int timeOutMs) override;
	RETURN_RET CaptureFrame(unsigned char *buf, size_t len, int timeOutMs) override;

	bool PrintError(const std::string &er);

private:

	bool CreateDevice(_Out_ ID3D11Device ** Device, _Out_ ID3D11DeviceContext ** Context);
	bool GetOutputDesc(_In_ ID3D11Device* Device, UINT DesplayIndex, _Out_ DXGI_OUTPUT_DESC* Desc);
	bool CreateTexture(_In_ ID3D11Device* Device, _In_ DXGI_OUTPUT_DESC* OutputDesc);

	DX::Monitor Monitor;

	UINT ImageWidth;
	UINT ImageHeight;
	UINT ImageSize;

	ID3D11Device * OutDevice;
	ID3D11DeviceContext * OutContext;
	ID3D11Texture2D* SharedTexture;
	ID3D11Texture2D* BgraTexture;

	FRAME_DATA CurrentData;
	PTR_INFO PtrInfo;
	DisplayManager DisplayMgr;
	DuplicationManager DuplicationMgr;
};

#endif