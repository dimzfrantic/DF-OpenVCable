# MCVAD - Multi-Cable Virtual Audio Driver

MCVAD adalah driver kernel audio virtual Windows WDM/WaveRT modern standar Windows 10 & 11 yang menggabungkan **5 pasang Virtual Audio Cable independen** ke dalam 1 driver kernel tunggal (`mcvad.sys`).

## Daftar Endpoint Audio (5 Pasang Kabel)
1. **Virtual Cable 1 (Main)**: Render & Capture
2. **Virtual Cable A**: Render & Capture
3. **Virtual Cable B**: Render & Capture
4. **Virtual Cable C**: Render & Capture
5. **Virtual Cable D**: Render & Capture

Setiap sinyal audio yang diputar ke perangkat Playback/Render secara otomatis dialirkan secara *loss-free* dan *zero-latency* ke perangkat Recording/Capture pada kabel yang bersangkutan melalui memori kernel non-paged pool.

---

## Fitur Utama
- **WaveRT Modern Architecture**: Menggunakan interface `IMiniportWaveRT` standar Windows 10/11 untuk latensi ultra rendah dan efisiensi CPU.
- **Zero Clock Drift Streaming**: Sinkronisasi transfer data menggunakan `KeQueryPerformanceCounter` (QPC) presisi tinggi untuk mencegah audio glitch, stutter, dan dropouts.
- **Dynamic Buffer Resizing**: Konfigurasi ukuran buffer (50 ms hingga 2000 ms) dan sample rate (44.1 kHz hingga 192 kHz) dapat diubah langsung saat runtime via IOCTL.
- **Control Panel GUI**: Tersedia aplikasi pengatur berbasis Python (Tkinter) dan Win32 C++ native dengan meter beban buffer dan penghitung glitch real-time.
- **One-Click Deployment**: Installer mandiri (`install.bat`) dan uninstaller (`uninstall.bat`) tanpa dependensi utility eksternal seperti `devcon.exe`.
- **Automated CI/CD**: Dilengkapi workflow GitHub Actions untuk build dan packaging otomatis.

---

## Panduan Instalasi Cepat

### Syarat Sistem:
- Windows 10 atau Windows 11 (64-bit / x64).
- Hak akses Administrator.

### Langkah Pemasangan:
1. Unduh paket rilis atau kompilasi driver.
2. Buka folder `package/`.
3. Klik kanan `install.bat` -> pilih **Run as Administrator**.
4. Skrip akan secara otomatis:
   - Mendaftarkan sertifikat code signing lokal 5 tahun ke Windows Certificate Store.
   - Mengaktifkan mode Windows Test Signing (`bcdedit /set testsigning on`).
   - Mendaftarkan node hardware PnP `ROOT\MCVAD` via Windows SetupAPI.
   - Memasang driver ke Driver Store via `pnputil.exe`.
5. *(Jika Windows Test Signing baru pertama kali diaktifkan, restart komputer 1x agar driver dimuat).*

### Menjalankan Control Panel:
- Versi Python: Jalankan `python control_panel/mcvad_control_panel.py`.
- Versi Native C++: Jalankan `control_panel/control_panel.exe`.

### Pencabutan / Uninstall Bersih:
- Klik kanan `package/uninstall.bat` -> pilih **Run as Administrator**.

---

## Lisensi & Pengembangan
Dikembangkan di bawah naungan proyek internal Hermes Agent.
