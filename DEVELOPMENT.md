# Development & Toolchain Guide (DEVELOPMENT.md)

## 1. Kebutuhan Toolchain Build Windows Driver
Untuk mengompilasi driver kernel Windows (`.sys`):
1. **Visual Studio Community/Professional/Enterprise** (2019 atau 2022) dengan workload "Desktop development with C++".
2. **Windows SDK** (versi 10.0.19041.0 atau lebih baru).
3. **Windows Driver Kit (WDK)** yang versinya cocok dengan Windows SDK terpasang.

*Catatan Status Host*: Saat ini pada sistem host belum terdeteksi instalasi MSVC compiler (`cl.exe`) dan WDK di PATH default. Struktur source code dirancang modular sehingga siap dikompilasi begitu WDK/VS tersedia.

## 2. Struktur Direktori Proyek
```
VirtualAudioCable/
├── PROJECT.md             # Gambaran umum & status
├── ARCHITECTURE.md        # Desain arsitektur internal & alur data
├── DEVELOPMENT.md         # Petunjuk toolchain & cara build
├── DECISIONS.md           # Log keputusan teknis
├── src/                   # Source code driver C++ (WaveRT & PortCls)
│   ├── driver.h
│   ├── adapter.cpp
│   ├── minwavert.h
│   ├── minwavert.cpp
│   ├── ringbuffer.h
│   └── ringbuffer.cpp
└── package/               # File instalasi driver
    └── mcvad.inf
```

## 3. Langkah Kompilasi & Instalasi Driver

### A. Kompilasi Driver Otomatis (`build_driver.bat`)
1. Di lingkungan Windows yang memiliki Visual Studio + WDK, jalankan:
   ```cmd
   package\build_driver.bat
   ```
2. Script ini otomatis mendeteksi `MSBuild.exe`, mengompilasi `mcvad.vcxproj` untuk target x64 Release, menyalin `mcvad.sys` ke folder `package/`, dan memanggil `sign_driver.bat` untuk self-signing.

### B. Instalasi Driver Otomatis (`install.bat`)
1. Klik kanan `package/install.bat` -> **Run as Administrator**.
2. Script otomatis mengeksekusi 4 langkah:
   - Membuat sertifikat code-signing lokal 5 tahun dan mendaftarkannya ke Root & TrustedPublisher store (`setup_cert.bat`).
   - Mengaktifkan Windows Test Signing Mode (`bcdedit /set testsigning on`).
   - Mendaftarkan root hardware device node `ROOT\MCVAD` via Windows SetupAPI (`create_device.ps1`).
   - Memasang `mcvad.inf` menggunakan `pnputil.exe` ke Driver Store.

*(Catatan: Jika Windows Test Signing baru pertama kali diaktifkan, restart Windows 1x agar driver test-signed dimuat)*.

### C. Pencabutan / Uninstall Bersih (`uninstall.bat`)
1. Klik kanan `package/uninstall.bat` -> **Run as Administrator**.
2. Script otomatis mencabut node `ROOT\MCVAD` dari Device Manager dan menghapus driver dari Windows Driver Store.