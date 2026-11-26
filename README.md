# DF-OpenVCable (DimzFrantic Open Virtual Audio Cable)

**DF-OpenVCable** adalah driver audio virtual kernel-mode tingkat lanjut (WDM/WaveRT) untuk sistem operasi Windows 10 dan Windows 11 (64-bit).

Proyek ini menghadirkan arsitektur **All-in-One Single Driver**: satu file binary kernel (`mcvad.sys`) dan satu file instalasi (`.inf`) yang langsung menyediakan **5 pasang Virtual Audio Cable independen** secara simultan tanpa memecah-mecah driver menjadi beberapa paket terpisah.

## 🎛️ Daftar Endpoint Audio (5 Pasang Kabel Independen)
1. **DF-OpenVCable A**: Render & Capture
2. **DF-OpenVCable B**: Render & Capture
3. **DF-OpenVCable C**: Render & Capture
4. **DF-OpenVCable D**: Render & Capture
5. **DF-OpenVCable E**: Render & Capture

Setiap sinyal audio yang diputar ke perangkat Playback/Render secara otomatis dialirkan secara *loss-free* dan *zero-latency* ke perangkat Recording/Capture pada kabel yang bersangkutan melalui memori kernel non-paged pool.

---

## Fitur Utama
- **WaveRT Modern Architecture**: Menggunakan interface `IMiniportWaveRT` standar Windows 10/11 untuk latensi ultra rendah dan efisiensi CPU.
- **Zero Clock Drift Streaming**: Sinkronisasi transfer data menggunakan `KeQueryPerformanceCounter` (QPC) presisi tinggi untuk mencegah audio glitch, stutter, dan dropouts.
- **Dynamic Buffer Resizing**: Konfigurasi ukuran buffer (50 ms hingga 2000 ms) dan sample rate (44.1 kHz hingga 192 kHz) dapat diubah langsung saat runtime via IOCTL.
- **One-Click Setup & Installer GUI**: Dilengkapi installer modern WinForms x64 (`DF-OpenVCable_Setup.exe`) yang mendeteksi status Secure Boot, mengaktifkan Test Signing, mendaftarkan PnP node, memasang driver, menyalin Control Panel ke `Program Files`, serta membuat shortcut Start Menu dan Desktop secara otomatis.
- **Control Panel GUI**: Tersedia aplikasi pengatur berbasis Native Win32 C++ (`control_panel.exe`) dan Python (Tkinter) dengan meter beban buffer dan penghitung glitch real-time.
- **Watermark Remover Tool**: Utilitas pembersih watermark "Test Mode" Windows terpisah di folder `Tools/` tanpa memicu false-positive antivirus.
- **Automated CI/CD**: Dilengkapi workflow GitHub Actions untuk build kernel driver, installer GUI, watermark patcher, dan packaging otomatis.

---

## Panduan Instalasi & Penggunaan

