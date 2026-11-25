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
    CABLE_A = 0, // Virtual Cable A
    CABLE_B = 1, // Virtual Cable B
    CABLE_C = 2, // Virtual Cable C
    CABLE_D = 3, // Virtual Cable D
    CABLE_E = 4  // Virtual Cable E
};

// Default format audio virtual.
//
// PENTING: pin Render dan pin Capture WAJIB menegosiasikan format yang identik
// (sample rate + container size). Loopback antar-kabel memindahkan PCM mentah
// lewat ring buffer byte-based tanpa resampling, jadi kalau kedua sisi beda rate
// (mis. Render 48 kHz vs Capture 44.1 kHz) backlog tumbuh linear, latensi naik
// terus, dan akhirnya overflow terus-menerus -> suara warble/robotik.
#define DEFAULT_SAMPLE_RATE         48000
#define DEFAULT_CHANNELS            2
#define DEFAULT_BITS_PER_SAMPLE     24
#define DEFAULT_CONTAINER_SIZE      24   // 24-bit = 3 byte/sample -> nBlockAlign 6
#define DEFAULT_BUFFER_MS           500  // durasi ring buffer default (dipakai juga saat restore gagal)

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
    BOOLEAN                 bEnabled;
    CCircularRingBuffer*    pSharedBuffer;
    PPORT                   pPort;
    PMINIPORT               pMiniport;
    PPORT                   pPortTopo;
    PMINIPORT               pMiniportTopo;
} CABLE_CONTEXT, *PCABLE_CONTEXT;
