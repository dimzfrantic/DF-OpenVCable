#include "minwavert.h"

// IUnknown standard helper boilerplate untuk PortCls
#pragma code_seg("PAGE")

STDMETHODIMP_(NTSTATUS) CMiniportWaveRT::NonDelegatingQueryInterface(REFIID InterfaceId, PVOID* Interface) {
    PAGED_CODE();
    if (!Interface) return STATUS_INVALID_PARAMETER;

    if (IsEqualGUIDAligned(InterfaceId, IID_IUnknown)) {
        *Interface = PVOID(PUNKNOWN(PMINIPORTWAVERT(this)));
    } else if (IsEqualGUIDAligned(InterfaceId, IID_IMiniport)) {
        *Interface = PVOID(PMINIPORT(this));
    } else if (IsEqualGUIDAligned(InterfaceId, IID_IMiniportWaveRT)) {
        *Interface = PVOID(PMINIPORTWAVERT(this));
    } else {
        *Interface = NULL;
        return STATUS_INVALID_PARAMETER;
    }

    PUNKNOWN(*Interface)->AddRef();
    return STATUS_SUCCESS;
}

CMiniportWaveRT::CMiniportWaveRT(PUNKNOWN pUnknownOuter) :
    CUnknown(pUnknownOuter),
    m_CableIndex(0),
    m_pSharedBuffer(NULL),
    m_pbEnabled(NULL),
    m_pPort(NULL),
    m_CaptureStreamCount(0),
    m_RenderStreamCount(0),
    m_RenderBlockAlign(0),
    m_RenderBytesPerSec(0),
    m_CaptureBlockAlign(0),
    m_CaptureBytesPerSec(0)
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

NTSTATUS CMiniportWaveRT::Init(ULONG CableIndex, CCircularRingBuffer* pSharedBuffer, BOOLEAN* pbEnabled) {
    PAGED_CODE();
    m_CableIndex = CableIndex;
    m_pSharedBuffer = pSharedBuffer;
    m_pbEnabled = pbEnabled;
    return STATUS_SUCCESS;
}

STDMETHODIMP_(NTSTATUS) CMiniportWaveRT::Init(PUNKNOWN UnknownAdapter, PRESOURCELIST ResourceList, PPORTWAVERT Port) {
    PAGED_CODE();
    UNREFERENCED_PARAMETER(UnknownAdapter);
    UNREFERENCED_PARAMETER(ResourceList);
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

    // Bridge pins (Pin 1 & 2) tidak mendukung stream creation
    if (Pin == PIN_WAVE_RENDER_SOURCE || Pin == PIN_WAVE_CAPTURE_SINK) {
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    if (Pin == PIN_WAVE_RENDER_SINK) {
        Capture = FALSE;
    } else if (Pin == PIN_WAVE_CAPTURE_SOURCE) {
        Capture = TRUE;
    } else {
        return STATUS_INVALID_PARAMETER;
    }

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

// Endpoint Node GUIDs
static const GUID g_KsNodeTypeSpeaker = { STATICGUIDOF(KSNODETYPE_SPEAKER) };
static const GUID g_KsNodeTypeMicrophone = { STATICGUIDOF(KSNODETYPE_MICROPHONE) };

// Bridge pin data range (koneksi analog ke audio endpoint device)
static const KSDATARANGE g_PinDataRangesBridge[] = {
    {
        sizeof(KSDATARANGE),
        0,
        0,
        0,
        { STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO) },
        { STATICGUIDOF(KSDATAFORMAT_SUBTYPE_ANALOG) },
        { STATICGUIDOF(KSDATAFORMAT_SPECIFIER_NONE) }
    }
};

static const PKSDATARANGE g_PinDataRangePointersBridge[] = {
    (PKSDATARANGE)&g_PinDataRangesBridge[0]
};

static const KSDATARANGE_AUDIO g_PinDataRangesAudio[] = {
    {
        {
            sizeof(KSDATARANGE_AUDIO),
            0,
            0,
            0,
            { STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO) },
            { STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM) },
            { STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX) }
        },
        2,      // MaximumChannels
        // Format DIKUNCI ke satu nilai saja (24-bit / 48 kHz / stereo).
        // Kalau rentang dibiarkan longgar, Windows memilih default berbeda untuk
        // endpoint Playback (48 kHz) dan Recording (44.1 kHz); karena loopback
        // memindahkan byte mentah tanpa resampling, selisih rate itu membuat
        // backlog tumbuh 23.4 KB/detik dan berakhir pada distorsi permanen.
        24,     // MinimumBitsPerSample
        24,     // MaximumBitsPerSample
        48000,  // MinimumSampleFrequency
        48000   // MaximumSampleFrequency
    }
};

