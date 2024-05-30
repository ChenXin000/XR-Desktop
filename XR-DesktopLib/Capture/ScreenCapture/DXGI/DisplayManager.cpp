#include "DisplayManager.h"

#include "PixelShader.h"
#include "VertexShader.h"

DisplayManager::DisplayManager() 
	: MoveSurf(nullptr)
	, DeskSurface(nullptr)
	, DeskRTView(nullptr)
	, SharedRTView(nullptr)
	, DesktopDesc(nullptr)
	, DirtyVertexBufferAlloc(nullptr)
	, DirtyVertexBufferAllocSize(0)
	, BlendState(nullptr)
	, Device(nullptr)
	, DeviceContext(nullptr)
	, VertexShader(nullptr)
	, PixelShader(nullptr)
	, InputLayout(nullptr)
	, SamplerLinear(nullptr)
{	
}

DisplayManager::~DisplayManager()
{
	Destroy();
}

bool DisplayManager::InitD3D(_In_ ID3D11Device * Dev, _In_ ID3D11DeviceContext * DevContext, _In_ DXGI_OUTPUT_DESC* DeskDesc)
{
	Device = Dev;
	DeviceContext = DevContext;
	DesktopDesc = DeskDesc;

	Device->AddRef();
	DeviceContext->AddRef();

	// VERTEX shader
	UINT Size = ARRAYSIZE(g_VS);
	HRESULT hr = Device->CreateVertexShader(g_VS, Size, nullptr, &VertexShader);
	if (FAILED(hr))
		return PrintError("Failed to create vertex shader in InitD3D");

	// Input layout
	D3D11_INPUT_ELEMENT_DESC Layout[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
	};
	UINT NumElements = ARRAYSIZE(Layout);
	hr = Device->CreateInputLayout(Layout, NumElements, g_VS, Size, &InputLayout);
	if (FAILED(hr))
		return PrintError("Failed to create input layout in InitD3D");

	DeviceContext->IASetInputLayout(InputLayout);

	// Pixel shader
	Size = ARRAYSIZE(g_PS);
	hr = Device->CreatePixelShader(g_PS, Size, nullptr, &PixelShader);
	if (FAILED(hr))
		return PrintError("Failed to create pixel shader in InitD3D");

	// Set up sampler
	D3D11_SAMPLER_DESC SampDesc;
	RtlZeroMemory(&SampDesc, sizeof(SampDesc));
	SampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	SampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	SampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	SampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	SampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	SampDesc.MinLOD = 0;
	SampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	hr = Device->CreateSamplerState(&SampDesc, &SamplerLinear);
	if (FAILED(hr))
		return PrintError("Failed to create sampler state in InitD3D");

	// Create the blend state
	D3D11_BLEND_DESC BlendStateDesc;
	RtlZeroMemory(&BlendStateDesc, sizeof(BlendStateDesc));
	BlendStateDesc.AlphaToCoverageEnable = FALSE;
	BlendStateDesc.IndependentBlendEnable = FALSE;
	BlendStateDesc.RenderTarget[0].BlendEnable = TRUE;
	BlendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	BlendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	BlendStateDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	BlendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	BlendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	BlendStateDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	BlendStateDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	hr = Device->CreateBlendState(&BlendStateDesc, &BlendState);
	if (FAILED(hr))
		return PrintError("Failed to create blend state in InitD3D");

	RECT DeskBounds = DesktopDesc->DesktopCoordinates;
	D3D11_TEXTURE2D_DESC Desc;
	RtlZeroMemory(&Desc, sizeof(Desc));
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

	hr = Device->CreateTexture2D(&Desc, nullptr, &DeskSurface);
	if (FAILED(hr))
		return PrintError("Failed to create DeskSurface.");

	hr = Device->CreateRenderTargetView(DeskSurface, nullptr, &DeskRTView);
	if (FAILED(hr))
		return PrintError("Failed to create render target view for dirty rects");
	
	return true;
}
//
// Process a given frame and its metadata
// 处理一帧数据
//
bool DisplayManager::ProcessFrame(_In_ FRAME_DATA* Data, INT OffsetX, INT OffsetY)
{
	bool Ret = true;

	// Process dirties and moves
	if (Data->FrameInfo.TotalMetadataBufferSize)
	{
		D3D11_TEXTURE2D_DESC Desc;
		Data->Frame->GetDesc(&Desc);

		if (Data->MoveCount)
		{
			Ret = CopyMove(DeskSurface, reinterpret_cast<DXGI_OUTDUPL_MOVE_RECT*>(Data->MetaData), Data->MoveCount, OffsetX, OffsetY, DesktopDesc, Desc.Width, Desc.Height);
			if (!Ret)
				return Ret;
		}

		if (Data->DirtyCount)
			Ret = CopyDirty(Data->Frame, DeskSurface, reinterpret_cast<RECT*>(Data->MetaData + (Data->MoveCount * sizeof(DXGI_OUTDUPL_MOVE_RECT))), Data->DirtyCount, OffsetX, OffsetY, DesktopDesc);
	}

	return Ret;
}
//
// Copy move rectangles
// 复制移动矩形数据
//
bool DisplayManager::CopyMove(_Inout_ ID3D11Texture2D* SharedSurf, _In_reads_(MoveCount) DXGI_OUTDUPL_MOVE_RECT* MoveBuffer, UINT MoveCount, INT OffsetX, INT OffsetY, _In_ DXGI_OUTPUT_DESC* DeskDesc, INT TexWidth, INT TexHeight)
{
	D3D11_TEXTURE2D_DESC FullDesc;
    SharedSurf->GetDesc(&FullDesc);

    // Make new intermediate surface to copy into for moving
    if (!MoveSurf)
    {
        D3D11_TEXTURE2D_DESC MoveDesc;
        MoveDesc = FullDesc;
        MoveDesc.Width = DeskDesc->DesktopCoordinates.right - DeskDesc->DesktopCoordinates.left;
        MoveDesc.Height = DeskDesc->DesktopCoordinates.bottom - DeskDesc->DesktopCoordinates.top;
        MoveDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
        MoveDesc.MiscFlags = 0;
        HRESULT hr = Device->CreateTexture2D(&MoveDesc, nullptr, &MoveSurf);
        if (FAILED(hr))
            return PrintError("Failed to create staging texture for move rects");
    }

    for (UINT i = 0; i < MoveCount; ++i)
    {
        RECT SrcRect;
        RECT DestRect;

        SetMoveRect(&SrcRect, &DestRect, DeskDesc, &(MoveBuffer[i]), TexWidth, TexHeight);

        // Copy rect out of shared surface
        D3D11_BOX Box;
        Box.left = SrcRect.left + DeskDesc->DesktopCoordinates.left - OffsetX;
        Box.top = SrcRect.top + DeskDesc->DesktopCoordinates.top - OffsetY;
        Box.front = 0;
        Box.right = SrcRect.right + DeskDesc->DesktopCoordinates.left - OffsetX;
        Box.bottom = SrcRect.bottom + DeskDesc->DesktopCoordinates.top - OffsetY;
        Box.back = 1;
        DeviceContext->CopySubresourceRegion(MoveSurf, 0, SrcRect.left, SrcRect.top, 0, SharedSurf, 0, &Box);

        // Copy back to shared surface
        Box.left = SrcRect.left;
        Box.top = SrcRect.top;
        Box.front = 0;
        Box.right = SrcRect.right;
        Box.bottom = SrcRect.bottom;
        Box.back = 1;
        DeviceContext->CopySubresourceRegion(SharedSurf, 0, DestRect.left + DeskDesc->DesktopCoordinates.left - OffsetX, DestRect.top + DeskDesc->DesktopCoordinates.top - OffsetY, 0, MoveSurf, 0, &Box);
    }

	return true;
}
//
// Copies dirty rectangles
// 复制脏矩形数据
//
bool DisplayManager::CopyDirty(_In_ ID3D11Texture2D* SrcSurface, _Inout_ ID3D11Texture2D* SharedSurf, _In_reads_(DirtyCount) RECT* DirtyBuffer, UINT DirtyCount, INT OffsetX, INT OffsetY, _In_ DXGI_OUTPUT_DESC* DeskDesc)
{
	HRESULT hr;

	D3D11_TEXTURE2D_DESC FullDesc;
	SharedSurf->GetDesc(&FullDesc);

	D3D11_TEXTURE2D_DESC ThisDesc;
	SrcSurface->GetDesc(&ThisDesc);

	D3D11_SHADER_RESOURCE_VIEW_DESC ShaderDesc;
	RtlZeroMemory(&ShaderDesc, sizeof(ShaderDesc));
	ShaderDesc.Format = ThisDesc.Format;
	ShaderDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	ShaderDesc.Texture2D.MostDetailedMip = ThisDesc.MipLevels - 1;
	ShaderDesc.Texture2D.MipLevels = ThisDesc.MipLevels;

	// Create new shader resource view
	ID3D11ShaderResourceView* ShaderResource = nullptr;
	hr = Device->CreateShaderResourceView(SrcSurface, &ShaderDesc, &ShaderResource);
	if (FAILED(hr))
		return PrintError("Failed to create shader resource view for dirty rects");

	FLOAT BlendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
	DeviceContext->OMSetBlendState(nullptr, BlendFactor, 0xFFFFFFFF);
	DeviceContext->OMSetRenderTargets(1, &DeskRTView, nullptr);
	DeviceContext->VSSetShader(VertexShader, nullptr, 0);
	DeviceContext->PSSetShader(PixelShader, nullptr, 0);
	DeviceContext->PSSetShaderResources(0, 1, &ShaderResource);
	DeviceContext->PSSetSamplers(0, 1, &SamplerLinear);
	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Create space for vertices for the dirty rects if the current space isn't large enough
	UINT BytesNeeded = sizeof(VERTEX) * NUMVERTICES * DirtyCount;
	if (BytesNeeded > DirtyVertexBufferAllocSize)
	{
		if (DirtyVertexBufferAlloc)
			delete[] DirtyVertexBufferAlloc;

		DirtyVertexBufferAlloc = new (std::nothrow) BYTE[BytesNeeded];
		if (!DirtyVertexBufferAlloc)
		{
			DirtyVertexBufferAllocSize = 0;
			return PrintError("Failed to allocate memory for dirty vertex buffer.");
		}

		DirtyVertexBufferAllocSize = BytesNeeded;
	}

	// Fill them in
	VERTEX* DirtyVertex = reinterpret_cast<VERTEX*>(DirtyVertexBufferAlloc);
	for (UINT i = 0; i < DirtyCount; ++i, DirtyVertex += NUMVERTICES)
		SetDirtyVert(DirtyVertex, &(DirtyBuffer[i]), OffsetX, OffsetY, DeskDesc, &FullDesc, &ThisDesc);

	// Create vertex buffer
	D3D11_BUFFER_DESC BufferDesc;
	RtlZeroMemory(&BufferDesc, sizeof(BufferDesc));
	BufferDesc.Usage = D3D11_USAGE_DEFAULT;
	BufferDesc.ByteWidth = BytesNeeded;
	BufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	BufferDesc.CPUAccessFlags = 0;
	D3D11_SUBRESOURCE_DATA InitData;
	RtlZeroMemory(&InitData, sizeof(InitData));
	InitData.pSysMem = DirtyVertexBufferAlloc;

	ID3D11Buffer* VertBuf = nullptr;
	hr = Device->CreateBuffer(&BufferDesc, &InitData, &VertBuf);
	if (FAILED(hr))
		return PrintError("Failed to create vertex buffer in dirty rect processing");

	UINT Stride = sizeof(VERTEX);
	UINT Offset = 0;
	DeviceContext->IASetVertexBuffers(0, 1, &VertBuf, &Stride, &Offset);

	D3D11_VIEWPORT VP;
	RtlZeroMemory(&VP, sizeof(VP));
	VP.Width = static_cast<FLOAT>(FullDesc.Width);
	VP.Height = static_cast<FLOAT>(FullDesc.Height);
	VP.MinDepth = 0.0f;
	VP.MaxDepth = 1.0f;
	VP.TopLeftX = 0.0f;
	VP.TopLeftY = 0.0f;
	DeviceContext->RSSetViewports(1, &VP);

	DeviceContext->Draw(NUMVERTICES * DirtyCount, 0);

	VertBuf->Release();
	VertBuf = nullptr;

	ShaderResource->Release();
	ShaderResource = nullptr;

	return true;
}
//
// Set appropriate source and destination rects for move rects
//
void DisplayManager::SetMoveRect(_Out_ RECT* SrcRect, _Out_ RECT* DestRect, _In_ DXGI_OUTPUT_DESC* DeskDesc, _In_ DXGI_OUTDUPL_MOVE_RECT* MoveRect, INT TexWidth, INT TexHeight)
{
	switch (DeskDesc->Rotation)
	{
	case DXGI_MODE_ROTATION_UNSPECIFIED:
	case DXGI_MODE_ROTATION_IDENTITY:
	{
		SrcRect->left = MoveRect->SourcePoint.x;
		SrcRect->top = MoveRect->SourcePoint.y;
		SrcRect->right = MoveRect->SourcePoint.x + MoveRect->DestinationRect.right - MoveRect->DestinationRect.left;
		SrcRect->bottom = MoveRect->SourcePoint.y + MoveRect->DestinationRect.bottom - MoveRect->DestinationRect.top;

		*DestRect = MoveRect->DestinationRect;
		break;
	}
	case DXGI_MODE_ROTATION_ROTATE90:
	{
		SrcRect->left = TexHeight - (MoveRect->SourcePoint.y + MoveRect->DestinationRect.bottom - MoveRect->DestinationRect.top);
		SrcRect->top = MoveRect->SourcePoint.x;
		SrcRect->right = TexHeight - MoveRect->SourcePoint.y;
		SrcRect->bottom = MoveRect->SourcePoint.x + MoveRect->DestinationRect.right - MoveRect->DestinationRect.left;

		DestRect->left = TexHeight - MoveRect->DestinationRect.bottom;
		DestRect->top = MoveRect->DestinationRect.left;
		DestRect->right = TexHeight - MoveRect->DestinationRect.top;
		DestRect->bottom = MoveRect->DestinationRect.right;
		break;
	}
	case DXGI_MODE_ROTATION_ROTATE180:
	{
		SrcRect->left = TexWidth - (MoveRect->SourcePoint.x + MoveRect->DestinationRect.right - MoveRect->DestinationRect.left);
		SrcRect->top = TexHeight - (MoveRect->SourcePoint.y + MoveRect->DestinationRect.bottom - MoveRect->DestinationRect.top);
		SrcRect->right = TexWidth - MoveRect->SourcePoint.x;
		SrcRect->bottom = TexHeight - MoveRect->SourcePoint.y;

		DestRect->left = TexWidth - MoveRect->DestinationRect.right;
		DestRect->top = TexHeight - MoveRect->DestinationRect.bottom;
		DestRect->right = TexWidth - MoveRect->DestinationRect.left;
		DestRect->bottom = TexHeight - MoveRect->DestinationRect.top;
		break;
	}
	case DXGI_MODE_ROTATION_ROTATE270:
	{
		SrcRect->left = MoveRect->SourcePoint.x;
		SrcRect->top = TexWidth - (MoveRect->SourcePoint.x + MoveRect->DestinationRect.right - MoveRect->DestinationRect.left);
		SrcRect->right = MoveRect->SourcePoint.y + MoveRect->DestinationRect.bottom - MoveRect->DestinationRect.top;
		SrcRect->bottom = TexWidth - MoveRect->SourcePoint.x;

		DestRect->left = MoveRect->DestinationRect.top;
		DestRect->top = TexWidth - MoveRect->DestinationRect.right;
		DestRect->right = MoveRect->DestinationRect.bottom;
		DestRect->bottom = TexWidth - MoveRect->DestinationRect.left;
		break;
	}
	default:
	{
		RtlZeroMemory(DestRect, sizeof(RECT));
		RtlZeroMemory(SrcRect, sizeof(RECT));
		break;
	}
	}
}
//
// Sets up vertices for dirty rects for rotated desktops
//
#pragma warning(push)
#pragma warning(disable:__WARNING_USING_UNINIT_VAR) // false positives in SetDirtyVert due to tool bug

