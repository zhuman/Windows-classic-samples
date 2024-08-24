// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#include "DuplicationManager.h"

//
// Constructor sets up references / variables
//
DuplicationManager::DuplicationManager()
{
}

//
// Destructor simply calls CleanRefs to destroy everything
//
DuplicationManager::~DuplicationManager()
{
}

//
// Initialize duplication interfaces
//
DUPL_RETURN DuplicationManager::InitDupl(_In_ ID3D11Device* Device, UINT Output)
{
    m_OutputNumber = Output;

    // Take a reference on the device
    m_Device.copy_from(Device);

    // Get DXGI device
    winrt::com_ptr<IDXGIDevice> DxgiDevice;
    HRESULT hr = m_Device->QueryInterface(__uuidof(IDXGIDevice), DxgiDevice.put_void());
    if (FAILED(hr))
    {
        return ProcessFailure(nullptr, L"Failed to QI for DXGI Device", L"Error", hr);
    }

    // Get DXGI adapter
    winrt::com_ptr<IDXGIAdapter> DxgiAdapter;
    hr = DxgiDevice->GetParent(__uuidof(IDXGIAdapter), DxgiAdapter.put_void());
    if (FAILED(hr))
    {
        return ProcessFailure(m_Device.get(), L"Failed to get parent DXGI Adapter", L"Error", hr, SystemTransitionsExpectedErrors);
    }

    // Get output
    winrt::com_ptr<IDXGIOutput> DxgiOutput;
    hr = DxgiAdapter->EnumOutputs(Output, DxgiOutput.put());
    if (FAILED(hr))
    {
        return ProcessFailure(m_Device.get(), L"Failed to get specified output in DUPLICATIONMANAGER", L"Error", hr, EnumOutputsExpectedErrors);
    }

    DxgiOutput->GetDesc(&m_OutputDesc);

    // QI for Output 1
    winrt::com_ptr<IDXGIOutput1> DxgiOutput1 = DxgiOutput.try_as<IDXGIOutput1>();
    if (!DxgiOutput1)
    {
        return ProcessFailure(nullptr, L"Failed to QI for DxgiOutput1 in DUPLICATIONMANAGER", L"Error", E_NOTIMPL);
    }

    // Create desktop duplication
    hr = DxgiOutput1->DuplicateOutput(m_Device.get(), m_DeskDupl.put());
    if (FAILED(hr))
    {
        if (hr == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE)
        {
            MessageBoxW(nullptr, L"There is already the maximum number of applications using the Desktop Duplication API running, please close one of those applications and then try again.", L"Error", MB_OK);
            return DUPL_RETURN_ERROR_UNEXPECTED;
        }
        return ProcessFailure(m_Device.get(), L"Failed to get duplicate output in DUPLICATIONMANAGER", L"Error", hr, CreateDuplicationExpectedErrors);
    }

    return DUPL_RETURN_SUCCESS;
}

//
// Retrieves mouse info and write it into PtrInfo
//
DUPL_RETURN DuplicationManager::GetMouse(_Inout_ PointerInfo* PtrInfo, _In_ DXGI_OUTDUPL_FRAME_INFO* FrameInfo, INT OffsetX, INT OffsetY)
{
    // A non-zero mouse update timestamp indicates that there is a mouse position update and optionally a shape change
    if (FrameInfo->LastMouseUpdateTime.QuadPart == 0)
    {
        return DUPL_RETURN_SUCCESS;
    }

    bool UpdatePosition = true;

    // Make sure we don't update pointer position wrongly
    // If pointer is invisible, make sure we did not get an update from another output that the last time that said pointer
    // was visible, if so, don't set it to invisible or update.
    if (!FrameInfo->PointerPosition.Visible && (PtrInfo->WhoUpdatedPositionLast != m_OutputNumber))
    {
        UpdatePosition = false;
    }

    // If two outputs both say they have a visible, only update if new update has newer timestamp
    if (FrameInfo->PointerPosition.Visible && PtrInfo->Visible && (PtrInfo->WhoUpdatedPositionLast != m_OutputNumber) && (PtrInfo->LastTimeStamp.QuadPart > FrameInfo->LastMouseUpdateTime.QuadPart))
    {
        UpdatePosition = false;
    }

    // Update position
    if (UpdatePosition)
    {
        PtrInfo->Position.x = FrameInfo->PointerPosition.Position.x + m_OutputDesc.DesktopCoordinates.left - OffsetX;
        PtrInfo->Position.y = FrameInfo->PointerPosition.Position.y + m_OutputDesc.DesktopCoordinates.top - OffsetY;
        PtrInfo->WhoUpdatedPositionLast = m_OutputNumber;
        PtrInfo->LastTimeStamp = FrameInfo->LastMouseUpdateTime;
        PtrInfo->Visible = FrameInfo->PointerPosition.Visible != 0;
    }

    // No new shape
    if (FrameInfo->PointerShapeBufferSize == 0)
    {
        return DUPL_RETURN_SUCCESS;
    }

    // Old buffer too small
    if (FrameInfo->PointerShapeBufferSize > PtrInfo->PtrShapeBuffer.size())
    {
        try
        {
            PtrInfo->PtrShapeBuffer.resize(FrameInfo->PointerShapeBufferSize);
        }
        catch (std::bad_alloc)
        {
            return ProcessFailure(nullptr, L"Failed to allocate memory for pointer shape in DUPLICATIONMANAGER", L"Error", E_OUTOFMEMORY);
        }
    }

    // Get shape
    UINT BufferSizeRequired;
    HRESULT hr = m_DeskDupl->GetFramePointerShape(FrameInfo->PointerShapeBufferSize, reinterpret_cast<VOID*>(PtrInfo->PtrShapeBuffer.data()), &BufferSizeRequired, &(PtrInfo->ShapeInfo));
    if (FAILED(hr))
    {
        PtrInfo->PtrShapeBuffer.clear();
        return ProcessFailure(m_Device.get(), L"Failed to get frame pointer shape in DUPLICATIONMANAGER", L"Error", hr, FrameInfoExpectedErrors);
    }

    return DUPL_RETURN_SUCCESS;
}


