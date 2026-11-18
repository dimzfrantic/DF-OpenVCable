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

public:
    CCircularRingBuffer();
    ~CCircularRingBuffer();

    NTSTATUS Initialize(ULONG bufferSizeBytes);
    VOID     Destroy();
    VOID     Reset();
    NTSTATUS Resize(ULONG newBufferSizeBytes);

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
