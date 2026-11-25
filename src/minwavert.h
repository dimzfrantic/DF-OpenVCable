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

    // Parameter format aktual (dipakai untuk preroll & deteksi backlog basi)
    ULONG                   m_BytesPerSec;
    ULONG                   m_BlockAlign;

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

    // Jumlah stream capture yang sedang berjalan pada kabel ini. Sisi render
    // memakai ini supaya tidak menumpuk audio ke ring saat tidak ada konsumen
    // (kalau tetap menulis, ring penuh dan counter overrun naik tanpa arti).
    volatile LONG           m_CaptureStreamCount;
    volatile LONG           m_RenderStreamCount;

    // Format stream yang benar-benar dinegosiasikan tiap sisi. Dipakai untuk
    // diagnosa: kalau RenderBytesPerSec != CaptureBytesPerSec, loopback byte-based
    // pasti drift dan suara rusak.
    volatile LONG           m_RenderBlockAlign;
    volatile LONG           m_RenderBytesPerSec;
    volatile LONG           m_CaptureBlockAlign;
    volatile LONG           m_CaptureBytesPerSec;

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

    VOID                    CaptureStreamStarted() { InterlockedIncrement(&m_CaptureStreamCount); }
    VOID                    CaptureStreamStopped() { InterlockedDecrement(&m_CaptureStreamCount); }
    BOOLEAN                 IsCaptureActive() const { return m_CaptureStreamCount > 0; }

    VOID                    RenderStreamStarted() { InterlockedIncrement(&m_RenderStreamCount); }
    VOID                    RenderStreamStopped() { InterlockedDecrement(&m_RenderStreamCount); }
    BOOLEAN                 IsRenderActive() const { return m_RenderStreamCount > 0; }

    VOID                    SetStreamFormat(BOOLEAN bCapture, ULONG blockAlign, ULONG bytesPerSec) {
        if (bCapture) {
            m_CaptureBlockAlign = (LONG)blockAlign;
            m_CaptureBytesPerSec = (LONG)bytesPerSec;
        } else {
            m_RenderBlockAlign = (LONG)blockAlign;
            m_RenderBytesPerSec = (LONG)bytesPerSec;
        }
    }
    ULONG                   GetRenderBlockAlign() const { return (ULONG)m_RenderBlockAlign; }
    ULONG                   GetRenderBytesPerSec() const { return (ULONG)m_RenderBytesPerSec; }
    ULONG                   GetCaptureBlockAlign() const { return (ULONG)m_CaptureBlockAlign; }
    ULONG                   GetCaptureBytesPerSec() const { return (ULONG)m_CaptureBytesPerSec; }
};
