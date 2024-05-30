// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#ifndef _DUPLICATIONMANAGER_H_
#define _DUPLICATIONMANAGER_H_

#include "CommonTypes.h"
#include "Debug.h"
//
// Handles the task of duplicating an output.
//
class DuplicationManager
{
public:
    DuplicationManager();
    ~DuplicationManager();
    bool InitDuplication(_In_ ID3D11Device* Dev, UINT DispIndex);
    void Destroy();
    HRESULT GetFrame(_Out_ FRAME_DATA* Data, UINT TimeOutMs = 500);
    void ReleaseFrame();
    bool ResetDuplication();
    HRESULT GetMouse(_Inout_ PTR_INFO* PtrInfo, _In_ DXGI_OUTDUPL_FRAME_INFO* FrameInfo, INT OffsetX, INT OffsetY);
    DXGI_OUTPUT_DESC* GetOutputDesc();

private:
    bool PrintError(const std::string &er);
    bool CreateDuplication();

private:
    // vars
    IDXGIOutputDuplication* DeskDuplication;
    ID3D11Texture2D* AcquiredDesktopImage;
    _Field_size_bytes_(MetaDataSize) BYTE* MetaDataBuffer;
    UINT MetaDataSize;
    UINT DisplayIndex;
    ID3D11Device* Device;
    IDXGIOutput1* DxgiOutput1;
    DXGI_OUTPUT_DESC OutputDesc;
};

#endif
