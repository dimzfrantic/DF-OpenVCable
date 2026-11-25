#include "ringbuffer.h"

CCircularRingBuffer::CCircularRingBuffer() :
    m_pBuffer(NULL),
    m_BufferSize(0),
    m_WriteOffset(0),
    m_ReadOffset(0),
    m_DataAvailable(0),
    m_OverrunCount(0),
    m_UnderrunCount(0),
    m_TrimCount(0),
    m_TrimBytes(0),
    m_FullDropBytes(0),
    m_LatencyLimitBytes(0),
    m_AlignBytes(1),
    m_PrerollBytes(0)
{
    KeInitializeSpinLock(&m_Lock);
    m_LastWriteTime.QuadPart = 0;
    KeQueryPerformanceCounter(&m_PerfFrequency);
}

CCircularRingBuffer::~CCircularRingBuffer() {
    Destroy();
}

NTSTATUS CCircularRingBuffer::Initialize(ULONG bufferSizeBytes) {
    if (m_pBuffer != NULL) {
        Destroy();
    }

    // Alokasi memori di NonPagedPoolNx (Non-Paged Pool No-Execute) sesuai standar Windows 10/11
    m_pBuffer = (PUCHAR)ExAllocatePool2(POOL_FLAG_NON_PAGED, bufferSizeBytes, MCVAD_POOL_TAG);
    if (!m_pBuffer) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(m_pBuffer, bufferSizeBytes);
    m_BufferSize = bufferSizeBytes;
    m_WriteOffset = 0;
    m_ReadOffset = 0;
    m_DataAvailable = 0;

    return STATUS_SUCCESS;
}

VOID CCircularRingBuffer::Destroy() {
    KIRQL oldIrql;
    KeAcquireSpinLock(&m_Lock, &oldIrql);

    if (m_pBuffer) {
        ExFreePoolWithTag(m_pBuffer, MCVAD_POOL_TAG);
        m_pBuffer = NULL;
    }
    m_BufferSize = 0;
    m_WriteOffset = 0;
    m_ReadOffset = 0;
    m_DataAvailable = 0;

    KeReleaseSpinLock(&m_Lock, oldIrql);
}

VOID CCircularRingBuffer::Reset() {
    KIRQL oldIrql;
    KeAcquireSpinLock(&m_Lock, &oldIrql);

    if (m_pBuffer && m_BufferSize > 0) {
        RtlZeroMemory(m_pBuffer, m_BufferSize);
    }
    m_WriteOffset = 0;
    m_ReadOffset = 0;
    m_DataAvailable = 0;

    KeReleaseSpinLock(&m_Lock, oldIrql);
}

VOID CCircularRingBuffer::ResetStats() {
    KIRQL oldIrql;
    KeAcquireSpinLock(&m_Lock, &oldIrql);
    m_OverrunCount = 0;
    m_UnderrunCount = 0;
    m_TrimCount = 0;
    m_TrimBytes = 0;
    m_FullDropBytes = 0;
    KeReleaseSpinLock(&m_Lock, oldIrql);
}

NTSTATUS CCircularRingBuffer::Resize(ULONG newBufferSizeBytes) {
    if (newBufferSizeBytes == 0) return STATUS_INVALID_PARAMETER;

    PUCHAR pNewBuffer = (PUCHAR)ExAllocatePool2(POOL_FLAG_NON_PAGED, newBufferSizeBytes, MCVAD_POOL_TAG);
    if (!pNewBuffer) return STATUS_INSUFFICIENT_RESOURCES;

    RtlZeroMemory(pNewBuffer, newBufferSizeBytes);

    KIRQL oldIrql;
    KeAcquireSpinLock(&m_Lock, &oldIrql);

    PUCHAR pOldBuffer = m_pBuffer;
    m_pBuffer = pNewBuffer;
    m_BufferSize = newBufferSizeBytes;
    m_WriteOffset = 0;
    m_ReadOffset = 0;
    m_DataAvailable = 0;

    KeReleaseSpinLock(&m_Lock, oldIrql);

    if (pOldBuffer) {
        ExFreePoolWithTag(pOldBuffer, MCVAD_POOL_TAG);
    }

    return STATUS_SUCCESS;
}

