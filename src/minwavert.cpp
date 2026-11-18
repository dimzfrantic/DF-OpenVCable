#include "minwavert.h"

// IUnknown standard helper boilerplate untuk PortCls
#pragma code_seg("PAGE")

STDMETHODIMP CMiniportWaveRT::QueryInterface(REFIID InterfaceId, PVOID* Interface) {
    PAGED_CODE();
    if (!Interface) return STATUS_INVALID_PARAMETER;

    if (IsEqualGUIDAligned(InterfaceId, IID_IUnknown) ||
        IsEqualGUIDAligned(InterfaceId, IID_IMiniport) ||
        IsEqualGUIDAligned(InterfaceId, IID_IMiniportWaveRT)) {
        *Interface = PVOID(PUNKNOWN(this));
        PUNKNOWN(*Interface)->AddRef();
        return STATUS_SUCCESS;
    }
    *Interface = NULL;
    return STATUS_NO_INTERFACE;
}

STDMETHODIMP_(ULONG) CMiniportWaveRT::AddRef() {
    return InterlockedIncrement((PLONG)&m_cRef);
}

STDMETHODIMP_(ULONG) CMiniportWaveRT::Release() {
    LONG count = InterlockedDecrement((PLONG)&m_cRef);
    if (count == 0) {
        delete this;
    }
    return count;
}

CMiniportWaveRT::CMiniportWaveRT(PUNKNOWN pUnknownOuter) :
    CUnknown(pUnknownOuter),
    m_CableIndex(0),
    m_pSharedBuffer(NULL),
    m_pPort(NULL)
{
    PAGED_CODE();
}

CMiniportWaveRT::~CMiniportWaveRT() {
    PAGED_CODE();
    if (m_pPort) {
        m_pPort->Release();
        m_pPort = NULL;
    }
}

NTSTATUS CMiniportWaveRT::Init(ULONG CableIndex, CCircularRingBuffer* pSharedBuffer) {
    PAGED_CODE();
    m_CableIndex = CableIndex;
    m_pSharedBuffer = pSharedBuffer;
    return STATUS_SUCCESS;
}

STDMETHODIMP_(NTSTATUS) CMiniportWaveRT::Init(PUNKNOWN UnknownAdapter, PPORTWAVERT Port) {
    PAGED_CODE();
    UNREFERENCED_PARAMETER(UnknownAdapter);
    if (!Port) return STATUS_INVALID_PARAMETER;

    m_pPort = Port;
    m_pPort->AddRef();
    return STATUS_SUCCESS;
}

STDMETHODIMP_(NTSTATUS) CMiniportWaveRT::GetDeviceDescription(PDEVICE_DESCRIPTION pDeviceDescription) {
    PAGED_CODE();
    if (!pDeviceDescription) return STATUS_INVALID_PARAMETER;
    RtlZeroMemory(pDeviceDescription, sizeof(DEVICE_DESCRIPTION));
    pDeviceDescription->Master = TRUE;
    pDeviceDescription->Dma32BitAddresses = TRUE;
    return STATUS_SUCCESS;
}

STDMETHODIMP_(NTSTATUS) CMiniportWaveRT::NewStream(
    PMINIPORTWAVERTSTREAM*  ppStream,
    PPORTWAVERTSTREAM       PortStream,
    ULONG                   Pin,
    BOOLEAN                 Capture,
    PKSDATAFORMAT           DataFormat
) {
    PAGED_CODE();
    UNREFERENCED_PARAMETER(PortStream);
    if (!ppStream || !DataFormat) return STATUS_INVALID_PARAMETER;

    CMiniportWaveRTStream* pNewStream = new (NonPagedPoolNx, MCVAD_POOL_TAG) CMiniportWaveRTStream(NULL);
    if (!pNewStream) return STATUS_INSUFFICIENT_RESOURCES;

    pNewStream->AddRef();
    NTSTATUS ntStatus = pNewStream->Init(this, Pin, Capture, DataFormat);
    if (!NT_SUCCESS(ntStatus)) {
        pNewStream->Release();
        return ntStatus;
    }

    *ppStream = pNewStream;
    return STATUS_SUCCESS;
}

