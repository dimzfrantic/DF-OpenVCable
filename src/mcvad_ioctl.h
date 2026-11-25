#pragma once

#ifndef _KERNEL_MODE
#include <windows.h>
#include <winioctl.h>
#endif

#define MCVAD_DEVICE_NAME           L"\\Device\\MCVAD_Control"
#define MCVAD_SYMBOLIC_NAME         L"\\DosDevices\\MCVAD_Control"
#define MCVAD_USERMODE_DEVICE_PATH  L"\\\\.\\MCVAD_Control"

#define FSCTL_MCVAD_BASE            0x8000

// IOCTL Codes
#define IOCTL_MCVAD_GET_CABLE_STATUS \
    CTL_CODE(FSCTL_MCVAD_BASE, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_MCVAD_SET_CABLE_CONFIG \
    CTL_CODE(FSCTL_MCVAD_BASE, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_MCVAD_RESET_CABLE_STATS \
    CTL_CODE(FSCTL_MCVAD_BASE, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)

// Struktur konfigurasi dan status kabel virtual (kompatibel user-mode & kernel-mode)
#pragma pack(push, 1)
typedef struct _MCVAD_CABLE_INFO {
    ULONG       CableIndex;            // 0=A, 1=B, 2=C, 3=D, 4=E
    WCHAR       Name[64];              // e.g. "DF-OpenVCable A"
    ULONG       SampleRate;            // 44100, 48000, 96000, 192000
    ULONG       BitsPerSample;         // 16, 24, 32
    ULONG       Channels;              // 2 (Stereo), 4 (Quad), 6 (5.1), 8 (7.1)
    ULONG       BufferDurationMs;      // Latensi/durasi buffer (ms): 50, 100, 250, 500, 1000
    ULONG       BufferSizeBytes;       // Total kapasitas buffer dalam byte
    ULONG       CurrentBufferFill;     // Jumlah byte audio yang saat ini mengantre
    ULONG       OverrunCount;          // Frekuensi audio terpotong karena buffer penuh
    ULONG       UnderrunCount;         // Frekuensi glitch/silence karena buffer kosong
    BOOLEAN     IsRenderActive;        // Apakah ada aplikasi yang sedang play audio
    BOOLEAN     IsCaptureActive;       // Apakah ada aplikasi yang sedang record/listen
    BOOLEAN     IsEnabled;             // Status aktif kabel virtual (TRUE = aktif, FALSE = disable)

    // Diagnostik format stream yang BENAR-BENAR dinegosiasikan (bukan format endpoint).
    // Kalau pasangan ini tidak sama, loopback byte-based akan drift dan suara rusak.
    ULONG       RenderBlockAlign;
    ULONG       RenderBytesPerSec;
    ULONG       CaptureBlockAlign;
    ULONG       CaptureBytesPerSec;

    // Rincian pembuangan data: trim latensi vs ring penuh
    ULONG       TrimCount;             // berapa kali backlog dipotong karena batas latensi
    ULONG       TrimBytes;             // total byte dibuang oleh trim
    ULONG       FullDropBytes;         // total byte dibuang karena ring penuh
} MCVAD_CABLE_INFO, *PMCVAD_CABLE_INFO;

typedef struct _MCVAD_ALL_STATUS {
    ULONG               Version;       // Versi driver
    ULONG               CableCount;    // Jumlah kabel (5)
    MCVAD_CABLE_INFO    Cables[5];     // Status masing-masing kabel
} MCVAD_ALL_STATUS, *PMCVAD_ALL_STATUS;

typedef struct _MCVAD_SET_CONFIG_REQ {
    ULONG       CableIndex;
    ULONG       SampleRate;            // Nilai baru (0 = tidak diubah)
    ULONG       BufferDurationMs;      // Nilai baru (0 = tidak diubah)
    LONG        EnableState;           // -1 = tidak diubah, 0 = Disable, 1 = Enable
} MCVAD_SET_CONFIG_REQ, *PMCVAD_SET_CONFIG_REQ;
#pragma pack(pop)
