#include "mintopo.h"

// GUID Endpoint untuk Bridge Pin
static const GUID g_KsNodeTypeSpeaker = { STATICGUIDOF(KSNODETYPE_SPEAKER) };
static const GUID g_KsNodeTypeMicrophone = { STATICGUIDOF(KSNODETYPE_MICROPHONE) };

// Format bridge (koneksi analog internal filter)
static const KSDATARANGE g_TopoBridgeRanges[] = {
    {
        sizeof(KSDATARANGE),
        0, 0, 0,
        { STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO) },
        { STATICGUIDOF(KSDATAFORMAT_SUBTYPE_ANALOG) },
        { STATICGUIDOF(KSDATAFORMAT_SPECIFIER_NONE) }
    }
};

static const PKSDATARANGE g_TopoBridgeRangePtrs[] = {
    (PKSDATARANGE)&g_TopoBridgeRanges[0]
};

// 4 Pin Descriptor untuk Topology Filter
static const PCPIN_DESCRIPTOR g_TopoPins[] = {
    // Pin 0: PIN_TOPO_WAVEOUT_SINK (Physical connection from WaveRT Render)
    {
        0, 0, 0, NULL,
        {
            0, NULL, 0, NULL,
            SIZEOF_ARRAY(g_TopoBridgeRangePtrs), g_TopoBridgeRangePtrs,
            KSPIN_DATAFLOW_IN, KSPIN_COMMUNICATION_NONE,
            &KSCATEGORY_AUDIO, NULL, 0
        }
    },
    // Pin 1: PIN_TOPO_SPEAKER_BRIDGE (Bridge Pin to Speakers / Line Out)
    {
        0, 0, 0, NULL,
        {
            0, NULL, 0, NULL,
            SIZEOF_ARRAY(g_TopoBridgeRangePtrs), g_TopoBridgeRangePtrs,
            KSPIN_DATAFLOW_OUT, KSPIN_COMMUNICATION_NONE,
            &g_KsNodeTypeSpeaker, NULL, 0
        }
    },
    // Pin 2: PIN_TOPO_MIC_BRIDGE (Bridge Pin from Microphone / Line In)
    {
        0, 0, 0, NULL,
        {
            0, NULL, 0, NULL,
            SIZEOF_ARRAY(g_TopoBridgeRangePtrs), g_TopoBridgeRangePtrs,
            KSPIN_DATAFLOW_IN, KSPIN_COMMUNICATION_NONE,
            &g_KsNodeTypeMicrophone, NULL, 0
        }
    },
    // Pin 3: PIN_TOPO_WAVEIN_SOURCE (Physical connection to WaveRT Capture)
    {
        0, 0, 0, NULL,
        {
            0, NULL, 0, NULL,
            SIZEOF_ARRAY(g_TopoBridgeRangePtrs), g_TopoBridgeRangePtrs,
            KSPIN_DATAFLOW_OUT, KSPIN_COMMUNICATION_NONE,
            &KSCATEGORY_AUDIO, NULL, 0
        }
    }
};

// Koneksi internal filter Topology:
// Pin 0 (Wave Out) -> Pin 1 (Speaker)
// Pin 2 (Mic) -> Pin 3 (Wave In)
static const PCCONNECTION_DESCRIPTOR g_TopoConnections[] = {
    { PCFILTER_NODE, PIN_TOPO_WAVEOUT_SINK, PCFILTER_NODE, PIN_TOPO_SPEAKER_BRIDGE },
    { PCFILTER_NODE, PIN_TOPO_MIC_BRIDGE,   PCFILTER_NODE, PIN_TOPO_WAVEIN_SOURCE }
};

static const GUID g_TopoCategories[] = {
    { STATICGUIDOF(KSCATEGORY_TOPOLOGY) },
    { STATICGUIDOF(KSCATEGORY_AUDIO) }
};

static const PCFILTER_DESCRIPTOR g_TopoFilterDescriptor = {
    0,
    NULL,
    sizeof(PCPIN_DESCRIPTOR),
    SIZEOF_ARRAY(g_TopoPins),
    g_TopoPins,
    sizeof(PCNODE_DESCRIPTOR),
    0,
    NULL,
    SIZEOF_ARRAY(g_TopoConnections),
    g_TopoConnections,
    SIZEOF_ARRAY(g_TopoCategories),
    g_TopoCategories
};

#pragma code_seg("PAGE")

CMiniportTopology::CMiniportTopology(PUNKNOWN pUnknownOuter) :
    CUnknown(pUnknownOuter),
    m_pPort(NULL),
    m_CableIndex(0)
{
    PAGED_CODE();
}

CMiniportTopology::~CMiniportTopology() {
    PAGED_CODE();
    if (m_pPort) {
        m_pPort->Release();
        m_pPort = NULL;
    }
}

STDMETHODIMP_(NTSTATUS) CMiniportTopology::Init(
    PUNKNOWN        UnknownAdapter,
    PRESOURCELIST   ResourceList,
    PPORTTOPOLOGY   Port
) {
    PAGED_CODE();
    UNREFERENCED_PARAMETER(UnknownAdapter);
    UNREFERENCED_PARAMETER(ResourceList);

    if (!Port) return STATUS_INVALID_PARAMETER;
    m_pPort = Port;
    m_pPort->AddRef();

    return STATUS_SUCCESS;
}

STDMETHODIMP CMiniportTopology::NonDelegatingQueryInterface(REFIID Interface, PVOID* Object) {
    PAGED_CODE();
    if (!Object) return STATUS_INVALID_PARAMETER;

    if (IsEqualGUIDAligned(Interface, IID_IUnknown)) {
        *Object = PVOID(PUNKNOWN(this));
    } else if (IsEqualGUIDAligned(Interface, IID_IMiniport)) {
        *Object = PVOID(PMINIPORT(this));
    } else if (IsEqualGUIDAligned(Interface, IID_IMiniportTopology)) {
        *Object = PVOID(PMINIPORTTOPOLOGY(this));
    } else {
        *Object = NULL;
        return STATUS_INVALID_PARAMETER;
    }

    PUNKNOWN(*Object)->AddRef();
    return STATUS_SUCCESS;
}

STDMETHODIMP_(NTSTATUS) CMiniportTopology::GetDescription(PPCFILTER_DESCRIPTOR* ppFilterDescriptor) {
    PAGED_CODE();
    if (!ppFilterDescriptor) return STATUS_INVALID_PARAMETER;
    *ppFilterDescriptor = (PPCFILTER_DESCRIPTOR)&g_TopoFilterDescriptor;
    return STATUS_SUCCESS;
}

STDMETHODIMP_(NTSTATUS) CMiniportTopology::DataRangeIntersection(
    ULONG           PinId,
    PKSDATARANGE    ClientDataRange,
    PKSDATARANGE    MyDataRange,
    ULONG           OutputBufferLength,
    PVOID           ResultantFormat,
    PULONG          ResultantFormatLength
) {
    PAGED_CODE();
    UNREFERENCED_PARAMETER(PinId);
    UNREFERENCED_PARAMETER(ClientDataRange);
    UNREFERENCED_PARAMETER(MyDataRange);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(ResultantFormat);
    UNREFERENCED_PARAMETER(ResultantFormatLength);
    return STATUS_NOT_IMPLEMENTED;
}

#pragma code_seg()
