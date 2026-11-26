# Virtual Audio Cable (All-in-One Multi-Endpoint Driver)

## 1. Identitas Proyek
- **Nama Proyek**: Multi-Cable Virtual Audio Driver (MCVAD)
- **Lokasi Direktori**: `E:\Hermes\Projects\VirtualAudioCable`
- **Tujuan**: Membangun kernel audio driver Windows WDM/WaveRT virtual yang menyediakan hingga 5 pasang Virtual Audio Cable independen (A, B, C, D, E) dalam satu paket driver tunggal (`.sys` + `.inf`).
- **Status**: Implementasi Lengkap (Kernel WaveRT, IOCTL Engine, 5-Cable Loopback, Control Panel GUI, Build & Installer Scripts)

## 3. Status Komponen
- **Arsitektur Kernel**: Selesai (`src/adapter.cpp`, `src/minwavert.cpp`, `src/ringbuffer.cpp`, `src/mcvad_ioctl.h`).
- **WaveRT Engine**: Zero-Clock-Drift & QPC Delta streaming dengan boundary wrapping presisi.
- **Control Panel GUI**: Tersedia versi Python (`mcvad_control_panel.py`) dan native C++ Win32 (`control_panel.cpp`).
- **Otomasi Build & Deployment**: Selesai (`package/build_driver.bat`, `package/install.bat`, `package/uninstall.bat`, `package/create_device.ps1`, `package/setup_cert.bat`).
- **Kondisi**: Siap di-build dengan MSBuild/WDK dan dipasang di Windows 10/11.

## 2. Fitur Utama
- **Single Driver Architecture**: Satu file `.sys` mengelola 5 pasangan virtual audio endpoint independen tanpa perlu instalasi berkali-kali.
- **5 Pasang Virtual Endpoint**:
  1. Virtual Cable A (Cable A In / Out)
  2. Virtual Cable B (Cable B In / Out)
  3. Virtual Cable C (Cable C In / Out)
  4. Virtual Cable D (Cable D In / Out)
  5. Virtual Cable E (Cable E In / Out)
- **Kinerja Rendah Latensi**: Berbasis arsitektur PortCls `IMiniportWaveRT` (Wave Real-Time), standar audio Windows modern.
- **Format Audio Terbuka**: PCM 16-bit, 24-bit, 32-bit float; sample rate 44.1 kHz, 48 kHz, 96 kHz, 192 kHz (Stereo hingga 8-channel).
- **Zero External Hardware**: Seluruh routing audio dilakukan melalui kernel non-paged circular ring buffer dengan simulasi clock presisi tinggi.