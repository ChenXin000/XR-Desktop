// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#include "DuplicationManager.h"

#pragma comment(lib,"User32.lib")
//
// Constructor sets up references / variables
//
DuplicationManager::DuplicationManager() 
    : DeskDuplication(nullptr)
    , AcquiredDesktopImage(nullptr)
    , MetaDataBuffer(nullptr)
    , MetaDataSize(0)
    , DisplayIndex(0)
    , Device(nullptr)
    , DxgiOutput1(nullptr)
{
    memset(&OutputDesc, 0, sizeof(OutputDesc));
}
//
// Destructor simply calls CleanRefs to destroy everything
//
DuplicationManager::~DuplicationManager()
{
    Destroy();
}
bool DuplicationManager::CreateDuplication()
{
    HRESULT hr = DxgiOutput1->DuplicateOutput(Device, &DeskDuplication);

    if (FAILED(hr))
    {
        if (hr == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE)
        {
            MessageBoxW(nullptr, L"There is already the maximum number of applications using the Desktop Duplication API running, please close one of those applications and then try again.", L"Error", MB_OK);
            return false;
        }
        // 多显卡第一次运行会出现 DXGI_ERROR_UNSUPPORTED （0x887A0004L）
        return PrintError("Failed to get duplicate output in DuplicationManager");
    }
    return true;
}

bool DuplicationManager::ResetDuplication()
{
    if (DeskDuplication)
    {
        DeskDuplication->Release();
        DeskDuplication = nullptr;
    }
    return CreateDuplication();
}

