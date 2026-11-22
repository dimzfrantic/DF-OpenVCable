#pragma once

#include "driver.h"

// Indeks Pin Topologi Miniport
enum {
    PIN_TOPO_WAVEOUT_SINK = 0,   // Sinyal dari WaveRT Playback Source masuk ke Topology
    PIN_TOPO_SPEAKER_BRIDGE = 1, // Bridge Pin Speaker (Destination Endpoint untuk AudioEndpointBuilder)
    PIN_TOPO_MIC_BRIDGE = 2,     // Bridge Pin Microphone (Source Endpoint untuk AudioEndpointBuilder)
    PIN_TOPO_WAVEIN_SOURCE = 3   // Sinyal dari Topology keluar ke WaveRT Capture Sink
};

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