static const PKSDATARANGE g_PinDataRangePointers[] = {
    (PKSDATARANGE)&g_PinDataRangesAudio[0]
};

static const PCPIN_DESCRIPTOR g_MiniportPins[] = {
    // Pin 0: WaveRT Render Host Streaming Pin (Playback Sink)
    {
        1, 1, 0, NULL,
        {
            0, NULL, 0, NULL,
            SIZEOF_ARRAY(g_PinDataRangePointers), g_PinDataRangePointers,
            KSPIN_DATAFLOW_IN, KSPIN_COMMUNICATION_SINK,
            &KSCATEGORY_AUDIO, NULL, 0
        }
    },
    // Pin 1: WaveRT Render Source (Physical Connection to Topology Pin 0)
    {
        0, 0, 0, NULL,
        {
            0, NULL, 0, NULL,
            SIZEOF_ARRAY(g_PinDataRangePointersBridge), g_PinDataRangePointersBridge,
            KSPIN_DATAFLOW_OUT, KSPIN_COMMUNICATION_NONE,
            &KSCATEGORY_AUDIO, NULL, 0
        }
    },
    // Pin 2: WaveRT Capture Sink (Physical Connection from Topology Pin 3)
    {
        0, 0, 0, NULL,
        {
            0, NULL, 0, NULL,
            SIZEOF_ARRAY(g_PinDataRangePointersBridge), g_PinDataRangePointersBridge,
            KSPIN_DATAFLOW_IN, KSPIN_COMMUNICATION_NONE,
            &KSCATEGORY_AUDIO, NULL, 0
        }
    },
    // Pin 3: WaveRT Capture Host Streaming Pin (Recording Source)
    {
        1, 1, 0, NULL,
        {
            0, NULL, 0, NULL,
            SIZEOF_ARRAY(g_PinDataRangePointers), g_PinDataRangePointers,
            KSPIN_DATAFLOW_OUT, KSPIN_COMMUNICATION_SINK,
            &KSCATEGORY_AUDIO, NULL, 0
        }
    }
};

//=====================================================================
// Internal Topology Connections
// Menghubungkan Host Streaming Pin ke Endpoint Bridge Pin
//=====================================================================
static const PCCONNECTION_DESCRIPTOR g_MiniportConnections[] = {
    // Playback: Host In (Pin 0) -> Speaker Bridge Out (Pin 1)
    { PCFILTER_NODE, PIN_WAVE_RENDER_SINK,    PCFILTER_NODE, PIN_WAVE_RENDER_SOURCE },
    // Recording: Mic Bridge In (Pin 2) -> Host Out (Pin 3)
    { PCFILTER_NODE, PIN_WAVE_CAPTURE_SINK,   PCFILTER_NODE, PIN_WAVE_CAPTURE_SOURCE }
};

