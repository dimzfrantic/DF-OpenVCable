# Technical Decisions & Rationale (DECISIONS.md)

## Decision 1: Penyatuan Seluruh Virtual Cable ke dalam 1 Driver Tunggal
- **Tanggal**: 18 November 2026
- **Keputusan**: Menggabungkan kabel default (1/Main), A, B, C, dan D ke dalam satu file binary kernel driver (`.sys`) dan satu file INF.
- **Alasan**: Pemisahan driver oleh vendor komersial seperti VB-Audio murni bermotif skema lisensi/donasi, bukan kendala teknis. Windows PortCls secara native mendukung pendaftaran banyak subdevice dari satu adapter. Pendekatan 1 driver menghemat alokasi memori kernel dan menyederhanakan siklus instalasi/maintenance.
- **Alternatif yang Dipertimbangkan**: Mengikuti cara VB-Audio (membuat 3 file driver terpisah dengan INF berbeda). Ditolak karena redundan dan menambah kompleksitas instalasi.

## Decision 2: Arsitektur Miniport WaveRT (Wave Real-Time)
- **Tanggal**: 18 November 2026
- **Keputusan**: Menggunakan interface `IMiniportWaveRT` daripada `IMiniportWaveCyclic`.
- **Alasan**: WaveRT adalah standar modern Windows untuk latensi rendah (direct user-mode buffer access / hardware audio engine), sedangkan WaveCyclic sudah berstatus legacy/obsolete.
- **Konsekuensi**: Memerlukan clock timer simulasi yang sangat presisi (`KeQueryPerformanceCounter`) untuk memetakan pointer posisi buffer secara akurat.

## Decision 3: Penanganan Driver Signing di Lingkungan Development
- **Tanggal**: 18 November 2026
- **Keputusan**: Menggunakan Test Certificate (Self-Signed) dan mengaktifkan Test Signing Mode (`bcdedit /set testsigning on`) saat pengujian lokal.
- **Alasan**: Windows x64 memblokir pemuatan driver kernel tanpa tanda tangan digital WHQL / EV Certificate berbayar. Mode test signing adalah jalur resmi standar Microsoft untuk rekayasa dan pengembangan driver internal.