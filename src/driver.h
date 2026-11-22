#pragma once

#include <portcls.h>
#include <stdunk.h>
#include <ksdebug.h>
#include <ntstrsafe.h>

#define MCVAD_POOL_TAG 'DAVM' // Pool tag: MVAD

#ifndef SIZEOF_ARRAY
#define SIZEOF_ARRAY(a) (sizeof(a)/sizeof((a)[0]))
#endif


// Maksimum jumlah pasang Virtual Audio Cable yang didukung dalam 1 driver
#define MAX_CABLES 5

enum CABLE_INDEX {
    CABLE_1 = 0, // Virtual Cable 1 (Main)
    CABLE_A = 1, // Virtual Cable A
    CABLE_B = 2, // Virtual Cable B
    CABLE_C = 3, // Virtual Cable C
    CABLE_D = 4  // Virtual Cable D
};

// Default format audio virtual
#define DEFAULT_SAMPLE_RATE         48000
#define DEFAULT_CHANNELS            2
#define DEFAULT_BITS_PER_SAMPLE     24
#define DEFAULT_CONTAINER_SIZE      32
#define BUFFER_DURATION_SECONDS     2   // Ukuran buffer memori loopback: 2 detik PCM

// Forward declarations
class CCircularRingBuffer;
class CMiniportWaveRT;
class CMiniportWaveRTStream;
class CMiniportTopology;

// Struktur konteks setiap kabel virtual
typedef struct _CABLE_CONTEXT {
    ULONG                   CableIndex;
    PCWSTR                  Name;
    PCWSTR                  SubdeviceName;
    PCWSTR                  TopoSubdeviceName;
    CCircularRingBuffer*    pSharedBuffer;
    PPORT                   pPort;
    PMINIPORT               pMiniport;
    PPORT                   pPortTopo;
    PMINIPORT               pMiniportTopo;
} CABLE_CONTEXT, *PCABLE_CONTEXT;
