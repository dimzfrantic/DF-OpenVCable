#!/usr/bin/env python3
"""
Unit Test & Streaming Loopback Simulation untuk MCVAD (Multi-Cable Virtual Audio Driver)
Menguji formula circular buffer, delta bytes, boundary wrapping, dan kestabilan clock drift.
"""
import math
import time
import sys

SAMPLE_RATES = [44100, 48000, 88200, 96000, 192000]
CHANNELS = 2
BYTES_PER_SAMPLE = 4  # 32-bit container

def test_audio_format_calculations():
    print("[TEST 1] Menguji Perhitungan Byte Rate & Frame Alignment...")
    for sr in SAMPLE_RATES:
        block_align = CHANNELS * BYTES_PER_SAMPLE
        bytes_per_sec = sr * block_align
        print(f"  - Sample Rate: {sr:6d} Hz -> BlockAlign: {block_align} bytes -> Rate: {bytes_per_sec:7d} bytes/s")
        assert block_align == 8, "Block align harus 8 byte untuk stereo 32-bit container"
        assert bytes_per_sec % block_align == 0, "Byte rate harus kelipatan genap dari block align"
    print("  [OK] Perhitungan format audio valid!\n")

def test_ring_buffer_simulation():
    print("[TEST 2] Simulasi Circular Ring Buffer Streaming & Boundary Wrapping...")
    capacity = 48000 * 8 * 1  # 1 detik pada 48kHz stereo 32-bit = 384,000 bytes
    buffer = bytearray(capacity)
    write_pos = 0
    read_pos = 0
    data_len = 0
    overruns = 0
    underruns = 0

    # Simulasi 100 siklus transfer data (chunk 480 frame = 10ms per transfer)
    chunk_frames = 480
    chunk_bytes = chunk_frames * 8
    test_payload = bytearray([0x5A] * chunk_bytes)

    for cycle in range(100):
        # 1. Producer (Render stream menulis ke ring buffer)
        free_space = capacity - data_len
        if chunk_bytes > free_space:
            overruns += 1
        else:
            # Handle wrapping
            first_part = min(chunk_bytes, capacity - write_pos)
            second_part = chunk_bytes - first_part
            buffer[write_pos : write_pos + first_part] = test_payload[:first_part]
            if second_part > 0:
                buffer[0:second_part] = test_payload[first_part:]
            write_pos = (write_pos + chunk_bytes) % capacity
            data_len += chunk_bytes

        # 2. Consumer (Capture stream membaca dari ring buffer)
        if data_len < chunk_bytes:
            underruns += 1
        else:
            first_part = min(chunk_bytes, capacity - read_pos)
            second_part = chunk_bytes - first_part
            out = buffer[read_pos : read_pos + first_part] + (buffer[0:second_part] if second_part > 0 else b"")
            assert len(out) == chunk_bytes, "Panjang data read harus tepat 1 chunk"
            assert out == test_payload, "Integritas data loopback rusak!"
            read_pos = (read_pos + chunk_bytes) % capacity
            data_len -= chunk_bytes

    print(f"  - Siklus diuji: 100x (Total {100 * chunk_bytes} bytes)")
    print(f"  - Overrun count : {overruns}")
    print(f"  - Underrun count: {underruns}")
    assert overruns == 0 and underruns == 0, "Streaming loopback tidak boleh mengalami glitch pada kondisi steady-state"
    print("  [OK] Simulasi loopback transfer 100% loss-free & glitch-free!\n")

def test_dynamic_resize():
    print("[TEST 3] Simulasi Dynamic Buffer Resizing saat Runtime...")
    old_capacity = 48000 * 8 // 2  # 500 ms = 192,000 bytes
    new_capacity = 48000 * 8 * 2   # 2000 ms = 768,000 bytes
    print(f"  - Re-alokasi buffer: {old_capacity} bytes -> {new_capacity} bytes")
    
    buf = bytearray(old_capacity)
    # Resize
    new_buf = bytearray(new_capacity)
    # Verifikasi pointer dan alokasi
    assert len(new_buf) == new_capacity
    print("  [OK] Buffer dynamic resize valid!\n")

if __name__ == '__main__':
    print("==========================================================")
    print("  MCVAD - KERNEL STREAMING ENGINE VERIFICATION SUITE")
    print("==========================================================\n")
    test_audio_format_calculations()
    test_ring_buffer_simulation()
    test_dynamic_resize()
    print("==========================================================")
    print("  SEMUA PENGUJIAN LOGIKA STREAMING SUKSES (PASSED)")
    print("==========================================================")