//
// Get next frame and write it into Data
//
_Success_(*Timeout == false && return == DUPL_RETURN_SUCCESS)
DUPL_RETURN DuplicationManager::GetFrame(_Out_ FrameData* Data, _Out_ bool* Timeout)
{
    winrt::com_ptr<IDXGIResource> DesktopResource;
    DXGI_OUTDUPL_FRAME_INFO FrameInfo = {};

    // Get new frame
    HRESULT hr = m_DeskDupl->AcquireNextFrame(500, &FrameInfo, DesktopResource.put());
    if (hr == DXGI_ERROR_WAIT_TIMEOUT)
    {
        *Timeout = true;
        return DUPL_RETURN_SUCCESS;
    }
    *Timeout = false;

    if (FAILED(hr))
    {
        return ProcessFailure(m_Device.get(), L"Failed to acquire next frame in DUPLICATIONMANAGER", L"Error", hr, FrameInfoExpectedErrors);
    }

    // If still holding old frame, destroy it
    m_AcquiredDesktopImage = DesktopResource.try_as<ID3D11Texture2D>();
    DesktopResource = nullptr;
    if (!m_AcquiredDesktopImage)
    {
        return ProcessFailure(nullptr, L"Failed to QI for ID3D11Texture2D from acquired IDXGIResource in DUPLICATIONMANAGER", L"Error", E_NOTIMPL);
    }

    // Get metadata
    if (FrameInfo.TotalMetadataBufferSize)
    {
        // Old buffer too small
        if (FrameInfo.TotalMetadataBufferSize > m_MetaDataBuffer.size())
        {
            try
            {
                m_MetaDataBuffer.resize(FrameInfo.TotalMetadataBufferSize);
            }
            catch (std::bad_alloc)
            {
                Data->MoveCount = 0;
                Data->DirtyCount = 0;
                return ProcessFailure(nullptr, L"Failed to allocate memory for metadata in DUPLICATIONMANAGER", L"Error", E_OUTOFMEMORY);
            }
        }

        UINT BufSize = FrameInfo.TotalMetadataBufferSize;

        // Get move rectangles
        hr = m_DeskDupl->GetFrameMoveRects(BufSize, reinterpret_cast<DXGI_OUTDUPL_MOVE_RECT*>(m_MetaDataBuffer.data()), &BufSize);
        if (FAILED(hr))
        {
            Data->MoveCount = 0;
            Data->DirtyCount = 0;
            return ProcessFailure(nullptr, L"Failed to get frame move rects in DUPLICATIONMANAGER", L"Error", hr, FrameInfoExpectedErrors);
        }
        Data->MoveCount = BufSize / sizeof(DXGI_OUTDUPL_MOVE_RECT);

        BYTE* DirtyRects = m_MetaDataBuffer.data() + BufSize;
        BufSize = FrameInfo.TotalMetadataBufferSize - BufSize;

        // Get dirty rectangles
        hr = m_DeskDupl->GetFrameDirtyRects(BufSize, reinterpret_cast<RECT*>(DirtyRects), &BufSize);
        if (FAILED(hr))
        {
            Data->MoveCount = 0;
            Data->DirtyCount = 0;
            return ProcessFailure(nullptr, L"Failed to get frame dirty rects in DUPLICATIONMANAGER", L"Error", hr, FrameInfoExpectedErrors);
        }
        Data->DirtyCount = BufSize / sizeof(RECT);

        Data->MetaData = m_MetaDataBuffer.data();
    }

    Data->Frame = m_AcquiredDesktopImage.get();
    Data->FrameInfo = FrameInfo;

    return DUPL_RETURN_SUCCESS;
}

//
// Release frame
//
DUPL_RETURN DuplicationManager::DoneWithFrame()
{
    HRESULT hr = m_DeskDupl->ReleaseFrame();
    if (FAILED(hr))
    {
        return ProcessFailure(m_Device.get(), L"Failed to release frame in DUPLICATIONMANAGER", L"Error", hr, FrameInfoExpectedErrors);
    }

    m_AcquiredDesktopImage = nullptr;

    return DUPL_RETURN_SUCCESS;
}

//
// Gets output desc into DescPtr
//
void DuplicationManager::GetOutputDesc(_Out_ DXGI_OUTPUT_DESC* DescPtr)
{
    *DescPtr = m_OutputDesc;
}
