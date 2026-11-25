#include "mintopo.h"

// GUID Endpoint untuk Bridge Pin
//
// Render memakai KSNODETYPE_LINE_CONNECTOR, BUKAN KSNODETYPE_SPEAKER.
// AudioEndpointBuilder memperlakukan pin berkategori speaker sebagai kasus khusus:
// nama & ikon endpoint dipaksa ke default form factor ("Speakers" + mmres.dll,-3010)
// dan hasil KSPROPERTY_PIN_NAME diabaikan, sehingga entri MediaCategories tidak pernah
// terpakai. Pin Capture tidak terkena special-case ini.
static const GUID g_KsNodeTypeLineConnector = { STATICGUIDOF(KSNODETYPE_LINE_CONNECTOR) };
static const GUID g_KsNodeTypeMicrophone = { STATICGUIDOF(KSNODETYPE_MICROPHONE) };

// GUID Pin Names untuk 5 kabel virtual (Render & Capture)
// MediaCategories di INF memetakan GUID ini ke "DF-OpenVCable A" s/d "DF-OpenVCable E"
static const GUID PINNAME_MCVAD_CABLE_A_RENDER  = { 0xd48c1101, 0x7f6a, 0x4a8e, { 0x95, 0x41, 0x67, 0x89, 0x0a, 0xbc, 0x00, 0x01 } };
static const GUID PINNAME_MCVAD_CABLE_A_CAPTURE = { 0xd48c1101, 0x7f6a, 0x4a8e, { 0x95, 0x41, 0x67, 0x89, 0x0a, 0xbc, 0x00, 0x02 } };
static const GUID PINNAME_MCVAD_CABLE_B_RENDER  = { 0xd48c1102, 0x7f6a, 0x4a8e, { 0x95, 0x41, 0x67, 0x89, 0x0a, 0xbc, 0x00, 0x01 } };
static const GUID PINNAME_MCVAD_CABLE_B_CAPTURE = { 0xd48c1102, 0x7f6a, 0x4a8e, { 0x95, 0x41, 0x67, 0x89, 0x0a, 0xbc, 0x00, 0x02 } };
static const GUID PINNAME_MCVAD_CABLE_C_RENDER  = { 0xd48c1103, 0x7f6a, 0x4a8e, { 0x95, 0x41, 0x67, 0x89, 0x0a, 0xbc, 0x00, 0x01 } };
static const GUID PINNAME_MCVAD_CABLE_C_CAPTURE = { 0xd48c1103, 0x7f6a, 0x4a8e, { 0x95, 0x41, 0x67, 0x89, 0x0a, 0xbc, 0x00, 0x02 } };
static const GUID PINNAME_MCVAD_CABLE_D_RENDER  = { 0xd48c1104, 0x7f6a, 0x4a8e, { 0x95, 0x41, 0x67, 0x89, 0x0a, 0xbc, 0x00, 0x01 } };
static const GUID PINNAME_MCVAD_CABLE_D_CAPTURE = { 0xd48c1104, 0x7f6a, 0x4a8e, { 0x95, 0x41, 0x67, 0x89, 0x0a, 0xbc, 0x00, 0x02 } };
static const GUID PINNAME_MCVAD_CABLE_E_RENDER  = { 0xd48c1105, 0x7f6a, 0x4a8e, { 0x95, 0x41, 0x67, 0x89, 0x0a, 0xbc, 0x00, 0x01 } };
static const GUID PINNAME_MCVAD_CABLE_E_CAPTURE = { 0xd48c1105, 0x7f6a, 0x4a8e, { 0x95, 0x41, 0x67, 0x89, 0x0a, 0xbc, 0x00, 0x02 } };

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
//
// Pin 0 dan 3 adalah bridge ke WaveRT (KSCATEGORY_AUDIO, tanpa nama) — bukan endpoint.
// Pin 1 dan 2 adalah bridge pin endpoint fisik:
//   Category : KSNODETYPE_LINE_CONNECTOR / KSNODETYPE_MICROPHONE.
//   Name     : GUID custom yang dipetakan ke "DF-OpenVCable A" s/d "DF-OpenVCable E"
//              lewat MediaCategories (lihat [MCVAD_Device.AddReg] di INF).
//
// Catatan riwayat: versi <= 1.0.0.7 memakai KSNODETYPE_SPEAKER untuk pin Render.
// KSPROPERTY_PIN_NAME saat itu sudah benar mengembalikan "DF-OpenVCable A", tetapi
// AudioEndpointBuilder tetap menampilkan "Speakers" karena kategori speaker di-hardcode.
// Memakai GUID custom sebagai Category juga salah (tipe endpoint tidak dikenali).
// Pola yang benar = kategori standar non-speaker + Name GUID custom.
static const PCPIN_DESCRIPTOR g_TopoPins_Cable[MAX_CABLES][4] = {
    // Cable A
    {
        { 0, 0, 0, NULL, { 0, NULL, 0, NULL, SIZEOF_ARRAY(g_TopoBridgeRangePtrs), g_TopoBridgeRangePtrs, KSPIN_DATAFLOW_IN,  KSPIN_COMMUNICATION_NONE, &KSCATEGORY_AUDIO,       NULL,                          0 } },
        { 0, 0, 0, NULL, { 0, NULL, 0, NULL, SIZEOF_ARRAY(g_TopoBridgeRangePtrs), g_TopoBridgeRangePtrs, KSPIN_DATAFLOW_OUT, KSPIN_COMMUNICATION_NONE, &g_KsNodeTypeLineConnector, &PINNAME_MCVAD_CABLE_A_RENDER, 0 } },
        { 0, 0, 0, NULL, { 0, NULL, 0, NULL, SIZEOF_ARRAY(g_TopoBridgeRangePtrs), g_TopoBridgeRangePtrs, KSPIN_DATAFLOW_IN,  KSPIN_COMMUNICATION_NONE, &g_KsNodeTypeMicrophone, &PINNAME_MCVAD_CABLE_A_CAPTURE, 0 } },
        { 0, 0, 0, NULL, { 0, NULL, 0, NULL, SIZEOF_ARRAY(g_TopoBridgeRangePtrs), g_TopoBridgeRangePtrs, KSPIN_DATAFLOW_OUT, KSPIN_COMMUNICATION_NONE, &KSCATEGORY_AUDIO,       NULL,                          0 } }
    },
    // Cable B
    {
        { 0, 0, 0, NULL, { 0, NULL, 0, NULL, SIZEOF_ARRAY(g_TopoBridgeRangePtrs), g_TopoBridgeRangePtrs, KSPIN_DATAFLOW_IN,  KSPIN_COMMUNICATION_NONE, &KSCATEGORY_AUDIO,       NULL,                          0 } },
        { 0, 0, 0, NULL, { 0, NULL, 0, NULL, SIZEOF_ARRAY(g_TopoBridgeRangePtrs), g_TopoBridgeRangePtrs, KSPIN_DATAFLOW_OUT, KSPIN_COMMUNICATION_NONE, &g_KsNodeTypeLineConnector, &PINNAME_MCVAD_CABLE_B_RENDER, 0 } },
        { 0, 0, 0, NULL, { 0, NULL, 0, NULL, SIZEOF_ARRAY(g_TopoBridgeRangePtrs), g_TopoBridgeRangePtrs, KSPIN_DATAFLOW_IN,  KSPIN_COMMUNICATION_NONE, &g_KsNodeTypeMicrophone, &PINNAME_MCVAD_CABLE_B_CAPTURE, 0 } },
        { 0, 0, 0, NULL, { 0, NULL, 0, NULL, SIZEOF_ARRAY(g_TopoBridgeRangePtrs), g_TopoBridgeRangePtrs, KSPIN_DATAFLOW_OUT, KSPIN_COMMUNICATION_NONE, &KSCATEGORY_AUDIO,       NULL,                          0 } }
    },
    // Cable C
    {
        { 0, 0, 0, NULL, { 0, NULL, 0, NULL, SIZEOF_ARRAY(g_TopoBridgeRangePtrs), g_TopoBridgeRangePtrs, KSPIN_DATAFLOW_IN,  KSPIN_COMMUNICATION_NONE, &KSCATEGORY_AUDIO,       NULL,                          0 } },
        { 0, 0, 0, NULL, { 0, NULL, 0, NULL, SIZEOF_ARRAY(g_TopoBridgeRangePtrs), g_TopoBridgeRangePtrs, KSPIN_DATAFLOW_OUT, KSPIN_COMMUNICATION_NONE, &g_KsNodeTypeLineConnector, &PINNAME_MCVAD_CABLE_C_RENDER, 0 } },
        { 0, 0, 0, NULL, { 0, NULL, 0, NULL, SIZEOF_ARRAY(g_TopoBridgeRangePtrs), g_TopoBridgeRangePtrs, KSPIN_DATAFLOW_IN,  KSPIN_COMMUNICATION_NONE, &g_KsNodeTypeMicrophone, &PINNAME_MCVAD_CABLE_C_CAPTURE, 0 } },
        { 0, 0, 0, NULL, { 0, NULL, 0, NULL, SIZEOF_ARRAY(g_TopoBridgeRangePtrs), g_TopoBridgeRangePtrs, KSPIN_DATAFLOW_OUT, KSPIN_COMMUNICATION_NONE, &KSCATEGORY_AUDIO,       NULL,                          0 } }
    },
    // Cable D
    {
        { 0, 0, 0, NULL, { 0, NULL, 0, NULL, SIZEOF_ARRAY(g_TopoBridgeRangePtrs), g_TopoBridgeRangePtrs, KSPIN_DATAFLOW_IN,  KSPIN_COMMUNICATION_NONE, &KSCATEGORY_AUDIO,       NULL,                          0 } },
        { 0, 0, 0, NULL, { 0, NULL, 0, NULL, SIZEOF_ARRAY(g_TopoBridgeRangePtrs), g_TopoBridgeRangePtrs, KSPIN_DATAFLOW_OUT, KSPIN_COMMUNICATION_NONE, &g_KsNodeTypeLineConnector, &PINNAME_MCVAD_CABLE_D_RENDER, 0 } },
        { 0, 0, 0, NULL, { 0, NULL, 0, NULL, SIZEOF_ARRAY(g_TopoBridgeRangePtrs), g_TopoBridgeRangePtrs, KSPIN_DATAFLOW_IN,  KSPIN_COMMUNICATION_NONE, &g_KsNodeTypeMicrophone, &PINNAME_MCVAD_CABLE_D_CAPTURE, 0 } },
        { 0, 0, 0, NULL, { 0, NULL, 0, NULL, SIZEOF_ARRAY(g_TopoBridgeRangePtrs), g_TopoBridgeRangePtrs, KSPIN_DATAFLOW_OUT, KSPIN_COMMUNICATION_NONE, &KSCATEGORY_AUDIO,       NULL,                          0 } }
    },
    // Cable E
    {
        { 0, 0, 0, NULL, { 0, NULL, 0, NULL, SIZEOF_ARRAY(g_TopoBridgeRangePtrs), g_TopoBridgeRangePtrs, KSPIN_DATAFLOW_IN,  KSPIN_COMMUNICATION_NONE, &KSCATEGORY_AUDIO,       NULL,                          0 } },
        { 0, 0, 0, NULL, { 0, NULL, 0, NULL, SIZEOF_ARRAY(g_TopoBridgeRangePtrs), g_TopoBridgeRangePtrs, KSPIN_DATAFLOW_OUT, KSPIN_COMMUNICATION_NONE, &g_KsNodeTypeLineConnector, &PINNAME_MCVAD_CABLE_E_RENDER, 0 } },
        { 0, 0, 0, NULL, { 0, NULL, 0, NULL, SIZEOF_ARRAY(g_TopoBridgeRangePtrs), g_TopoBridgeRangePtrs, KSPIN_DATAFLOW_IN,  KSPIN_COMMUNICATION_NONE, &g_KsNodeTypeMicrophone, &PINNAME_MCVAD_CABLE_E_CAPTURE, 0 } },
        { 0, 0, 0, NULL, { 0, NULL, 0, NULL, SIZEOF_ARRAY(g_TopoBridgeRangePtrs), g_TopoBridgeRangePtrs, KSPIN_DATAFLOW_OUT, KSPIN_COMMUNICATION_NONE, &KSCATEGORY_AUDIO,       NULL,                          0 } }
    }
};