void DisplayManager::SetDirtyVert(_Out_writes_(NUMVERTICES) VERTEX* Vertices, _In_ RECT* Dirty, INT OffsetX, INT OffsetY, _In_ DXGI_OUTPUT_DESC* DeskDesc, _In_ D3D11_TEXTURE2D_DESC* FullDesc, _In_ D3D11_TEXTURE2D_DESC* ThisDesc)
{
	INT CenterX = FullDesc->Width / 2;
	INT CenterY = FullDesc->Height / 2;

	INT Width = DeskDesc->DesktopCoordinates.right - DeskDesc->DesktopCoordinates.left;
	INT Height = DeskDesc->DesktopCoordinates.bottom - DeskDesc->DesktopCoordinates.top;

	// Rotation compensated destination rect
	RECT DestDirty = *Dirty;

	// Set appropriate coordinates compensated for rotation
	switch (DeskDesc->Rotation)
	{
	case DXGI_MODE_ROTATION_ROTATE90:
	{
		DestDirty.left = Width - Dirty->bottom;
		DestDirty.top = Dirty->left;
		DestDirty.right = Width - Dirty->top;
		DestDirty.bottom = Dirty->right;

		Vertices[0].TexCoord = DirectX::XMFLOAT2(Dirty->right / static_cast<FLOAT>(ThisDesc->Width), Dirty->bottom / static_cast<FLOAT>(ThisDesc->Height));
		Vertices[1].TexCoord = DirectX::XMFLOAT2(Dirty->left / static_cast<FLOAT>(ThisDesc->Width), Dirty->bottom / static_cast<FLOAT>(ThisDesc->Height));
		Vertices[2].TexCoord = DirectX::XMFLOAT2(Dirty->right / static_cast<FLOAT>(ThisDesc->Width), Dirty->top / static_cast<FLOAT>(ThisDesc->Height));
		Vertices[5].TexCoord = DirectX::XMFLOAT2(Dirty->left / static_cast<FLOAT>(ThisDesc->Width), Dirty->top / static_cast<FLOAT>(ThisDesc->Height));
		break;
	}
	case DXGI_MODE_ROTATION_ROTATE180:
	{
		DestDirty.left = Width - Dirty->right;
		DestDirty.top = Height - Dirty->bottom;
		DestDirty.right = Width - Dirty->left;
		DestDirty.bottom = Height - Dirty->top;

		Vertices[0].TexCoord = DirectX::XMFLOAT2(Dirty->right / static_cast<FLOAT>(ThisDesc->Width), Dirty->top / static_cast<FLOAT>(ThisDesc->Height));
		Vertices[1].TexCoord = DirectX::XMFLOAT2(Dirty->right / static_cast<FLOAT>(ThisDesc->Width), Dirty->bottom / static_cast<FLOAT>(ThisDesc->Height));
		Vertices[2].TexCoord = DirectX::XMFLOAT2(Dirty->left / static_cast<FLOAT>(ThisDesc->Width), Dirty->top / static_cast<FLOAT>(ThisDesc->Height));
		Vertices[5].TexCoord = DirectX::XMFLOAT2(Dirty->left / static_cast<FLOAT>(ThisDesc->Width), Dirty->bottom / static_cast<FLOAT>(ThisDesc->Height));
		break;
	}
	case DXGI_MODE_ROTATION_ROTATE270:
	{
		DestDirty.left = Dirty->top;
		DestDirty.top = Height - Dirty->right;
		DestDirty.right = Dirty->bottom;
		DestDirty.bottom = Height - Dirty->left;

		Vertices[0].TexCoord = DirectX::XMFLOAT2(Dirty->left / static_cast<FLOAT>(ThisDesc->Width), Dirty->top / static_cast<FLOAT>(ThisDesc->Height));
		Vertices[1].TexCoord = DirectX::XMFLOAT2(Dirty->right / static_cast<FLOAT>(ThisDesc->Width), Dirty->top / static_cast<FLOAT>(ThisDesc->Height));
		Vertices[2].TexCoord = DirectX::XMFLOAT2(Dirty->left / static_cast<FLOAT>(ThisDesc->Width), Dirty->bottom / static_cast<FLOAT>(ThisDesc->Height));
		Vertices[5].TexCoord = DirectX::XMFLOAT2(Dirty->right / static_cast<FLOAT>(ThisDesc->Width), Dirty->bottom / static_cast<FLOAT>(ThisDesc->Height));
		break;
	}
	default:
		assert(false); // drop through
	case DXGI_MODE_ROTATION_UNSPECIFIED:
	case DXGI_MODE_ROTATION_IDENTITY:
	{
		Vertices[0].TexCoord = DirectX::XMFLOAT2(Dirty->left / static_cast<FLOAT>(ThisDesc->Width), Dirty->bottom / static_cast<FLOAT>(ThisDesc->Height));
		Vertices[1].TexCoord = DirectX::XMFLOAT2(Dirty->left / static_cast<FLOAT>(ThisDesc->Width), Dirty->top / static_cast<FLOAT>(ThisDesc->Height));
		Vertices[2].TexCoord = DirectX::XMFLOAT2(Dirty->right / static_cast<FLOAT>(ThisDesc->Width), Dirty->bottom / static_cast<FLOAT>(ThisDesc->Height));
		Vertices[5].TexCoord = DirectX::XMFLOAT2(Dirty->right / static_cast<FLOAT>(ThisDesc->Width), Dirty->top / static_cast<FLOAT>(ThisDesc->Height));
		break;
	}
	}
	 
	// Set positions
	FLOAT X = 0.0, Y = 0.0, Z = 0.0;
	X = (DestDirty.left + DeskDesc->DesktopCoordinates.left - OffsetX - CenterX) / static_cast<FLOAT>(CenterX);
	Y = (DestDirty.bottom + DeskDesc->DesktopCoordinates.top - OffsetY - CenterY) / static_cast<FLOAT>(CenterY) * -1;
	Vertices[0].Pos = DirectX::XMFLOAT3(X,Y,Z);

	Y = (DestDirty.top + DeskDesc->DesktopCoordinates.top - OffsetY - CenterY) / static_cast<FLOAT>(CenterY) * -1;
	Vertices[1].Pos = DirectX::XMFLOAT3(X,Y,Z);

	X = (DestDirty.right + DeskDesc->DesktopCoordinates.left - OffsetX - CenterX) / static_cast<FLOAT>(CenterX);
	Y = (DestDirty.bottom + DeskDesc->DesktopCoordinates.top - OffsetY - CenterY) / static_cast<FLOAT>(CenterY) * -1;
	Vertices[2].Pos = DirectX::XMFLOAT3(X,Y,Z);

	Vertices[3].Pos = Vertices[2].Pos;
	Vertices[4].Pos = Vertices[1].Pos;

	Y = (DestDirty.top + DeskDesc->DesktopCoordinates.top - OffsetY - CenterY) / static_cast<FLOAT>(CenterY) * -1;
	Vertices[5].Pos = DirectX::XMFLOAT3(X,Y,Z);

	Vertices[3].TexCoord = Vertices[2].TexCoord;
	Vertices[4].TexCoord = Vertices[1].TexCoord;
}
#pragma warning(pop) // re-enable __WARNING_USING_UNINIT_VAR

