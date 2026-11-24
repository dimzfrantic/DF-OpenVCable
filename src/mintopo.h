#pragma once

#include "driver.h"

// Indeks Pin Topologi Miniport
enum {
    PIN_TOPO_WAVEOUT_SINK = 0,   // Sinyal dari WaveRT Playback Source masuk ke Topology
    PIN_TOPO_SPEAKER_BRIDGE = 1, // Bridge Pin Speaker (Destination Endpoint untuk AudioEndpointBuilder)
    PIN_TOPO_MIC_BRIDGE = 2,     // Bridge Pin Microphone (Source Endpoint untuk AudioEndpointBuilder)
    PIN_TOPO_WAVEIN_SOURCE = 3   // Sinyal dari Topology keluar ke WaveRT Capture Sink
};

// GUID Pin Names untuk 5 kabel virtual (Render & Capture)
// MediaCategories di INF memetakan GUID ini ke "DF-OpenVCable A" s/d "DF-OpenVCable E"
DEFINE_GUID(PINNAME_MCVAD_CABLE_A_RENDER,  0xd48c1101, 0x7f6a, 0x4a8e, 0x95, 0x41, 0x67, 0x89, 0x0a, 0xbc, 0x00, 0x01);
DEFINE_GUID(PINNAME_MCVAD_CABLE_A_CAPTURE, 0xd48c1101, 0x7f6a, 0x4a8e, 0x95, 0x41, 0x67, 0x89, 0x0a, 0xbc, 0x00, 0x02);
DEFINE_GUID(PINNAME_MCVAD_CABLE_B_RENDER,  0xd48c1102, 0x7f6a, 0x4a8e, 0x95, 0x41, 0x67, 0x89, 0x0a, 0xbc, 0x00, 0x01);
DEFINE_GUID(PINNAME_MCVAD_CABLE_B_CAPTURE, 0xd48c1102, 0x7f6a, 0x4a8e, 0x95, 0x41, 0x67, 0x89, 0x0a, 0xbc, 0x00, 0x02);
DEFINE_GUID(PINNAME_MCVAD_CABLE_C_RENDER,  0xd48c1103, 0x7f6a, 0x4a8e, 0x95, 0x41, 0x67, 0x89, 0x0a, 0xbc, 0x00, 0x01);
DEFINE_GUID(PINNAME_MCVAD_CABLE_C_CAPTURE, 0xd48c1103, 0x7f6a, 0x4a8e, 0x95, 0x41, 0x67, 0x89, 0x0a, 0xbc, 0x00, 0x02);
DEFINE_GUID(PINNAME_MCVAD_CABLE_D_RENDER,  0xd48c1104, 0x7f6a, 0x4a8e, 0x95, 0x41, 0x67, 0x89, 0x0a, 0xbc, 0x00, 0x01);
DEFINE_GUID(PINNAME_MCVAD_CABLE_D_CAPTURE, 0xd48c1104, 0x7f6a, 0x4a8e, 0x95, 0x41, 0x67, 0x89, 0x0a, 0xbc, 0x00, 0x02);
DEFINE_GUID(PINNAME_MCVAD_CABLE_E_RENDER,  0xd48c1105, 0x7f6a, 0x4a8e, 0x95, 0x41, 0x67, 0x89, 0x0a, 0xbc, 0x00, 0x01);
DEFINE_GUID(PINNAME_MCVAD_CABLE_E_CAPTURE, 0xd48c1105, 0x7f6a, 0x4a8e, 0x95, 0x41, 0x67, 0x89, 0x0a, 0xbc, 0x00, 0x02);

class CMiniportTopology :
    public IMiniportTopology,
    public CUnknown
{
private:
    PPORTTOPOLOGY m_pPort;
    ULONG         m_CableIndex;

public:
    DECLARE_STD_UNKNOWN();

    CMiniportTopology(PUNKNOWN pUnknownOuter);
    ~CMiniportTopology();

    IMP_IMiniportTopology;

    void SetCableIndex(ULONG CableIndex) { m_CableIndex = CableIndex; }
    ULONG GetCableIndex() const { return m_CableIndex; }
};