//=====================================================================
// Pin & Filter Descriptors untuk WaveRT
//=====================================================================
static const KSDATARANGE_AUDIO g_PinDataRangesAudio[] = {
    {
        {
            sizeof(KSDATARANGE_AUDIO),
            0,
            0,
            0,
            STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM),
            STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)
        },
        2,      // MaximumChannels
        16,     // MinimumBitsPerSample
        32,     // MaximumBitsPerSample
        44100,  // MinimumSampleFrequency
        192000  // MaximumSampleFrequency
    }
};

static const PKSDATARANGE g_PinDataRangePointers[] = {
    (PKSDATARANGE)&g_PinDataRangesAudio[0]
};

static const PCPIN_DESCRIPTOR g_MiniportPins[] = {
    // Pin 0: WaveRT Render (Playback Sink)
    {
        0, 0, 0, NULL,
        {
            0, NULL, 0, NULL,
            SIZEOF_ARRAY(g_PinDataRangePointers), g_PinDataRangePointers,
            KSPIN_DATAFLOW_IN, KSPIN_COMMUNICATION_SINK,
            &KSCATEGORY_AUDIO, NULL, 0
        }
    },
    // Pin 1: WaveRT Capture (Recording Source)
    {
        0, 0, 0, NULL,
        {
            0, NULL, 0, NULL,
            SIZEOF_ARRAY(g_PinDataRangePointers), g_PinDataRangePointers,
            KSPIN_DATAFLOW_OUT, KSPIN_COMMUNICATION_SINK,
            &KSCATEGORY_AUDIO, NULL, 0
        }
    }
};

static const GUID g_MiniportCategories[] = {
    STATICGUIDOF(KSCATEGORY_AUDIO),
    STATICGUIDOF(KSCATEGORY_RENDER),
    STATICGUIDOF(KSCATEGORY_CAPTURE),
    STATICGUIDOF(KSCATEGORY_REALTIME)
};

static const PCFILTER_DESCRIPTOR g_MiniportFilterDescriptor = {
    0,
    NULL,
    sizeof(PCPIN_DESCRIPTOR),
    SIZEOF_ARRAY(g_MiniportPins),
    g_MiniportPins,
    sizeof(PCNODE_DESCRIPTOR),
    0,
    NULL,
    0,
    NULL,
    SIZEOF_ARRAY(g_MiniportCategories),
    g_MiniportCategories
};

STDMETHODIMP_(NTSTATUS) CMiniportWaveRT::GetDescription(PPCFILTER_DESCRIPTOR* ppFilterDescriptor) {
    PAGED_CODE();
    if (!ppFilterDescriptor) return STATUS_INVALID_PARAMETER;
    *ppFilterDescriptor = (PPCFILTER_DESCRIPTOR)&g_MiniportFilterDescriptor;
    return STATUS_SUCCESS;
}

