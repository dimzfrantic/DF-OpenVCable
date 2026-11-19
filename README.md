# DF-OpenVCable (DimzFrantic Open Virtual Audio Cable)

**DF-OpenVCable** adalah driver audio virtual kernel-mode tingkat lanjut (WDM/WaveRT) untuk sistem operasi Windows 10 dan Windows 11 (64-bit).

Proyek ini menghadirkan arsitektur **All-in-One Single Driver**: satu file binary kernel (`mcvad.sys`) dan satu file instalasi (`.inf`) yang langsung menyediakan **5 pasang Virtual Audio Cable independen** secara simultan tanpa memecah-mecah driver menjadi beberapa paket terpisah.

## 🎛️ Daftar Endpoint Audio (5 Pasang Kabel Independen)
1. **DF-OpenVCable 1 (Main)**: Render & Capture
2. **DF-OpenVCable A**: Render & Capture
3. **DF-OpenVCable B**: Render & Capture
4. **DF-OpenVCable C**: Render & Capture
5. **DF-OpenVCable D**: Render & Capture

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

### Unduh Paket Siap Pakai (tanpa login)
Build CI terbaru selalu tersedia di halaman rilis:
`https://github.com/dimzfrantic/DF-OpenVCable/releases/download/latest-build/DF-OpenVCable-v1.0.0-x64.zip`

Isi paket: `mcvad.sys`, `mcvad.inf`, `mcvad.cat` (test-signed), `MCVAD_TestCert.cer`, `install.bat`, `uninstall.bat`, `setup_cert.bat`, `create_device.ps1`, `control_panel.exe`, `mcvad_control_panel.py`.

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

## 📄 Lisensi
Open-source under MIT License. Dibuat dan dikembangkan oleh **dimzfrantic**.