// Koneksi internal filter Topology:
// Pin 0 (Wave Out) -> Pin 1 (Speaker)
// Pin 2 (Mic) -> Pin 3 (Wave In)
static const PCCONNECTION_DESCRIPTOR g_TopoConnections[] = {
    { PCFILTER_NODE, PIN_TOPO_WAVEOUT_SINK, PCFILTER_NODE, PIN_TOPO_SPEAKER_BRIDGE },
    { PCFILTER_NODE, PIN_TOPO_MIC_BRIDGE,   PCFILTER_NODE, PIN_TOPO_WAVEIN_SOURCE }
};

// Filter topology TIDAK mendeklarasikan KSCATEGORY_AUDIO.
// Kalau dideklarasikan, PortCls/INF mendaftarkan filter topology sebagai device
// interface audio kedua per kabel ("CableX_Topo"), dan AudioEndpointBuilder lalu
// membuat satu set endpoint tambahan dari pin yang sama -> endpoint duplikat 2x
// di Sound Settings. Registrasi yang benar: KSCATEGORY_TOPOLOGY saja, sesuai
// sample WDK sysvad (filter descriptor topology: CategoryCount = 0).
static const GUID g_TopoCategories[] = {
    { STATICGUIDOF(KSCATEGORY_TOPOLOGY) }
};

