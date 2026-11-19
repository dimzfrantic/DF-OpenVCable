#pragma once

#include "driver.h"
#include "ringbuffer.h"

// Forward declaration
class CMiniportWaveRT;

//=====================================================================
// CMiniportWaveRTStream: Implementasi stream audio WaveRT
//=====================================================================
class CMiniportWaveRTStream :
    public IMiniportWaveRTStream,
    public CUnknown
{
private:
    CMiniportWaveRT*        m_pMiniport;
    ULONG                   m_PinId;            // 0 = Render (Playback), 1 = Capture (Recording)
    BOOLEAN                 m_bCapture;
    KSSTATE                 m_State;
    PKSDATAFORMAT_WAVEFORMATEXTENSIBLE m_pFormat;

    // Buffer DMA WaveRT virtual yang di-map ke User Mode Audio Engine
    PMDL                    m_pDmaMdl;
    PVOID                   m_pDmaBuffer;
    ULONG                   m_DmaBufferSize;

    // Posisi dan Clock stream simulasi
    LARGE_INTEGER           m_StartTime;
    LARGE_INTEGER           m_Frequency;
    ULONGLONG               m_LinearPosition;

public:
    DECLARE_STD_UNKNOWN();

    CMiniportWaveRTStream(PUNKNOWN pUnknownOuter);
    ~CMiniportWaveRTStream();

    NTSTATUS Init(
        CMiniportWaveRT*                    pMiniport,
        ULONG                               Pin,
        BOOLEAN                             Capture,
        PKSDATAFORMAT                       DataFormat
    );

    // IMiniportWaveRTStream methods
    STDMETHODIMP_(NTSTATUS) SetState(KSSTATE State);
    STDMETHODIMP_(NTSTATUS) SetFormat(PKSDATAFORMAT DataFormat);
    STDMETHODIMP_(NTSTATUS) GetPosition(KSAUDIO_POSITION* pPosition);
    STDMETHODIMP_(NTSTATUS) AllocateAudioBuffer(ULONG RequestedSize, PMDL* ppMdl, ULONG* pActualSize, ULONG* pOffsetFromArm, MEMORY_CACHING_TYPE* pCacheType);
    STDMETHODIMP_(VOID)     FreeAudioBuffer(PMDL pMdl, ULONG Size);
    STDMETHODIMP_(NTSTATUS) GetClockRegister(KSRTAUDIO_HWREGISTER* pRegister);
    STDMETHODIMP_(NTSTATUS) GetPositionRegister(KSRTAUDIO_HWREGISTER* pRegister);
    STDMETHODIMP_(VOID)     GetHWLatency(KSRTAUDIO_HWLATENCY* pLatency);
};

//=====================================================================
// CMiniportWaveRT: Implementasi miniport adapter audio WaveRT
//=====================================================================
class CMiniportWaveRT :
    public IMiniportWaveRT,
    public CUnknown
{
private:
    ULONG                   m_CableIndex;
    CCircularRingBuffer*    m_pSharedBuffer;
    PPORTWAVERT             m_pPort;

public:
    DECLARE_STD_UNKNOWN();

    CMiniportWaveRT(PUNKNOWN pUnknownOuter);
    ~CMiniportWaveRT();

    NTSTATUS Init(ULONG CableIndex, CCircularRingBuffer* pSharedBuffer);

    // IMiniport methods
    STDMETHODIMP_(NTSTATUS) GetDescription(PPCFILTER_DESCRIPTOR* ppFilterDescriptor);
    STDMETHODIMP_(NTSTATUS) DataRangeIntersection(
        ULONG           PinId,
        PKSDATARANGE    ClientDataRange,
        PKSDATARANGE    MyDataRange,
        ULONG           OutputBufferLength,
        PVOID           ResultantFormat,
        PULONG          ResultantFormatLength
    );

    // IMiniportWaveRT methods
    STDMETHODIMP_(NTSTATUS) Init(PUNKNOWN UnknownAdapter, PRESOURCELIST ResourceList, PPORTWAVERT Port);
    STDMETHODIMP_(NTSTATUS) NewStream(
        PMINIPORTWAVERTSTREAM*  ppStream,
        PPORTWAVERTSTREAM       PortStream,
        ULONG                   Pin,
        BOOLEAN                 Capture,
        PKSDATAFORMAT           DataFormat
    );
    STDMETHODIMP_(NTSTATUS) GetDeviceDescription(PDEVICE_DESCRIPTION pDeviceDescription);

    CCircularRingBuffer*    GetSharedBuffer() const { return m_pSharedBuffer; }
    ULONG                   GetCableIndex() const { return m_CableIndex; }
};