### Unduh Paket Siap Pakai (tanpa login)
Build CI terbaru selalu tersedia di halaman rilis:
[`DF-OpenVCable-v1.0.0-x64.zip`](https://github.com/dimzfrantic/DF-OpenVCable/releases/download/latest-build/DF-OpenVCable-v1.0.0-x64.zip)

**Isi Paket Rilis:**
* `DF-OpenVCable_Setup.exe` : Installer utama ber-GUI (Run as Administrator).
* `control_panel.exe` : Aplikasi Native Control Panel untuk memantau 5 kabel audio.
* `Driver/` : Binary kernel (`mcvad.sys`), catalog (`mcvad.cat`), INF (`mcvad.inf`), sertifikat (`MCVAD_TestCert.cer`), dan skrip PnP (`create_device.ps1`, `install.bat`, `uninstall.bat`).
* `Tools/` : Utilitas `Remove-Watermark.exe` untuk membersihkan watermark Test Mode.
* `tests/` & `docs/` : Skrip validasi loopback audio dan dokumentasi teknis.

### Syarat Sistem:
1. **Sistem Operasi**: Windows 10 atau Windows 11 (64-bit / x64).
2. **Hak Akses**: Administrator.
3. **Secure Boot**: **WAJIB DINONAKTIFKAN (Disabled)** di BIOS/UEFI.
   > *Catatan*: Kernel Windows memblokir pemuatan driver uji coba (test-signed) jika Secure Boot aktif. Installer akan memperingatkan status ini secara otomatis.

---

### Metode 1: Instalasi Cepat via GUI Installer (Sangat Direkomendasikan)

1. Unduh file `DF-OpenVCable-v1.0.0-x64.zip` dari GitHub Releases dan **ekstrak seluruh isinya** ke satu folder lokal (misal: `C:\DF-OpenVCable`).
2. Klik kanan pada **`DF-OpenVCable_Setup.exe`** -> pilih **Run as administrator**.
3. Periksa status sistem pada panel installer:
   - Jika **Secure Boot: [AKTIF]** (Merah): Matikan Secure Boot terlebih dahulu melalui pengaturan BIOS/UEFI komputer Anda.
   - Jika **Secure Boot: [NONAKTIF]** (Hijau): Sistem siap menjalankan driver.
4. Klik tombol **Pasang DF-OpenVCable**.
   Installer akan otomatis menjalankan 5 tahapan:
   - `[1/5]` Mengimpor sertifikat uji coba (`MCVAD_TestCert.cer`) ke store `Trusted Root` dan `TrustedPublisher`.
   - `[2/5]` Mengaktifkan Windows Test Signing (`bcdedit /set testsigning on`).
   - `[3/5]` Mendaftarkan node hardware audio virtual (`ROOT\MCVAD`).
   - `[4/5]` Memasang driver ke Driver Store via `pnputil.exe`.
   - `[5/5]` Menyalin `control_panel.exe` ke `C:\Program Files\DF-OpenVCable\` serta membuat shortcut di Desktop dan Start Menu.
5. **Restart Komputer**:
   Jika Windows Test Signing baru pertama kali diaktifkan, installer akan meminta konfirmasi restart. Klik **Yes** untuk me-restart komputer agar kernel Windows memuat driver.

---

### Metode 2: Instalasi Manual via Skrip (CLI)

Bagi pengguna teknis atau automasi:
1. Masuk ke subfolder `Driver/`.
2. Klik kanan **`install.bat`** -> pilih **Run as administrator**.
3. Restart komputer jika Test Signing baru diaktifkan.

---

### Menghilangkan Watermark "Test Mode" (Opsional)

Setelah Windows Test Signing aktif, Windows akan memunculkan teks kecil watermark di pojok kanan bawah desktop.
* Buka folder `Tools/`.
* Klik kanan **`Remove-Watermark.exe`** -> pilih **Run as administrator**.
* Klik **Patch Watermark**, lalu restart explorer atau komputer Anda.

---

### Menjalankan Control Panel

Setelah instalasi selesai, Anda dapat membuka Control Panel langsung dari shortcut Desktop/Start Menu (**DF-OpenVCable Control Panel**) atau menjalankan:
* **Versi Native C++**: `C:\Program Files\DF-OpenVCable\control_panel.exe`
* **Versi Python**: `python control_panel/mcvad_control_panel.py`

---

### Verifikasi Instalasi & Uji Audio
Panduan bertahap lengkap (prasyarat, instalasi, uji tone loopback, troubleshooting BSOD):
**`docs/VALIDATION.md`**

```powershell
# Cek status instalasi & prasyarat
powershell -ExecutionPolicy Bypass -File tests\mcvad_selftest.ps1

# Uji apakah audio benar-benar mengalir (tone 1 kHz playback -> recording)
powershell -ExecutionPolicy Bypass -File tests\audio_loopback.ps1 -Test -Pattern "DF-OpenVCable A"
```

---

### Pencabutan / Uninstall Bersih

* **Via GUI**: Buka kembali `DF-OpenVCable_Setup.exe` sebagai Administrator -> Klik tombol **Hapus Driver**.
* **Via CLI**: Masuk ke folder `Driver/` -> Klik kanan `uninstall.bat` -> pilih **Run as administrator**.
Seluruh perangkat virtual, file aplikasi di `Program Files`, shortcut di Desktop/Start Menu, dan driver di Driver Store akan dibersihkan secara tuntas.

---

## 📄 Lisensi
Open-source under MIT License. Dibuat dan dikembangkan oleh **dimzfrantic**.