//
// Initialize duplication interfaces
//
bool DuplicationManager::InitDuplication(_In_ ID3D11Device* Dev, UINT DispIndex)
{
    DisplayIndex = DispIndex;

    // Take a reference on the device
    Device = Dev;
    Device->AddRef();

    // Get DXGI device
    IDXGIDevice* DxgiDevice = nullptr;
    HRESULT hr = Device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&DxgiDevice));
    if (FAILED(hr))
        return PrintError("Failed to QI for DXGI Device");

    // Get DXGI adapter
    IDXGIAdapter* DxgiAdapter = nullptr;
    hr = DxgiDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(&DxgiAdapter));
    DxgiDevice->Release();
    DxgiDevice = nullptr;
    if (FAILED(hr))
        return PrintError("Failed to get parent DXGI Adapter");

    // Get output
    IDXGIOutput* DxgiOutput = nullptr;
    hr = DxgiAdapter->EnumOutputs(DispIndex, &DxgiOutput);
    DxgiAdapter->Release();
    DxgiAdapter = nullptr;
    if (FAILED(hr))
        return PrintError("Failed to get specified output in DuplicationManager");

    DxgiOutput->GetDesc(&OutputDesc);

    // QI for Output 1
    hr = DxgiOutput->QueryInterface(__uuidof(DxgiOutput1), reinterpret_cast<void**>(&DxgiOutput1));
    DxgiOutput->Release();
    DxgiOutput = nullptr;
    if (FAILED(hr))
        return PrintError("Failed to QI for DxgiOutput1 in DuplicationManager");

    // Create desktop duplication
    return CreateDuplication();
}
void DuplicationManager::Destroy()
{
    if (DxgiOutput1)
    {
        DxgiOutput1->Release();
        DxgiOutput1 = nullptr;
    }
    if (DeskDuplication)
    {
        DeskDuplication->Release();
        DeskDuplication = nullptr;
    }

    if (AcquiredDesktopImage)
    {
        AcquiredDesktopImage->Release();
        AcquiredDesktopImage = nullptr;
    }

    if (MetaDataBuffer)
    {
        delete[] MetaDataBuffer;
        MetaDataBuffer = nullptr;
    }

    if (Device)
    {
        Device->Release();
        Device = nullptr;
    }
}
//
// Retrieves mouse info and write it into PtrInfo
// 获取鼠标信息
//
HRESULT DuplicationManager::GetMouse(_Inout_ PTR_INFO* PtrInfo, _In_ DXGI_OUTDUPL_FRAME_INFO* FrameInfo, INT OffsetX, INT OffsetY)
{
    // A non-zero mouse update timestamp indicates that there is a mouse position update and optionally a shape change
    if (FrameInfo->LastMouseUpdateTime.QuadPart == 0)
        return 0;

    bool UpdatePosition = true;

    // Make sure we don't update pointer position wrongly
    // If pointer is invisible, make sure we did not get an update from another output that the last time that said pointer
    // was visible, if so, don't set it to invisible or update.
    if (!FrameInfo->PointerPosition.Visible && (PtrInfo->WhoUpdatedPositionLast != DisplayIndex))
        UpdatePosition = false;

    // If two outputs both say they have a visible, only update if new update has newer timestamp
    if (FrameInfo->PointerPosition.Visible && PtrInfo->Visible && (PtrInfo->WhoUpdatedPositionLast != DisplayIndex) && (PtrInfo->LastTimeStamp.QuadPart > FrameInfo->LastMouseUpdateTime.QuadPart))
        UpdatePosition = false;

    // Update position
    if (UpdatePosition)
    {
        PtrInfo->Position.x = FrameInfo->PointerPosition.Position.x + OutputDesc.DesktopCoordinates.left - OffsetX;
        PtrInfo->Position.y = FrameInfo->PointerPosition.Position.y + OutputDesc.DesktopCoordinates.top - OffsetY;
        PtrInfo->WhoUpdatedPositionLast = DisplayIndex;
        PtrInfo->LastTimeStamp = FrameInfo->LastMouseUpdateTime;
        PtrInfo->Visible = FrameInfo->PointerPosition.Visible != 0;
    }

    // No new shape
    if (FrameInfo->PointerShapeBufferSize == 0)
        return 0;

    // Old buffer too small
    if (FrameInfo->PointerShapeBufferSize > PtrInfo->BufferSize)
    {
        if (PtrInfo->PtrShapeBuffer)
        {
            delete[] PtrInfo->PtrShapeBuffer;
            PtrInfo->PtrShapeBuffer = nullptr;
        }
        PtrInfo->PtrShapeBuffer = new (std::nothrow) BYTE[FrameInfo->PointerShapeBufferSize];
        if (!PtrInfo->PtrShapeBuffer)
        {
            PtrInfo->BufferSize = 0;
            PrintError("Failed to allocate memory for pointer shape in DuplicationManager");
            return -1;
        }

        // Update buffer size
        PtrInfo->BufferSize = FrameInfo->PointerShapeBufferSize;
    }

    // Get shape
    UINT BufferSizeRequired;
    HRESULT hr = DeskDuplication->GetFramePointerShape(FrameInfo->PointerShapeBufferSize, reinterpret_cast<VOID*>(PtrInfo->PtrShapeBuffer), &BufferSizeRequired, &(PtrInfo->ShapeInfo));
    if (FAILED(hr))
    {
        delete[] PtrInfo->PtrShapeBuffer;
        PtrInfo->PtrShapeBuffer = nullptr;
        PtrInfo->BufferSize = 0;
        PrintError("Failed to get frame pointer shape in DuplicationManager");
        return hr;
    }

    return 0;
}
//
// Get next frame and write it into Data
// 获取下一帧数据
//
HRESULT DuplicationManager::GetFrame(_Out_ FRAME_DATA * Data, UINT TimeOutMs)
{
    IDXGIResource* DesktopResource = nullptr;
    DXGI_OUTDUPL_FRAME_INFO FrameInfo;

    if (!DeskDuplication)
        return DXGI_ERROR_ACCESS_LOST;
    // Get new frame
    HRESULT hr = DeskDuplication->AcquireNextFrame(TimeOutMs, &FrameInfo, &DesktopResource);
    if (FAILED(hr))
    {
        //PrintError("Failed to acquire next frame in DuplicationManager");
        return hr;
    }

    // If still holding old frame, destroy it
    if (AcquiredDesktopImage)
    {
        AcquiredDesktopImage->Release();
        AcquiredDesktopImage = nullptr;
    }

    // QI for IDXGIResource
    hr = DesktopResource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&AcquiredDesktopImage));
    DesktopResource->Release();
    DesktopResource = nullptr;
    if (FAILED(hr))
    {
        PrintError("Failed to QI for ID3D11Texture2D from acquired IDXGIResource in DuplicationManager");
        return hr;
    }

    // Get metadata
    if (FrameInfo.TotalMetadataBufferSize)
    {
        // Old buffer too small
        if (FrameInfo.TotalMetadataBufferSize > MetaDataSize)
        {
            if (MetaDataBuffer)
            {
                delete[] MetaDataBuffer;
                MetaDataBuffer = nullptr;
            }
            MetaDataBuffer = new (std::nothrow) BYTE[FrameInfo.TotalMetadataBufferSize];
            if (!MetaDataBuffer)
            {
                MetaDataSize = 0;
                Data->MoveCount = 0;
                Data->DirtyCount = 0;
                PrintError("Failed to allocate memory for metadata in DuplicationManager");
                return -1;
            }
            MetaDataSize = FrameInfo.TotalMetadataBufferSize;
        }

        UINT BufSize = FrameInfo.TotalMetadataBufferSize;

        // Get move rectangles
        hr = DeskDuplication->GetFrameMoveRects(BufSize, reinterpret_cast<DXGI_OUTDUPL_MOVE_RECT*>(MetaDataBuffer), &BufSize);
        if (FAILED(hr))
        {
            Data->MoveCount = 0;
            Data->DirtyCount = 0;
            PrintError("Failed to get frame move rects in DuplicationManager");
            return hr;
        }
        Data->MoveCount = BufSize / sizeof(DXGI_OUTDUPL_MOVE_RECT);

        BYTE* DirtyRects = MetaDataBuffer + BufSize;
        BufSize = FrameInfo.TotalMetadataBufferSize - BufSize;

        // Get dirty rectangles
        hr = DeskDuplication->GetFrameDirtyRects(BufSize, reinterpret_cast<RECT*>(DirtyRects), &BufSize);
        if (FAILED(hr))
        {
            Data->MoveCount = 0;
            Data->DirtyCount = 0;
            PrintError("Failed to get frame dirty rects in DuplicationManager");
            return hr;
        }
        Data->DirtyCount = BufSize / sizeof(RECT);

        Data->MetaData = MetaDataBuffer;
    }

    Data->Frame = AcquiredDesktopImage;
    Data->FrameInfo = FrameInfo;

    return hr;
}
//
// Release frame
// 释放一帧数据
//
void DuplicationManager::ReleaseFrame()
{
    if (AcquiredDesktopImage)
    {
        AcquiredDesktopImage->Release();
        AcquiredDesktopImage = nullptr;
    }
    if(DeskDuplication)
        DeskDuplication->ReleaseFrame();
}
//
// Gets output desc into DescPtr
// 获取输出描述信息
//
DXGI_OUTPUT_DESC* DuplicationManager::GetOutputDesc()
{
    return &OutputDesc;
}

bool DuplicationManager::PrintError(const std::string& er)
{
    Debug::PrintError("[DuplicationManager]: ", er);
    return false;
}