//
// 绘制一帧数据到 SharedSurf
//
bool DisplayManager::DrawFrame(_Inout_ ID3D11Texture2D* SharedSurf)
{
	HRESULT hr;

	// Vertices for drawing whole texture
	VERTEX Vertices[NUMVERTICES] =
	{
		{DirectX::XMFLOAT3(-1.0f, -1.0f, 0),DirectX::XMFLOAT2(0.0f, 1.0f)},
		{DirectX::XMFLOAT3(-1.0f, 1.0f, 0), DirectX::XMFLOAT2(0.0f, 0.0f)},
		{DirectX::XMFLOAT3(1.0f, -1.0f, 0), DirectX::XMFLOAT2(1.0f, 1.0f)},
		{DirectX::XMFLOAT3(1.0f, -1.0f, 0), DirectX::XMFLOAT2(1.0f, 1.0f)},
		{DirectX::XMFLOAT3(-1.0f, 1.0f, 0), DirectX::XMFLOAT2(0.0f, 0.0f)},
		{DirectX::XMFLOAT3(1.0f, 1.0f, 0),  DirectX::XMFLOAT2(1.0f, 0.0f)},
	};

	if (!SharedRTView)
	{
		hr = Device->CreateRenderTargetView(SharedSurf, nullptr, &SharedRTView);
		if (FAILED(hr))
			return PrintError("Failed to create render target view for dirty rects");
	}

	D3D11_TEXTURE2D_DESC FrameDesc;
	DeskSurface->GetDesc(&FrameDesc);

	D3D11_SHADER_RESOURCE_VIEW_DESC ShaderDesc;
	RtlZeroMemory(&ShaderDesc, sizeof(ShaderDesc));
	ShaderDesc.Format = FrameDesc.Format;
	ShaderDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	ShaderDesc.Texture2D.MostDetailedMip = FrameDesc.MipLevels - 1;
	ShaderDesc.Texture2D.MipLevels = FrameDesc.MipLevels;

	// Create new shader resource view
	ID3D11ShaderResourceView* ShaderResource = nullptr;
	hr = Device->CreateShaderResourceView(DeskSurface, &ShaderDesc, &ShaderResource);
	if (FAILED(hr))
		return PrintError("Failed to create shader resource when drawing a frame");

	// Set resources
	UINT Stride = sizeof(VERTEX);
	UINT Offset = 0;
	FLOAT blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
	DeviceContext->OMSetBlendState(nullptr, blendFactor, 0xffffffff);
	DeviceContext->OMSetRenderTargets(1, &SharedRTView, nullptr);
	DeviceContext->VSSetShader(VertexShader, nullptr, 0);
	DeviceContext->PSSetShader(PixelShader, nullptr, 0);
	DeviceContext->PSSetShaderResources(0, 1, &ShaderResource);
	DeviceContext->PSSetSamplers(0, 1, &SamplerLinear);
	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	D3D11_BUFFER_DESC BufferDesc;
	RtlZeroMemory(&BufferDesc, sizeof(BufferDesc));
	BufferDesc.Usage = D3D11_USAGE_DEFAULT;
	BufferDesc.ByteWidth = sizeof(VERTEX) * NUMVERTICES;
	BufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	BufferDesc.CPUAccessFlags = 0;
	D3D11_SUBRESOURCE_DATA InitData;
	RtlZeroMemory(&InitData, sizeof(InitData));
	InitData.pSysMem = Vertices;

	ID3D11Buffer* VertexBuffer = nullptr;

	// Create vertex buffer
	hr = Device->CreateBuffer(&BufferDesc, &InitData, &VertexBuffer);
	if (FAILED(hr))
	{
		ShaderResource->Release();
		ShaderResource = nullptr;
		return PrintError("Failed to create vertex buffer when drawing a frame");
	}
	DeviceContext->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);

	// Draw textured quad onto render target
	DeviceContext->Draw(NUMVERTICES, 0);

	VertexBuffer->Release();
	VertexBuffer = nullptr;

	// Release shader resource
	ShaderResource->Release();
	ShaderResource = nullptr;

	return true;
}
//
// Draw mouse provided in buffer to backbuffer
// 绘制鼠标
//
bool DisplayManager::DrawMouse(_In_ PTR_INFO* PtrInfo, _Inout_ ID3D11Texture2D* SharedSurf)
{
	HRESULT hr;
	// Vars to be used
	ID3D11Texture2D* MouseTex = nullptr;
	ID3D11ShaderResourceView* ShaderRes = nullptr;
	ID3D11Buffer* VertexBufferMouse = nullptr;

	// Position will be changed based on mouse position
	VERTEX Vertices[NUMVERTICES] =
	{
		{DirectX::XMFLOAT3(-1.0f, -1.0f, 0), DirectX::XMFLOAT2(0.0f, 1.0f)},
		{DirectX::XMFLOAT3(-1.0f, 1.0f, 0), DirectX::XMFLOAT2(0.0f, 0.0f)},
		{DirectX::XMFLOAT3(1.0f, -1.0f, 0), DirectX::XMFLOAT2(1.0f, 1.0f)},
		{DirectX::XMFLOAT3(1.0f, -1.0f, 0), DirectX::XMFLOAT2(1.0f, 1.0f)},
		{DirectX::XMFLOAT3(-1.0f, 1.0f, 0), DirectX::XMFLOAT2(0.0f, 0.0f)},
		{DirectX::XMFLOAT3(1.0f, 1.0f, 0),  DirectX::XMFLOAT2(1.0f, 0.0f)},
	};

	if (!SharedRTView)
	{
		hr = Device->CreateRenderTargetView(SharedSurf, nullptr, &SharedRTView);
		if (FAILED(hr))
			return PrintError("Failed to create render target view for dirty rects");
	}

	D3D11_TEXTURE2D_DESC FullDesc;
	SharedSurf->GetDesc(&FullDesc);
	INT DesktopWidth = FullDesc.Width;
	INT DesktopHeight = FullDesc.Height;

	// Center of desktop dimensions
	INT CenterX = (DesktopWidth / 2);
	INT CenterY = (DesktopHeight / 2);

	// Clipping adjusted coordinates / dimensions
	INT PtrWidth = 0;
	INT PtrHeight = 0;
	INT PtrLeft = 0;
	INT PtrTop = 0;

	// Buffer used if necessary (in case of monochrome or masked pointer)
	BYTE* InitBuffer = nullptr;

	// Used for copying pixels
	D3D11_BOX Box;
	RtlZeroMemory(&Box, sizeof(Box));
	Box.front = 0;
	Box.back = 1;

	D3D11_TEXTURE2D_DESC Desc;
	RtlZeroMemory(&Desc, sizeof(Desc));
	Desc.MipLevels = 1;
	Desc.ArraySize = 1;
	Desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	Desc.SampleDesc.Count = 1;
	Desc.SampleDesc.Quality = 0;
	Desc.Usage = D3D11_USAGE_DEFAULT;
	Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	Desc.CPUAccessFlags = 0;
	Desc.MiscFlags = 0;

	// Set shader resource properties
	D3D11_SHADER_RESOURCE_VIEW_DESC SDesc;
	RtlZeroMemory(&SDesc, sizeof(SDesc));
	SDesc.Format = Desc.Format;
	SDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SDesc.Texture2D.MostDetailedMip = Desc.MipLevels - 1;
	SDesc.Texture2D.MipLevels = Desc.MipLevels;

	switch (PtrInfo->ShapeInfo.Type)
	{
	case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR:
	{
		PtrLeft = PtrInfo->Position.x;
		PtrTop = PtrInfo->Position.y;

		PtrWidth = static_cast<INT>(PtrInfo->ShapeInfo.Width);
		PtrHeight = static_cast<INT>(PtrInfo->ShapeInfo.Height);
		break;
	}

	case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME:
	{
		if (!ProcessMonoMask(true, PtrInfo, SharedSurf, &PtrWidth, &PtrHeight, &PtrLeft, &PtrTop, &InitBuffer, &Box))
			return false;
		break;
	}

	case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MASKED_COLOR:
	{
		if (!ProcessMonoMask(false, PtrInfo, SharedSurf, &PtrWidth, &PtrHeight, &PtrLeft, &PtrTop, &InitBuffer, &Box))
			return false;
		break;
	}

	default:
		break;
	}

	// VERTEX creation
	Vertices[0].Pos.x = (PtrLeft - CenterX) / (FLOAT)CenterX;
	Vertices[0].Pos.y = -1 * ((PtrTop + PtrHeight) - CenterY) / (FLOAT)CenterY;
	Vertices[1].Pos.x = (PtrLeft - CenterX) / (FLOAT)CenterX;
	Vertices[1].Pos.y = -1 * (PtrTop - CenterY) / (FLOAT)CenterY;
	Vertices[2].Pos.x = ((PtrLeft + PtrWidth) - CenterX) / (FLOAT)CenterX;
	Vertices[2].Pos.y = -1 * ((PtrTop + PtrHeight) - CenterY) / (FLOAT)CenterY;
	Vertices[3].Pos.x = Vertices[2].Pos.x;
	Vertices[3].Pos.y = Vertices[2].Pos.y;
	Vertices[4].Pos.x = Vertices[1].Pos.x;
	Vertices[4].Pos.y = Vertices[1].Pos.y;
	Vertices[5].Pos.x = ((PtrLeft + PtrWidth) - CenterX) / (FLOAT)CenterX;
	Vertices[5].Pos.y = -1 * (PtrTop - CenterY) / (FLOAT)CenterY;

	// Set texture properties
	Desc.Width = PtrWidth;
	Desc.Height = PtrHeight;

	// Set up init data
	D3D11_SUBRESOURCE_DATA InitData;
	RtlZeroMemory(&InitData, sizeof(InitData));
	InitData.pSysMem = (PtrInfo->ShapeInfo.Type & DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR) ? PtrInfo->PtrShapeBuffer : InitBuffer;
	InitData.SysMemPitch = (PtrInfo->ShapeInfo.Type & DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR) ? PtrInfo->ShapeInfo.Pitch : PtrWidth * BPP;
	InitData.SysMemSlicePitch = 0;

	// Create mouseshape as texture
	hr = Device->CreateTexture2D(&Desc, &InitData, &MouseTex);
	if (FAILED(hr))
		return PrintError("Failed to create mouse pointer texture");

	// Create shader resource from texture
	hr = Device->CreateShaderResourceView(MouseTex, &SDesc, &ShaderRes);
	if (FAILED(hr))
	{
		MouseTex->Release();
		MouseTex = nullptr;
		return PrintError("Failed to create shader resource from mouse pointer texture");
	}

	D3D11_BUFFER_DESC BDesc;
	ZeroMemory(&BDesc, sizeof(D3D11_BUFFER_DESC));
	BDesc.Usage = D3D11_USAGE_DEFAULT;
	BDesc.ByteWidth = sizeof(VERTEX) * NUMVERTICES;
	BDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	BDesc.CPUAccessFlags = 0;

	ZeroMemory(&InitData, sizeof(D3D11_SUBRESOURCE_DATA));
	InitData.pSysMem = Vertices;

	// Create vertex buffer
	hr = Device->CreateBuffer(&BDesc, &InitData, &VertexBufferMouse);
	if (FAILED(hr))
	{
		ShaderRes->Release();
		ShaderRes = nullptr;
		MouseTex->Release();
		MouseTex = nullptr;
		return PrintError("Failed to create mouse pointer vertex buffer in OutputManager");
	}

	// Set resources
	FLOAT BlendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
	UINT Stride = sizeof(VERTEX);
	UINT Offset = 0;
	DeviceContext->IASetVertexBuffers(0, 1, &VertexBufferMouse, &Stride, &Offset);
	DeviceContext->OMSetBlendState(BlendState, BlendFactor, 0xFFFFFFFF);
	DeviceContext->OMSetRenderTargets(1, &SharedRTView, nullptr);
	DeviceContext->VSSetShader(VertexShader, nullptr, 0);
	DeviceContext->PSSetShader(PixelShader, nullptr, 0);
	DeviceContext->PSSetShaderResources(0, 1, &ShaderRes);
	DeviceContext->PSSetSamplers(0, 1, &SamplerLinear);

	// Draw
	DeviceContext->Draw(NUMVERTICES, 0);

	// Clean
	if (VertexBufferMouse)
	{
		VertexBufferMouse->Release();
		VertexBufferMouse = nullptr;
	}
	if (ShaderRes)
	{
		ShaderRes->Release();
		ShaderRes = nullptr;
	}
	if (MouseTex)
	{
		MouseTex->Release();
		MouseTex = nullptr;
	}
	if (InitBuffer)
	{
		delete[] InitBuffer;
		InitBuffer = nullptr;
	}

	return true;
}
//
// Process both masked and monochrome pointers
//
bool DisplayManager::ProcessMonoMask(bool IsMono, _Inout_ PTR_INFO* PtrInfo, _In_ ID3D11Texture2D* SharedSurf, _Out_ INT* PtrWidth, _Out_ INT* PtrHeight, _Out_ INT* PtrLeft, _Out_ INT* PtrTop, _Outptr_result_bytebuffer_(*PtrHeight** PtrWidth* BPP) BYTE** InitBuffer, _Out_ D3D11_BOX* Box)
{
	// Desktop dimensions
	D3D11_TEXTURE2D_DESC FullDesc;
	SharedSurf->GetDesc(&FullDesc);
	INT DesktopWidth = FullDesc.Width;
	INT DesktopHeight = FullDesc.Height;

	// Pointer position
	INT GivenLeft = PtrInfo->Position.x;
	INT GivenTop = PtrInfo->Position.y;

	// Figure out if any adjustment is needed for out of bound positions
	if (GivenLeft < 0)
		*PtrWidth = GivenLeft + static_cast<INT>(PtrInfo->ShapeInfo.Width);
	else if ((GivenLeft + static_cast<INT>(PtrInfo->ShapeInfo.Width)) > DesktopWidth)
		*PtrWidth = DesktopWidth - GivenLeft;
	else
		*PtrWidth = static_cast<INT>(PtrInfo->ShapeInfo.Width);

	if (IsMono)
		PtrInfo->ShapeInfo.Height = PtrInfo->ShapeInfo.Height / 2;

	if (GivenTop < 0)
		*PtrHeight = GivenTop + static_cast<INT>(PtrInfo->ShapeInfo.Height);
	else if ((GivenTop + static_cast<INT>(PtrInfo->ShapeInfo.Height)) > DesktopHeight)
		*PtrHeight = DesktopHeight - GivenTop;
	else
		*PtrHeight = static_cast<INT>(PtrInfo->ShapeInfo.Height);

	if (IsMono)
		PtrInfo->ShapeInfo.Height = PtrInfo->ShapeInfo.Height * 2;

	*PtrLeft = (GivenLeft < 0) ? 0 : GivenLeft;
	*PtrTop = (GivenTop < 0) ? 0 : GivenTop;

	// Staging buffer/texture
	D3D11_TEXTURE2D_DESC CopyBufferDesc;
	RtlZeroMemory(&CopyBufferDesc, sizeof(CopyBufferDesc));
	CopyBufferDesc.Width = *PtrWidth;
	CopyBufferDesc.Height = *PtrHeight;
	CopyBufferDesc.MipLevels = 1;
	CopyBufferDesc.ArraySize = 1;
	CopyBufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	CopyBufferDesc.SampleDesc.Count = 1;
	CopyBufferDesc.SampleDesc.Quality = 0;
	CopyBufferDesc.Usage = D3D11_USAGE_STAGING;
	CopyBufferDesc.BindFlags = 0;
	CopyBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	CopyBufferDesc.MiscFlags = 0;

	ID3D11Texture2D* CopyBuffer = nullptr;
	HRESULT hr = Device->CreateTexture2D(&CopyBufferDesc, nullptr, &CopyBuffer);
	if (FAILED(hr))
		return PrintError("Failed creating staging texture for pointer");

	// Copy needed part of desktop image
	Box->left = *PtrLeft;
	Box->top = *PtrTop;
	Box->right = *PtrLeft + *PtrWidth;
	Box->bottom = *PtrTop + *PtrHeight;
	DeviceContext->CopySubresourceRegion(CopyBuffer, 0, 0, 0, 0, SharedSurf, 0, Box);

	// QI for IDXGISurface
	IDXGISurface* CopySurface = nullptr;
	hr = CopyBuffer->QueryInterface(__uuidof(IDXGISurface), (void**)&CopySurface);
	CopyBuffer->Release();
	CopyBuffer = nullptr;
	if (FAILED(hr))
		return PrintError("Failed to QI staging texture into IDXGISurface for pointer");

	// Map pixels
	DXGI_MAPPED_RECT MappedSurface;
	hr = CopySurface->Map(&MappedSurface, DXGI_MAP_READ);
	if (FAILED(hr))
	{
		CopySurface->Release();
		CopySurface = nullptr;
		return PrintError("Failed to map surface for pointer");
	}

	// New mouseshape buffer
	*InitBuffer = new (std::nothrow) BYTE[*PtrWidth * *PtrHeight * BPP];
	if (!(*InitBuffer))
		return PrintError("Failed to allocate memory for new mouse shape buffer.");

	UINT* InitBuffer32 = reinterpret_cast<UINT*>(*InitBuffer);
	UINT* Desktop32 = reinterpret_cast<UINT*>(MappedSurface.pBits);
	UINT  DesktopPitchInPixels = MappedSurface.Pitch / sizeof(UINT);

	// What to skip (pixel offset)
	UINT SkipX = (GivenLeft < 0) ? (-1 * GivenLeft) : (0);
	UINT SkipY = (GivenTop < 0) ? (-1 * GivenTop) : (0);

	if (IsMono)
	{
		for (INT Row = 0; Row < *PtrHeight; ++Row)
		{
			// Set mask
			BYTE Mask = 0x80;
			Mask = Mask >> (SkipX % 8);
			for (INT Col = 0; Col < *PtrWidth; ++Col)
			{
				// Get masks using appropriate offsets
				BYTE AndMask = PtrInfo->PtrShapeBuffer[((Col + SkipX) / 8) + ((Row + SkipY) * (PtrInfo->ShapeInfo.Pitch))] & Mask;
				BYTE XorMask = PtrInfo->PtrShapeBuffer[((Col + SkipX) / 8) + ((Row + SkipY + (PtrInfo->ShapeInfo.Height / 2)) * (PtrInfo->ShapeInfo.Pitch))] & Mask;
				UINT AndMask32 = (AndMask) ? 0xFFFFFFFF : 0xFF000000;
				UINT XorMask32 = (XorMask) ? 0x00FFFFFF : 0x00000000;

				// Set new pixel
				InitBuffer32[(Row * *PtrWidth) + Col] = (Desktop32[(Row * DesktopPitchInPixels) + Col] & AndMask32) ^ XorMask32;

				// Adjust mask
				if (Mask == 0x01)
					Mask = 0x80;
				else
					Mask = Mask >> 1;
			}
		}
	}
	else
	{
		UINT* Buffer32 = reinterpret_cast<UINT*>(PtrInfo->PtrShapeBuffer);

		// Iterate through pixels
		for (INT Row = 0; Row < *PtrHeight; ++Row)
		{
			for (INT Col = 0; Col < *PtrWidth; ++Col)
			{
				// Set up mask
				UINT MaskVal = 0xFF000000 & Buffer32[(Col + SkipX) + ((Row + SkipY) * (PtrInfo->ShapeInfo.Pitch / sizeof(UINT)))];
				if (MaskVal)
					// Mask was 0xFF
					InitBuffer32[(Row * *PtrWidth) + Col] = (Desktop32[(Row * DesktopPitchInPixels) + Col] ^ Buffer32[(Col + SkipX) + ((Row + SkipY) * (PtrInfo->ShapeInfo.Pitch / sizeof(UINT)))]) | 0xFF000000;
				else
					// Mask was 0x00
					InitBuffer32[(Row * *PtrWidth) + Col] = Buffer32[(Col + SkipX) + ((Row + SkipY) * (PtrInfo->ShapeInfo.Pitch / sizeof(UINT)))] | 0xFF000000;
			}
		}
	}

	// Done with resource
	hr = CopySurface->Unmap();
	CopySurface->Release();
	CopySurface = nullptr;
	if (FAILED(hr))
		return PrintError("Failed to unmap surface for pointer");

	return true;
}

