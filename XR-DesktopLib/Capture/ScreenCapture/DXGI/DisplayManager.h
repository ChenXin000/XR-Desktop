#pragma once
#include "CommonTypes.h"
#include "Debug.h"

class DisplayManager
{
public:
	explicit DisplayManager();
	~DisplayManager();

private:
	ID3D11Texture2D* DeskSurface;

	ID3D11Texture2D * MoveSurf;
	ID3D11RenderTargetView* DeskRTView;
	ID3D11RenderTargetView* SharedRTView;

	BYTE * DirtyVertexBufferAlloc;
	UINT DirtyVertexBufferAllocSize;

	ID3D11BlendState * BlendState;

	ID3D11Device* Device;
	ID3D11DeviceContext* DeviceContext;
	ID3D11VertexShader* VertexShader;
	ID3D11PixelShader* PixelShader;
	ID3D11InputLayout* InputLayout;
	ID3D11SamplerState* SamplerLinear;

	DXGI_OUTPUT_DESC* DesktopDesc;

public:
	bool InitD3D(_In_ ID3D11Device* Dev, _In_ ID3D11DeviceContext* DevContext, _In_ DXGI_OUTPUT_DESC* DeskDesc);
	void Destroy();
	bool ProcessFrame(_In_ FRAME_DATA* Data, INT OffsetX, INT OffsetY);
	bool DrawFrame(_Inout_ ID3D11Texture2D* SharedSurf);
	bool DrawMouse(_In_ PTR_INFO* PtrInfo, _Inout_ ID3D11Texture2D* SharedSurf);

private:
	bool CopyMove(_Inout_ ID3D11Texture2D* SharedSurf, _In_reads_(MoveCount) DXGI_OUTDUPL_MOVE_RECT* MoveBuffer, UINT MoveCount, INT OffsetX, INT OffsetY, _In_ DXGI_OUTPUT_DESC* DeskDesc, INT TexWidth, INT TexHeight);
	bool CopyDirty(_In_ ID3D11Texture2D* SrcSurface, _Inout_ ID3D11Texture2D* SharedSurf, _In_reads_(DirtyCount) RECT* DirtyBuffer, UINT DirtyCount, INT OffsetX, INT OffsetY, _In_ DXGI_OUTPUT_DESC* DeskDesc);
	void SetMoveRect(_Out_ RECT* SrcRect, _Out_ RECT* DestRect, _In_ DXGI_OUTPUT_DESC* DeskDesc, _In_ DXGI_OUTDUPL_MOVE_RECT* MoveRect, INT TexWidth, INT TexHeight);
	void SetDirtyVert(_Out_writes_(NUMVERTICES) VERTEX* Vertices, _In_ RECT* Dirty, INT OffsetX, INT OffsetY, _In_ DXGI_OUTPUT_DESC* DeskDesc, _In_ D3D11_TEXTURE2D_DESC* FullDesc, _In_ D3D11_TEXTURE2D_DESC* ThisDesc);
	bool ProcessMonoMask(bool IsMono, _Inout_ PTR_INFO* PtrInfo, _In_ ID3D11Texture2D* SharedSurf, _Out_ INT* PtrWidth, _Out_ INT* PtrHeight, _Out_ INT* PtrLeft, _Out_ INT* PtrTop, _Outptr_result_bytebuffer_(*PtrHeight** PtrWidth* BPP) BYTE** InitBuffer, _Out_ D3D11_BOX* Box);

	bool PrintError(const std::string &er);
};