static const GUID g_MiniportCategories[] = {
    { STATICGUIDOF(KSCATEGORY_AUDIO) },
    { STATICGUIDOF(KSCATEGORY_RENDER) },
    { STATICGUIDOF(KSCATEGORY_CAPTURE) },
    { STATICGUIDOF(KSCATEGORY_REALTIME) }
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
    SIZEOF_ARRAY(g_MiniportConnections),
    g_MiniportConnections,
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

    // Format driver DIKUNCI 48 kHz / 24-bit / stereo, jadi permintaan klien harus
    // kompatibel dengan itu. Kalau tidak, kembalikan STATUS_NO_MATCH.
    //
    // Sebelum ini fungsi mengabaikan ClientDataRange dan selalu membalas 24-bit:
    // aplikasi yang meminta format lain tetap "berhasil" lalu menafsirkan byte
    // dengan format berbeda. Akibatnya laju byte sisi Capture tidak sama dengan
    // sisi Render (loopback byte-based jadi drift) dan suara terdengar robotik.
    if (!IsEqualGUIDAligned(ClientDataRange->MajorFormat, KSDATAFORMAT_TYPE_AUDIO) ||
        !IsEqualGUIDAligned(ClientDataRange->SubFormat, KSDATAFORMAT_SUBTYPE_PCM)) {
        *ResultantFormatLength = 0;
        return STATUS_NO_MATCH;
    }

    if (ClientDataRange->FormatSize >= sizeof(KSDATARANGE_AUDIO)) {
        const KSDATARANGE_AUDIO* pAudio = (const KSDATARANGE_AUDIO*)ClientDataRange;
        if (DEFAULT_CHANNELS > pAudio->MaximumChannels ||
            DEFAULT_BITS_PER_SAMPLE < pAudio->MinimumBitsPerSample ||
            DEFAULT_BITS_PER_SAMPLE > pAudio->MaximumBitsPerSample ||
            DEFAULT_SAMPLE_RATE < pAudio->MinimumSampleFrequency ||
            DEFAULT_SAMPLE_RATE > pAudio->MaximumSampleFrequency) {
            *ResultantFormatLength = 0;
            return STATUS_NO_MATCH;
        }
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
    m_LinearPosition(0),
    m_BytesPerSec(0),
    m_BlockAlign(6)
{
    m_StartTime.QuadPart = 0;
    KeQueryPerformanceCounter(&m_Frequency);
}

CMiniportWaveRTStream::~CMiniportWaveRTStream() {
    // Lepas hitungan stream kalau dihancurkan tanpa SetState(STOP).
    if (m_pMiniport && m_State == KSSTATE_RUN) {
        if (m_bCapture) m_pMiniport->CaptureStreamStopped();
        else            m_pMiniport->RenderStreamStopped();
        m_State = KSSTATE_STOP;
    }
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

    // Sinkronkan parameter ring buffer dengan format stream yang benar-benar
    // dinegosiasikan. Alignment dipakai saat membuang backlog; batas latensi hanya
    // dipasang oleh sisi capture karena hanya capture yang mengonsumsi ring.
    ULONG blockAlign = m_pFormat->WaveFormatExt.Format.nBlockAlign;
    ULONG bytesPerSec = m_pFormat->WaveFormatExt.Format.nAvgBytesPerSec;
    if (blockAlign == 0) blockAlign = DEFAULT_CHANNELS * (DEFAULT_CONTAINER_SIZE / 8);
    if (bytesPerSec == 0) bytesPerSec = DEFAULT_SAMPLE_RATE * blockAlign;

    CCircularRingBuffer* pRing = m_pMiniport ? m_pMiniport->GetSharedBuffer() : NULL;
    if (pRing) {
        pRing->SetAlignment(blockAlign);
        if (m_bCapture) {
            // Cushion 50 ms: jarak aman terhadap jitter polling WASAPI (~10 ms).
            // Batas atas 150 ms: kalau backlog melewatinya, buang sampai cushion.
            pRing->SetPreroll((bytesPerSec * 50) / 1000);
            pRing->SetLatencyLimit((bytesPerSec * 150) / 1000);
        }
    }

    m_BytesPerSec = bytesPerSec;
    m_BlockAlign = blockAlign;

    // Catat format stream ke miniport supaya bisa dibaca lewat IOCTL status.
    if (m_pMiniport) {
        m_pMiniport->SetStreamFormat(m_bCapture, blockAlign, bytesPerSec);
    }

    return STATUS_SUCCESS;
}

STDMETHODIMP_(NTSTATUS) CMiniportWaveRTStream::SetState(KSSTATE State) {
    if (m_State == State) return STATUS_SUCCESS;

    KSSTATE oldState = m_State;
    m_State = State;

    // Lacak jumlah stream yang benar-benar berjalan per arah. Sisi render memakai
    // angka capture untuk berhenti menulis ke ring saat tidak ada konsumen.
    if (m_pMiniport) {
        if (m_bCapture) {
            if (oldState != KSSTATE_RUN && State == KSSTATE_RUN) {
                m_pMiniport->CaptureStreamStarted();
            } else if (oldState == KSSTATE_RUN && State != KSSTATE_RUN) {
                m_pMiniport->CaptureStreamStopped();
            }
        } else {
            if (oldState != KSSTATE_RUN && State == KSSTATE_RUN) {
                m_pMiniport->RenderStreamStarted();
            } else if (oldState == KSSTATE_RUN && State != KSSTATE_RUN) {
                m_pMiniport->RenderStreamStopped();
            }
        }
    }

    if (State == KSSTATE_RUN) {
        KeQueryPerformanceCounter(&m_StartTime);
        m_LinearPosition = 0;

        if (m_bCapture && m_pMiniport) {
            CCircularRingBuffer* pRing = m_pMiniport->GetSharedBuffer();
            // Buang backlog BASI saja (mis. render sudah jalan lama sebelum recorder
            // dibuka). Ambang 500 ms dipakai supaya siklus STOP/RUN normal tidak
            // memotong audio yang sedang berjalan.
            if (pRing && m_BytesPerSec > 0 &&
                pRing->GetDataAvailable() > (m_BytesPerSec / 2)) {
                pRing->Reset();
            }
        }
    } else if (State == KSSTATE_STOP) {
        m_LinearPosition = 0;
        if (m_pDmaBuffer && m_DmaBufferSize > 0) {
            RtlZeroMemory(m_pDmaBuffer, m_DmaBufferSize);
        }
    }

    return STATUS_SUCCESS;
}

STDMETHODIMP_(NTSTATUS) CMiniportWaveRTStream::SetFormat(PKSDATAFORMAT DataFormat) {
    UNREFERENCED_PARAMETER(DataFormat);
    return STATUS_SUCCESS;
}

STDMETHODIMP_(NTSTATUS) CMiniportWaveRTStream::GetPosition(KSAUDIO_POSITION* pPosition) {
    if (!pPosition || m_DmaBufferSize == 0) return STATUS_INVALID_PARAMETER;

    if (m_State != KSSTATE_RUN) {
        pPosition->PlayOffset = 0;
        pPosition->WriteOffset = 0;
        return STATUS_SUCCESS;
    }

    LARGE_INTEGER now, elapsed;
    now = KeQueryPerformanceCounter(NULL);
    elapsed.QuadPart = now.QuadPart - m_StartTime.QuadPart;

    // Hitung bytes per detik dan block align secara akurat berdasar format stream aktual
    ULONG bytesPerSec = 0;
    ULONG blockAlign = 4;
    if (m_pFormat) {
        if (m_pFormat->WaveFormatExt.Format.nAvgBytesPerSec > 0) {
            bytesPerSec = m_pFormat->WaveFormatExt.Format.nAvgBytesPerSec;
        }
        if (m_pFormat->WaveFormatExt.Format.nBlockAlign > 0) {
            blockAlign = m_pFormat->WaveFormatExt.Format.nBlockAlign;
        }
    }
    if (bytesPerSec == 0) {
        bytesPerSec = DEFAULT_SAMPLE_RATE * DEFAULT_CHANNELS * (DEFAULT_CONTAINER_SIZE / 8);
        blockAlign = DEFAULT_CHANNELS * (DEFAULT_CONTAINER_SIZE / 8);
    }

    if (m_Frequency.QuadPart == 0) {
        KeQueryPerformanceCounter(&m_Frequency);
        if (m_Frequency.QuadPart == 0) m_Frequency.QuadPart = 10000000;
    }

    ULONGLONG totalBytesStreamed = (elapsed.QuadPart * bytesPerSec) / m_Frequency.QuadPart;

    // Selaraskan posisi ke batas audio frame (nBlockAlign) agar tidak terjadi pergeseran byte (fase/noise/kresek-kresek)
    totalBytesStreamed = (totalBytesStreamed / blockAlign) * blockAlign;

    // PlayOffset & WriteOffset pada WaveRT WAJIB merupakan byte offset di dalam circular DMA buffer (modulo buffer size)
    ULONG dmaPos = (ULONG)(totalBytesStreamed % m_DmaBufferSize);
    dmaPos = (dmaPos / blockAlign) * blockAlign;

    pPosition->PlayOffset = dmaPos;
    pPosition->WriteOffset = dmaPos;

    // Loopback transfer berdasar delta bytes aktual sejak pembacaan sebelumnya
    CCircularRingBuffer* pRing = m_pMiniport ? m_pMiniport->GetSharedBuffer() : NULL;
    if (pRing && m_pDmaBuffer && totalBytesStreamed > m_LinearPosition) {
        ULONGLONG delta = totalBytesStreamed - m_LinearPosition;
        delta = (delta / blockAlign) * blockAlign;

        if (delta > 0) {
            if (delta > m_DmaBufferSize) {
                delta = m_DmaBufferSize;
                delta = (delta / blockAlign) * blockAlign;
            }

            ULONG prevPos = (ULONG)(m_LinearPosition % m_DmaBufferSize);
            prevPos = (prevPos / blockAlign) * blockAlign;
            ULONG toTransfer = (ULONG)delta;

            // Tangani pembagian chunk jika posisi melewati batas circular buffer DMA
            BOOLEAN bCableActive = m_pMiniport ? m_pMiniport->IsCableEnabled() : TRUE;
            // Render hanya mengisi ring kalau ada stream capture yang benar-benar
            // berjalan. Tanpa cek ini, ring terisi audio basi saat tidak ada yang
            // merekam dan counter overrun naik tanpa arti.
            BOOLEAN bHasConsumer = m_pMiniport ? m_pMiniport->IsCaptureActive() : TRUE;
            BOOLEAN bWriteToRing = bCableActive && bHasConsumer;

            if (prevPos + toTransfer <= m_DmaBufferSize) {
                if (!m_bCapture) {
                    if (bWriteToRing) {
                        pRing->Write((PUCHAR)m_pDmaBuffer + prevPos, toTransfer);
                    }
                } else {
                    if (bCableActive) {
                        pRing->Read((PUCHAR)m_pDmaBuffer + prevPos, toTransfer);
                    } else {
                        RtlZeroMemory((PUCHAR)m_pDmaBuffer + prevPos, toTransfer);
                    }
                }
            } else {
                ULONG firstChunk = m_DmaBufferSize - prevPos;
                firstChunk = (firstChunk / blockAlign) * blockAlign;
                ULONG secondChunk = toTransfer - firstChunk;
                if (!m_bCapture) {
                    if (bWriteToRing) {
                        pRing->Write((PUCHAR)m_pDmaBuffer + prevPos, firstChunk);
                        pRing->Write((PUCHAR)m_pDmaBuffer, secondChunk);
                    }
                } else {
                    if (bCableActive) {
                        pRing->Read((PUCHAR)m_pDmaBuffer + prevPos, firstChunk);
                        pRing->Read((PUCHAR)m_pDmaBuffer, secondChunk);
                    } else {
                        RtlZeroMemory((PUCHAR)m_pDmaBuffer + prevPos, firstChunk);
                        RtlZeroMemory((PUCHAR)m_pDmaBuffer, secondChunk);
                    }
                }
            }
            m_LinearPosition += toTransfer;
        }
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
    RtlZeroMemory(m_pDmaBuffer, m_DmaBufferSize);

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

STDMETHODIMP_(NTSTATUS) CMiniportWaveRTStream::GetClockRegister(KSRTAUDIO_HWREGISTER* pRegister) {
    UNREFERENCED_PARAMETER(pRegister);
    return STATUS_NOT_IMPLEMENTED;
}

STDMETHODIMP_(NTSTATUS) CMiniportWaveRTStream::GetPositionRegister(KSRTAUDIO_HWREGISTER* pRegister) {
    UNREFERENCED_PARAMETER(pRegister);
    return STATUS_NOT_IMPLEMENTED;
}

STDMETHODIMP_(VOID) CMiniportWaveRTStream::GetHWLatency(KSRTAUDIO_HWLATENCY* pLatency) {
    if (pLatency) {
        pLatency->FifoSize = 0;
        pLatency->ChipsetDelay = 0;
        pLatency->CodecDelay = 0;
    }
}

// IUnknown implementation untuk stream
STDMETHODIMP_(NTSTATUS) CMiniportWaveRTStream::NonDelegatingQueryInterface(REFIID InterfaceId, PVOID* Interface) {
    if (!Interface) return STATUS_INVALID_PARAMETER;

    if (IsEqualGUIDAligned(InterfaceId, IID_IUnknown)) {
        *Interface = PVOID(PUNKNOWN(PMINIPORTWAVERTSTREAM(this)));
    } else if (IsEqualGUIDAligned(InterfaceId, IID_IMiniportWaveRTStream)) {
        *Interface = PVOID(PMINIPORTWAVERTSTREAM(this));
    } else {
        *Interface = NULL;
        return STATUS_INVALID_PARAMETER;
    }

    PUNKNOWN(*Interface)->AddRef();
    return STATUS_SUCCESS;
}