bool DisplayManager::PrintError(const std::string& er)
{
	Debug::PrintError("[DisplayManager]: ", er);
	return false;
}

void DisplayManager::Destroy()
{
	if (InputLayout)
	{
		InputLayout->Release();
		InputLayout = nullptr;
	}
	if (PixelShader)
	{
		PixelShader->Release();
		PixelShader = nullptr;
	}
	if (SamplerLinear)
	{
		SamplerLinear->Release();
		SamplerLinear = nullptr;
	}
	if (VertexShader)
	{
		VertexShader->Release();
		VertexShader = nullptr;
	}
	if (DirtyVertexBufferAlloc)
	{
		delete[] DirtyVertexBufferAlloc;
		DirtyVertexBufferAlloc = nullptr;
	}
	if (DeskRTView)
	{
		DeskRTView->Release();
		DeskRTView = nullptr;
	}
	if (SharedRTView)
	{
		SharedRTView->Release();
		SharedRTView = nullptr;
	}
	if (MoveSurf)
	{
		MoveSurf->Release();
		MoveSurf = nullptr;
	}
	if (BlendState)
	{
		BlendState->Release();
		BlendState = nullptr;
	}
	if (DeskSurface)
	{
		DeskSurface->Release();
		DeskSurface = nullptr;
	}
	if (DeviceContext)
	{
		DeviceContext->Release();
		DeviceContext = nullptr;
	}
	if (Device)
	{
		Device->Release();
		Device = nullptr;
	}
}