STDMETHODIMP_(NTSTATUS) CMiniportWaveRT::DataRangeIntersection(
    ULONG           PinId,
    PKSDATARANGE    ClientDataRange,
    PKSDATARANGE    MyDataRange,
    ULONG           OutputBufferLength,
    PVOID           ResultantFormat,
    PULONG          ResultantFormatLength
) {
    PAGED_CODE();
    UNREFERENCED_PARAMETER(PinId);
    UNREFERENCED_PARAMETER(MyDataRange);

    if (!ClientDataRange || !ResultantFormatLength) {
        return STATUS_INVALID_PARAMETER;
    }

    ULONG requiredSize = sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE);
    *ResultantFormatLength = requiredSize;

    if (OutputBufferLength == 0) {
        return STATUS_BUFFER_OVERFLOW;
    }
    if (OutputBufferLength < requiredSize) {
        return STATUS_BUFFER_TOO_SMALL;
    }
    if (!ResultantFormat) {
        return STATUS_INVALID_PARAMETER;
    }

    PKSDATAFORMAT_WAVEFORMATEXTENSIBLE pFormat = (PKSDATAFORMAT_WAVEFORMATEXTENSIBLE)ResultantFormat;
    RtlZeroMemory(pFormat, sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE));

    pFormat->DataFormat.FormatSize = sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE);
    pFormat->DataFormat.MajorFormat = KSDATAFORMAT_TYPE_AUDIO;
    pFormat->DataFormat.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
    pFormat->DataFormat.Specifier = KSDATAFORMAT_SPECIFIER_WAVEFORMATEX;

    WAVEFORMATEXTENSIBLE* pWaveFormat = &pFormat->WaveFormatExt;
    pWaveFormat->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    pWaveFormat->Format.nChannels = DEFAULT_CHANNELS;
    pWaveFormat->Format.nSamplesPerSec = DEFAULT_SAMPLE_RATE;
    pWaveFormat->Format.wBitsPerSample = DEFAULT_BITS_PER_SAMPLE;
    pWaveFormat->Format.nBlockAlign = (pWaveFormat->Format.nChannels * DEFAULT_CONTAINER_SIZE) / 8;
    pWaveFormat->Format.nAvgBytesPerSec = pWaveFormat->Format.nSamplesPerSec * pWaveFormat->Format.nBlockAlign;
    pWaveFormat->Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    pWaveFormat->Samples.wValidBitsPerSample = DEFAULT_BITS_PER_SAMPLE;
    pWaveFormat->dwChannelMask = KSAUDIO_SPEAKER_STEREO;
    pWaveFormat->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

    return STATUS_SUCCESS;
}

//=====================================================================
// Stream Implementation
//=====================================================================
CMiniportWaveRTStream::CMiniportWaveRTStream(PUNKNOWN pUnknownOuter) :
    CUnknown(pUnknownOuter),
    m_pMiniport(NULL),
    m_PinId(0),
    m_bCapture(FALSE),
    m_State(KSSTATE_STOP),
    m_pFormat(NULL),
    m_pDmaMdl(NULL),
    m_pDmaBuffer(NULL),
    m_DmaBufferSize(0),
    m_LinearPosition(0)
{
    m_StartTime.QuadPart = 0;
    m_Frequency = KeQueryPerformanceFrequency(NULL);
}

CMiniportWaveRTStream::~CMiniportWaveRTStream() {
    if (m_pDmaBuffer && m_pDmaMdl) {
        MmUnlockPages(m_pDmaMdl);
        IoFreeMdl(m_pDmaMdl);
        ExFreePoolWithTag(m_pDmaBuffer, MCVAD_POOL_TAG);
        m_pDmaBuffer = NULL;
        m_pDmaMdl = NULL;
    }
    if (m_pFormat) {
        ExFreePoolWithTag(m_pFormat, MCVAD_POOL_TAG);
        m_pFormat = NULL;
    }
}

NTSTATUS CMiniportWaveRTStream::Init(
    CMiniportWaveRT*    pMiniport,
    ULONG               Pin,
    BOOLEAN             Capture,
    PKSDATAFORMAT       DataFormat
) {
    m_pMiniport = pMiniport;
    m_PinId = Pin;
    m_bCapture = Capture;

    ULONG formatSize = sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE);
    m_pFormat = (PKSDATAFORMAT_WAVEFORMATEXTENSIBLE)ExAllocatePool2(POOL_FLAG_NON_PAGED, formatSize, MCVAD_POOL_TAG);
    if (!m_pFormat) return STATUS_INSUFFICIENT_RESOURCES;

    RtlCopyMemory(m_pFormat, DataFormat, sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE));
    return STATUS_SUCCESS;
}

STDMETHODIMP_(NTSTATUS) CMiniportWaveRTStream::SetState(KSSTATE State) {
    m_State = State;
    if (State == KSSTATE_RUN) {
        KeQueryPerformanceCounter(&m_StartTime);
        m_LinearPosition = 0;
    } else if (State == KSSTATE_STOP) {
        m_LinearPosition = 0;
    }
    return STATUS_SUCCESS;
}

