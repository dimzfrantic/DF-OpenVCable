#pragma once

#include "driver.h"
#include "ringbuffer.h"

// Forward declaration
class CMiniportWaveRT;

// Pin Enums untuk Topologi WaveRT (Host Streaming & Physical Bridge Pins)
enum {
    PIN_WAVE_RENDER_SINK = 0,       // WaveRT Playback Host Stream (Host In)
    PIN_WAVE_RENDER_SOURCE = 1,     // WaveRT Render Source (Physical Connection to Topology)
    PIN_WAVE_CAPTURE_SINK = 2,      // WaveRT Capture Sink (Physical Connection from Topology)
    PIN_WAVE_CAPTURE_SOURCE = 3     // WaveRT Capture Host Stream (Host Out)
};

//=====================================================================
// CMiniportWaveRTStream: Implementasi stream audio WaveRT
//=====================================================================
class CMiniportWaveRTStream :
    public IMiniportWaveRTStream,
    public CUnknown
{
private:
    CMiniportWaveRT*        m_pMiniport;
    ULONG                   m_PinId;            // PIN_WAVE_RENDER_SINK / PIN_WAVE_CAPTURE_SINK
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
    BOOLEAN*                m_pbEnabled;
    PPORTWAVERT             m_pPort;

public:
    DECLARE_STD_UNKNOWN();

    CMiniportWaveRT(PUNKNOWN pUnknownOuter);
    ~CMiniportWaveRT();

    NTSTATUS Init(ULONG CableIndex, CCircularRingBuffer* pSharedBuffer, BOOLEAN* pbEnabled = NULL);

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
        PKSDATARANGE            DataFormat
    );
    STDMETHODIMP_(NTSTATUS) GetDeviceDescription(PDEVICE_DESCRIPTION pDeviceDescription);

    CCircularRingBuffer*    GetSharedBuffer() const { return m_pSharedBuffer; }
    ULONG                   GetCableIndex() const { return m_CableIndex; }
    BOOLEAN                 IsCableEnabled() const { return m_pbEnabled ? *m_pbEnabled : TRUE; }
};