ULONG CCircularRingBuffer::Write(PUCHAR pSource, ULONG byteCount) {
    if (!m_pBuffer || byteCount == 0 || !pSource) return 0;

    KIRQL oldIrql;
    KeAcquireSpinLock(&m_Lock, &oldIrql);

    if (byteCount > m_BufferSize) {
        pSource += (byteCount - m_BufferSize);
        byteCount = m_BufferSize;
    }

    ULONG freeSpace = m_BufferSize - m_DataAvailable;
    if (byteCount > freeSpace) {
        // Overflow handling: majukan pointer baca untuk membuang sample tertua (FIFO ring drop)
        ULONG overflowBytes = byteCount - freeSpace;
        m_ReadOffset = (m_ReadOffset + overflowBytes) % m_BufferSize;
        m_DataAvailable -= overflowBytes;
        m_OverrunCount++;
        m_FullDropBytes += overflowBytes;
    }

    // Tulis ke buffer memori sirkular (tangani wrap-around jika mencapai akhir buffer)
    ULONG firstChunk = m_BufferSize - m_WriteOffset;
    if (firstChunk > byteCount) firstChunk = byteCount;
    ULONG secondChunk = byteCount - firstChunk;

    RtlCopyMemory(m_pBuffer + m_WriteOffset, pSource, firstChunk);
    if (secondChunk > 0) {
        RtlCopyMemory(m_pBuffer, pSource + firstChunk, secondChunk);
    }

    m_WriteOffset = (m_WriteOffset + byteCount) % m_BufferSize;
    m_DataAvailable += byteCount;

    // KeQueryPerformanceCounter(&x) mengisi x dengan FREKUENSI, bukan waktu sekarang.
    // Waktu sekarang adalah return value fungsi ini.
    m_LastWriteTime = KeQueryPerformanceCounter(NULL);
    KeReleaseSpinLock(&m_Lock, oldIrql);
    return byteCount;
}

ULONG CCircularRingBuffer::Read(PUCHAR pDestination, ULONG byteCount) {
    if (!pDestination || byteCount == 0) return 0;

    KIRQL oldIrql;
    KeAcquireSpinLock(&m_Lock, &oldIrql);

    if (!m_pBuffer) {
        RtlZeroMemory(pDestination, byteCount);
        KeReleaseSpinLock(&m_Lock, oldIrql);
        return byteCount;
    }

    // Pre-roll: selagi cushion belum terisi, keluarkan senyap dan JANGAN konsumsi
    // data. Ini bukan underrun — ini fase pengisian awal yang disengaja, supaya
    // capture punya jarak aman terhadap jitter scheduling.
    // Cushion dibatasi separuh kapasitas ring: kalau user menyetel buffer sangat
    // kecil, preroll tidak boleh melebihi isi ring (kalau tidak, capture akan
    // menunggu selamanya dan yang keluar hanya senyap).
    ULONG preroll = m_PrerollBytes;
    if (preroll > m_BufferSize / 2) preroll = m_BufferSize / 2;
    if (preroll > 0 && m_DataAvailable < preroll) {
        RtlZeroMemory(pDestination, byteCount);
        KeReleaseSpinLock(&m_Lock, oldIrql);
        return byteCount;
    }

    if (m_DataAvailable == 0) {
        // Underflow handling: isi dengan digital silence (0x00)
        RtlZeroMemory(pDestination, byteCount);
        m_UnderrunCount++;
        KeReleaseSpinLock(&m_Lock, oldIrql);
        return byteCount;
    }

    // Penjaga latensi: kalau backlog melebihi batas, buang sample TERTUA supaya
    // latensi kembali terkendali. Tanpa ini, capture yang baru mulai beberapa detik
    // setelah render akan memutar audio basi dengan latensi sebesar isi buffer.
    if (m_LatencyLimitBytes > 0 && m_DataAvailable > m_LatencyLimitBytes) {
        // Buang sampai kembali ke target cushion (bukan ke batas atas), supaya
        // setelah koreksi latensi kembali normal dan tidak menetap di batas.
        ULONG target = (m_PrerollBytes > 0 && m_PrerollBytes < m_LatencyLimitBytes)
                       ? m_PrerollBytes : m_LatencyLimitBytes;
        ULONG drop = m_DataAvailable - target;
        drop = (drop / m_AlignBytes) * m_AlignBytes;   // jaga batas frame PCM
        if (drop > 0) {
            m_ReadOffset = (m_ReadOffset + drop) % m_BufferSize;
            m_DataAvailable -= drop;
            m_OverrunCount++;
            m_TrimCount++;
            m_TrimBytes += drop;
        }
    }

    ULONG toRead = byteCount;
    ULONG paddingZeroes = 0;

    if (toRead > m_DataAvailable) {
        paddingZeroes = toRead - m_DataAvailable;
        toRead = m_DataAvailable;
        m_UnderrunCount++;
    }

    ULONG firstChunk = m_BufferSize - m_ReadOffset;
    if (firstChunk > toRead) firstChunk = toRead;
    ULONG secondChunk = toRead - firstChunk;

    RtlCopyMemory(pDestination, m_pBuffer + m_ReadOffset, firstChunk);
    if (secondChunk > 0) {
        RtlCopyMemory(pDestination + firstChunk, m_pBuffer, secondChunk);
    }

    m_ReadOffset = (m_ReadOffset + toRead) % m_BufferSize;
    m_DataAvailable -= toRead;

    if (paddingZeroes > 0) {
        RtlZeroMemory(pDestination + toRead, paddingZeroes);
    }

    KeReleaseSpinLock(&m_Lock, oldIrql);
    return byteCount;
}

ULONG CCircularRingBuffer::GetDataAvailable() {
    KIRQL oldIrql;
    KeAcquireSpinLock(&m_Lock, &oldIrql);
    ULONG avail = m_DataAvailable;
    KeReleaseSpinLock(&m_Lock, oldIrql);
    return avail;
}