STDMETHODIMP_(NTSTATUS) CMiniportWaveRTStream::GetPosition(KSRTAUDIO_GETPOSITION_INFO* pPositionInfo) {
    if (!pPositionInfo || m_DmaBufferSize == 0) return STATUS_INVALID_PARAMETER;

    LARGE_INTEGER now, elapsed;
    now = KeQueryPerformanceCounter(NULL);
    elapsed.QuadPart = now.QuadPart - m_StartTime.QuadPart;

    // Hitung bytes per detik secara akurat berdasar format stream aktual
    ULONG bytesPerSec = 0;
    if (m_pFormat && m_pFormat->WaveFormatExt.Format.nAvgBytesPerSec > 0) {
        bytesPerSec = m_pFormat->WaveFormatExt.Format.nAvgBytesPerSec;
    } else {
        bytesPerSec = DEFAULT_SAMPLE_RATE * DEFAULT_CHANNELS * (DEFAULT_CONTAINER_SIZE / 8);
    }

    ULONGLONG totalBytesStreamed = (elapsed.QuadPart * bytesPerSec) / m_Frequency.QuadPart;

    pPositionInfo->ActualPosition = (ULONG)(totalBytesStreamed % m_DmaBufferSize);
    pPositionInfo->LinearPosition = totalBytesStreamed;
    pPositionInfo->PerformanceCounterPosition = now.QuadPart;

    // Loopback transfer berdasar delta bytes aktual sejak pembacaan sebelumnya
    // Mencegah glitch, duplikasi data, dan clock drift
    CCircularRingBuffer* pRing = m_pMiniport->GetSharedBuffer();
    if (pRing && m_State == KSSTATE_RUN && m_pDmaBuffer && totalBytesStreamed > m_LinearPosition) {
        ULONGLONG delta = totalBytesStreamed - m_LinearPosition;
        if (delta > m_DmaBufferSize) {
            delta = m_DmaBufferSize;
        }

        ULONG prevPos = (ULONG)(m_LinearPosition % m_DmaBufferSize);
        ULONG toTransfer = (ULONG)delta;

        // Tangani pembagian chunk jika posisi melewati batas circular buffer DMA
        if (prevPos + toTransfer <= m_DmaBufferSize) {
            if (!m_bCapture) {
                pRing->Write((PUCHAR)m_pDmaBuffer + prevPos, toTransfer);
            } else {
                pRing->Read((PUCHAR)m_pDmaBuffer + prevPos, toTransfer);
            }
        } else {
            ULONG firstChunk = m_DmaBufferSize - prevPos;
            ULONG secondChunk = toTransfer - firstChunk;
            if (!m_bCapture) {
                pRing->Write((PUCHAR)m_pDmaBuffer + prevPos, firstChunk);
                pRing->Write((PUCHAR)m_pDmaBuffer, secondChunk);
            } else {
                pRing->Read((PUCHAR)m_pDmaBuffer + prevPos, firstChunk);
                pRing->Read((PUCHAR)m_pDmaBuffer, secondChunk);
            }
        }
        m_LinearPosition = totalBytesStreamed;
    }

    return STATUS_SUCCESS;
}

