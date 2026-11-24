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

// 4 Pin Descriptor untuk Topology Filter per Virtual Cable
#define DEFINE_TOPO_PINS_FOR_CABLE(renderNameGuid, captureNameGuid) { \n    { 0, 0, 0, NULL, { 0, NULL, 0, NULL, SIZEOF_ARRAY(g_TopoBridgeRangePtrs), g_TopoBridgeRangePtrs, KSPIN_DATAFLOW_IN, KSPIN_COMMUNICATION_NONE, &KSCATEGORY_AUDIO, NULL, 0 } }, \n    { 0, 0, 0, NULL, { 0, NULL, 0, NULL, SIZEOF_ARRAY(g_TopoBridgeRangePtrs), g_TopoBridgeRangePtrs, KSPIN_DATAFLOW_OUT, KSPIN_COMMUNICATION_NONE, &g_KsNodeTypeSpeaker, &(renderNameGuid), 0 } }, \n    { 0, 0, 0, NULL, { 0, NULL, 0, NULL, SIZEOF_ARRAY(g_TopoBridgeRangePtrs), g_TopoBridgeRangePtrs, KSPIN_DATAFLOW_IN, KSPIN_COMMUNICATION_NONE, &g_KsNodeTypeMicrophone, &(captureNameGuid), 0 } }, \n    { 0, 0, 0, NULL, { 0, NULL, 0, NULL, SIZEOF_ARRAY(g_TopoBridgeRangePtrs), g_TopoBridgeRangePtrs, KSPIN_DATAFLOW_OUT, KSPIN_COMMUNICATION_NONE, &KSCATEGORY_AUDIO, NULL, 0 } } \n}

static const PCPIN_DESCRIPTOR g_TopoPins_Cable[MAX_CABLES][4] = {
    DEFINE_TOPO_PINS_FOR_CABLE(PINNAME_MCVAD_CABLE_A_RENDER, PINNAME_MCVAD_CABLE_A_CAPTURE),
    DEFINE_TOPO_PINS_FOR_CABLE(PINNAME_MCVAD_CABLE_B_RENDER, PINNAME_MCVAD_CABLE_B_CAPTURE),
    DEFINE_TOPO_PINS_FOR_CABLE(PINNAME_MCVAD_CABLE_C_RENDER, PINNAME_MCVAD_CABLE_C_CAPTURE),
    DEFINE_TOPO_PINS_FOR_CABLE(PINNAME_MCVAD_CABLE_D_RENDER, PINNAME_MCVAD_CABLE_D_CAPTURE),
    DEFINE_TOPO_PINS_FOR_CABLE(PINNAME_MCVAD_CABLE_E_RENDER, PINNAME_MCVAD_CABLE_E_CAPTURE)
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

#define DEFINE_TOPO_FILTER_FOR_CABLE(idx) { \n    0, NULL, sizeof(PCPIN_DESCRIPTOR), 4, g_TopoPins_Cable[idx], \n    sizeof(PCNODE_DESCRIPTOR), 0, NULL, \n    SIZEOF_ARRAY(g_TopoConnections), g_TopoConnections, \n    SIZEOF_ARRAY(g_TopoCategories), g_TopoCategories \n}

static const PCFILTER_DESCRIPTOR g_TopoFilterDescriptors[MAX_CABLES] = {
    DEFINE_TOPO_FILTER_FOR_CABLE(0),
    DEFINE_TOPO_FILTER_FOR_CABLE(1),
    DEFINE_TOPO_FILTER_FOR_CABLE(2),
    DEFINE_TOPO_FILTER_FOR_CABLE(3),
    DEFINE_TOPO_FILTER_FOR_CABLE(4)
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
    ULONG idx = (m_CableIndex < MAX_CABLES) ? m_CableIndex : 0;
    *ppFilterDescriptor = (PPCFILTER_DESCRIPTOR)&g_TopoFilterDescriptors[idx];
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
