# Architecture & Technical Design: MCVAD

## 1. Topologi Subdevice & Filter

Windows Audio Subsystem (PortCls) mengizinkan satu adapter mendaftarkan banyak subdevice:

```
                            ┌─── [ MCVAD Adapter Driver (.sys) ] ───┐
                            │                                       │
[ App 1 Player ]  ──► [ Cable 1 In  (Render)  ] ──► (FIFO Buffer 0) ──► [ Cable 1 Out (Capture) ] ──► [ App 1 Rec ]
[ App 2 Player ]  ──► [ Cable A In  (Render)  ] ──► (FIFO Buffer 1) ──► [ Cable A Out (Capture) ] ──► [ App 2 Rec ]
[ App 3 Player ]  ──► [ Cable B In  (Render)  ] ──► (FIFO Buffer 2) ──► [ Cable B Out (Capture) ] ──► [ App 3 Rec ]
[ App 4 Player ]  ──► [ Cable C In  (Render)  ] ──► (FIFO Buffer 3) ──► [ Cable C Out (Capture) ] ──► [ App 4 Rec ]
[ App 5 Player ]  ──► [ Cable D In  (Render)  ] ──► (FIFO Buffer 4) ──► [ Cable D Out (Capture) ] ──► [ App 5 Rec ]
                            └───────────────────────────────────────┘
```

## 2. Komponen Inti Driver

1. **Adapter / Driver Entry (`adapter.cpp`)**:
   - Mendaftarkan WDF / PortCls adapter driver (`PcInitializeAdapterDriver`, `PcAddAdapterDevice`).
   - Menginisialisasi array konteks virtual cable (`CABLE_CONTEXT g_Cables[MAX_CABLES]`).

2. **Miniport WaveRT (`minwavert.cpp`)**:
   - Mengimplementasikan `IMiniportWaveRT` dan `IMiniportWaveRTStream`.
   - Menangani alokasi buffer DMA virtual (`AllocatePagesForMdl`, `MapAllocatedPages`).
   - Memberikan sinyal posisi stream waktu nyata ke OS (`GetPosition`, `GetClockRegister`).

3. **Loopback Ring Buffer (`ringbuffer.cpp`)**:
   - Mengelola memori antrean sirkular di `NonPagedPoolNx`.
   - Sinkronisasi atomic lock-free / spinlock ringan untuk menjamin pembacaan data tanpa glitch antar thread render dan capture.
   - Kompensasi clock drift jika rate konsumsi capture berbeda tipis dengan produksi render.

4. **INF & PnP Configuration (`mcvad.inf`)**:
   - Mendaftarkan 5 subdevice Interface GUID.
   - Menyediakan FriendlyName di Sound Control Panel Windows.

## 3. Komparasi dengan VB-CABLE Asli
| Parameter | VB-Audio Cable | MCVAD (Replika Kita) |
|---|---|---|
| Jumlah File Driver | 3 paket driver terpisah (CABLE, A+B, C+D) | 1 driver terpadu (`.sys` tunggal) |
| Proses Instalasi | 3x install & 3x reboot | 1x install tunggal |
| Endpoint Aktif | 1 s/d 5 kabel (tergantung paket yang dibeli) | 5 kabel langsung aktif |
| Basis Miniport | PortCls WaveRT / WaveCyclic hybrid | Murni WaveRT Modern |