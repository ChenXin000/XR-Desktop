// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#ifndef _COMMONTYPES_H_
#define _COMMONTYPES_H_

#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <sal.h>
#include <new>
#include <warning.h>
#include <DirectXMath.h>

//#include "PixelShader.h"
//#include "VertexShader.h"

#define NUMVERTICES 6
#define BPP         4

#define OCCLUSION_STATUS_MSG WM_USER

//
// Holds info about the pointer/cursor
//
typedef struct _PTR_INFO
{
    _Field_size_bytes_(BufferSize) BYTE* PtrShapeBuffer;
    DXGI_OUTDUPL_POINTER_SHAPE_INFO ShapeInfo;
    POINT Position;
    bool Visible;
    UINT BufferSize;
    UINT WhoUpdatedPositionLast;
    LARGE_INTEGER LastTimeStamp;
} PTR_INFO;

//
// FRAME_DATA holds information about an acquired frame
//
typedef struct _FRAME_DATA
{
    ID3D11Texture2D* Frame;
    _Field_size_bytes_((MoveCount * sizeof(DXGI_OUTDUPL_MOVE_RECT)) + (DirtyCount * sizeof(RECT))) BYTE* MetaData;
    DXGI_OUTDUPL_FRAME_INFO FrameInfo;
    UINT DirtyCount;
    UINT MoveCount;
} FRAME_DATA;

//
// A vertex with a position and texture coordinate
//
typedef struct _VERTEX
{
    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT2 TexCoord;
} VERTEX;

#endif