STDMETHODIMP_(NTSTATUS) CMiniportWaveRTStream::AllocateAudioBuffer(
    ULONG RequestedSize,
    PMDL* ppMdl,
    ULONG* pActualSize,
    ULONG* pOffsetFromArm,
    MEMORY_CACHING_TYPE* pCacheType
) {
    if (!ppMdl || !pActualSize || !pOffsetFromArm || !pCacheType) return STATUS_INVALID_PARAMETER;

    m_DmaBufferSize = (RequestedSize + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    m_pDmaBuffer = ExAllocatePool2(POOL_FLAG_NON_PAGED, m_DmaBufferSize, MCVAD_POOL_TAG);
    if (!m_pDmaBuffer) return STATUS_INSUFFICIENT_RESOURCES;

    m_pDmaMdl = IoAllocateMdl(m_pDmaBuffer, m_DmaBufferSize, FALSE, FALSE, NULL);
    if (!m_pDmaMdl) {
        ExFreePoolWithTag(m_pDmaBuffer, MCVAD_POOL_TAG);
        m_pDmaBuffer = NULL;
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    MmBuildMdlForNonPagedPool(m_pDmaMdl);

    *ppMdl = m_pDmaMdl;
    *pActualSize = m_DmaBufferSize;
    *pOffsetFromArm = 0;
    *pCacheType = MmCached;

    return STATUS_SUCCESS;
}

STDMETHODIMP_(VOID) CMiniportWaveRTStream::FreeAudioBuffer(PMDL pMdl, ULONG Size) {
    UNREFERENCED_PARAMETER(Size);
    if (pMdl == m_pDmaMdl) {
        if (m_pDmaBuffer) {
            IoFreeMdl(m_pDmaMdl);
            ExFreePoolWithTag(m_pDmaBuffer, MCVAD_POOL_TAG);
            m_pDmaBuffer = NULL;
            m_pDmaMdl = NULL;
        }
    }
}

STDMETHODIMP_(VOID) CMiniportWaveRTStream::GetClockRegister(KSRTAUDIO_HWREGISTER* pRegister) {
    if (pRegister) RtlZeroMemory(pRegister, sizeof(KSRTAUDIO_HWREGISTER));
}

STDMETHODIMP_(VOID) CMiniportWaveRTStream::GetPositionRegister(KSRTAUDIO_HWREGISTER* pRegister) {
    if (pRegister) RtlZeroMemory(pRegister, sizeof(KSRTAUDIO_HWREGISTER));
}

STDMETHODIMP_(VOID) CMiniportWaveRTStream::GetHWLatency(KSRTAUDIO_HWLATENCY* pLatency) {
    if (pLatency) {
        pLatency->FifoSize = 0;
        pLatency->ChipsetDelay = 0;
        pLatency->CodecDelay = 0;
    }
}

STDMETHODIMP_(NTSTATUS) CMiniportWaveRTStream::SetWritePacket(ULONG PacketNumber, DWORD Flags, ULONG EosPacketLength) {
    UNREFERENCED_PARAMETER(PacketNumber);
    UNREFERENCED_PARAMETER(Flags);
    UNREFERENCED_PARAMETER(EosPacketLength);
    return STATUS_SUCCESS;
}

STDMETHODIMP_(NTSTATUS) CMiniportWaveRTStream::GetReadPacket(ULONG* pPacketNumber, DWORD* pFlags, ULONG64* pPerformanceCounterValue, BOOL* pMoreData) {
    if (pPacketNumber) *pPacketNumber = 0;
    if (pFlags) *pFlags = 0;
    if (pPerformanceCounterValue) *pPerformanceCounterValue = KeQueryPerformanceCounter(NULL).QuadPart;
    if (pMoreData) *pMoreData = FALSE;
    return STATUS_SUCCESS;
}

// IUnknown implementation untuk stream
STDMETHODIMP CMiniportWaveRTStream::QueryInterface(REFIID InterfaceId, PVOID* Interface) {
    if (!Interface) return STATUS_INVALID_PARAMETER;
    if (IsEqualGUIDAligned(InterfaceId, IID_IUnknown) ||
        IsEqualGUIDAligned(InterfaceId, IID_IMiniportWaveRTStream)) {
        *Interface = PVOID(PUNKNOWN(this));
        PUNKNOWN(*Interface)->AddRef();
        return STATUS_SUCCESS;
    }
    *Interface = NULL;
    return STATUS_NO_INTERFACE;
}

STDMETHODIMP_(ULONG) CMiniportWaveRTStream::AddRef() {
    return InterlockedIncrement((PLONG)&m_cRef);
}

STDMETHODIMP_(ULONG) CMiniportWaveRTStream::Release() {
    LONG count = InterlockedDecrement((PLONG)&m_cRef);
    if (count == 0) {
        delete this;
    }
    return count;
}