static const PCFILTER_DESCRIPTOR g_TopoFilterDescriptors[MAX_CABLES] = {
    { 0, NULL, sizeof(PCPIN_DESCRIPTOR), 4, g_TopoPins_Cable[0], sizeof(PCNODE_DESCRIPTOR), 0, NULL, SIZEOF_ARRAY(g_TopoConnections), g_TopoConnections, SIZEOF_ARRAY(g_TopoCategories), g_TopoCategories },
    { 0, NULL, sizeof(PCPIN_DESCRIPTOR), 4, g_TopoPins_Cable[1], sizeof(PCNODE_DESCRIPTOR), 0, NULL, SIZEOF_ARRAY(g_TopoConnections), g_TopoConnections, SIZEOF_ARRAY(g_TopoCategories), g_TopoCategories },
    { 0, NULL, sizeof(PCPIN_DESCRIPTOR), 4, g_TopoPins_Cable[2], sizeof(PCNODE_DESCRIPTOR), 0, NULL, SIZEOF_ARRAY(g_TopoConnections), g_TopoConnections, SIZEOF_ARRAY(g_TopoCategories), g_TopoCategories },
    { 0, NULL, sizeof(PCPIN_DESCRIPTOR), 4, g_TopoPins_Cable[3], sizeof(PCNODE_DESCRIPTOR), 0, NULL, SIZEOF_ARRAY(g_TopoConnections), g_TopoConnections, SIZEOF_ARRAY(g_TopoCategories), g_TopoCategories },
    { 0, NULL, sizeof(PCPIN_DESCRIPTOR), 4, g_TopoPins_Cable[4], sizeof(PCNODE_DESCRIPTOR), 0, NULL, SIZEOF_ARRAY(g_TopoConnections), g_TopoConnections, SIZEOF_ARRAY(g_TopoCategories), g_TopoCategories }
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
