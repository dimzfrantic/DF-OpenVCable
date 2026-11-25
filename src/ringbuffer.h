#pragma once

#include "driver.h"

class CCircularRingBuffer {
private:
    PUCHAR          m_pBuffer;
    ULONG           m_BufferSize;      // Total ukuran buffer dalam byte
    ULONG           m_WriteOffset;     // Posisi tulis render
    ULONG           m_ReadOffset;      // Posisi baca capture
    ULONG           m_DataAvailable;   // Byte PCM yang siap dibaca
    KSPIN_LOCK      m_Lock;            // Spinlock sinkronisasi antar IRQL / core
    LARGE_INTEGER   m_LastWriteTime;
    LARGE_INTEGER   m_PerfFrequency;

    // Statistik & Monitoring Glitch
    ULONG           m_OverrunCount;    // Counter buffer overflow
    ULONG           m_UnderrunCount;   // Counter buffer underflow

    // Penjaga latensi: backlog di atas batas ini dibuang (sample tertua) sebelum
    // dibaca, supaya latensi tidak tumbuh tanpa batas kalau kedua stream sempat
    // tidak sinkron (mis. capture baru start setelah render jalan beberapa detik).
    ULONG           m_LatencyLimitBytes;
    ULONG           m_AlignBytes;      // nBlockAlign, untuk menjaga frame alignment saat trim

public:
    CCircularRingBuffer();
    ~CCircularRingBuffer();

    NTSTATUS Initialize(ULONG bufferSizeBytes);
    VOID     Destroy();
    VOID     Reset();
    NTSTATUS Resize(ULONG newBufferSizeBytes);

    VOID     SetAlignment(ULONG blockAlign)   { m_AlignBytes = blockAlign ? blockAlign : 1; }
    VOID     SetLatencyLimit(ULONG maxBytes)  { m_LatencyLimitBytes = maxBytes; }
    ULONG    GetLatencyLimit() const          { return m_LatencyLimitBytes; }

    // Dipanggil oleh stream Render (Playback) untuk mengisi buffer audio
    ULONG    Write(PUCHAR pSource, ULONG byteCount);

    // Dipanggil oleh stream Capture (Recording) untuk mengambil audio loopback
    ULONG    Read(PUCHAR pDestination, ULONG byteCount);

    ULONG    GetDataAvailable();
    ULONG    GetBufferSize() const { return m_BufferSize; }
    ULONG    GetOverrunCount() const { return m_OverrunCount; }
    ULONG    GetUnderrunCount() const { return m_UnderrunCount; }
    VOID     ResetStats();
};
