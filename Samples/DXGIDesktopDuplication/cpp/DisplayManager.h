// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#pragma once

#include "CommonTypes.h"

//
// Handles the task of processing frames
//
class DisplayManager
{
public:
	DisplayManager();
	~DisplayManager();
	void InitD3D(DxResources* Data);
	ID3D11Device* GetDevice();
	DUPL_RETURN ProcessFrame(_In_ FrameData* Data, _Inout_ ID3D11Texture2D* SharedSurf, INT OffsetX, INT OffsetY, _In_ DXGI_OUTPUT_DESC* DeskDesc);
	void CleanRefs();

private:
	// methods
	DUPL_RETURN CopyDirty(_In_ ID3D11Texture2D* SrcSurface, _Inout_ ID3D11Texture2D* SharedSurf, _In_reads_(DirtyCount) RECT* DirtyBuffer, UINT DirtyCount, INT OffsetX, INT OffsetY, _In_ DXGI_OUTPUT_DESC* DeskDesc);
	DUPL_RETURN CopyMove(_Inout_ ID3D11Texture2D* SharedSurf, _In_reads_(MoveCount) DXGI_OUTDUPL_MOVE_RECT* MoveBuffer, UINT MoveCount, INT OffsetX, INT OffsetY, _In_ DXGI_OUTPUT_DESC* DeskDesc, INT TexWidth, INT TexHeight);
	void SetDirtyVert(_Out_writes_(NUMVERTICES) Vertex* Vertices, _In_ RECT* Dirty, INT OffsetX, INT OffsetY, _In_ DXGI_OUTPUT_DESC* DeskDesc, _In_ D3D11_TEXTURE2D_DESC* FullDesc, _In_ D3D11_TEXTURE2D_DESC* ThisDesc);
	void SetMoveRect(_Out_ RECT* SrcRect, _Out_ RECT* DestRect, _In_ DXGI_OUTPUT_DESC* DeskDesc, _In_ DXGI_OUTDUPL_MOVE_RECT* MoveRect, INT TexWidth, INT TexHeight);

	// variables
	winrt::com_ptr<ID3D11Device> m_Device;
	winrt::com_ptr<ID3D11DeviceContext> m_DeviceContext;
	winrt::com_ptr<ID3D11Texture2D> m_MoveSurf;
	winrt::com_ptr<ID3D11VertexShader> m_VertexShader;
	winrt::com_ptr<ID3D11PixelShader> m_PixelShader;
	winrt::com_ptr<ID3D11InputLayout> m_InputLayout;
	winrt::com_ptr<ID3D11RenderTargetView> m_RTV;
	winrt::com_ptr<ID3D11SamplerState> m_SamplerLinear;
	std::vector<BYTE> m_DirtyVertexBuffer;
};
