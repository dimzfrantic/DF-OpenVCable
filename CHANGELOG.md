# Changelog: DF-OpenVCable (DimzFrantic Open Virtual Audio Cable)

## [1.0.0] - 2026-11-19
### Added
- **Harness validasi (`tests/`)**:
  - `tests/mcvad_selftest.ps1` — 11 pemeriksaan read-only (Test Signing, Secure Boot, sertifikat, DriverStore, service, devnode, driver running, endpoint, bugcheck 24 jam) dengan exit code = jumlah FAIL dan mode `-Json`.
  - `tests/audio_loopback.ps1` — uji fungsional audio via WinMM P/Invoke: `-List`, `-Probe`, `-Test`. Memainkan tone 1 kHz ke endpoint playback lalu merekam dari endpoint recording kabel yang sama, menghitung RMS dan magnitudo Goertzel.
  - `docs/VALIDATION.md` — prosedur 12 bagian dengan bukti yang diharapkan per langkah dan tabel interpretasi ProblemCode.
- **CI: lint PowerShell** — setiap `.ps1` di `tests/` dan `package/` di-parse dengan `System.Management.Automation.Language.Parser`; syntax error menggagalkan build.
- **CI/CD Pipeline Hijau (GitHub Actions `windows-2022`)**: Build driver kernel `mcvad.sys` (x64 Release) berhasil 0 error, dilanjutkan pembuatan katalog `mcvad.cat` via Inf2Cat (signability test: 0 error, 0 warning), test-signing `mcvad.sys` + `mcvad.cat` via signtool SHA256, kompilasi `control_panel.exe`, dan packaging zip.
- **Rolling GitHub Release**: Setiap push ke `main` otomatis mempublikasikan ulang rilis `latest-build` sehingga paket dapat diunduh tanpa login:
  `https://github.com/dimzfrantic/DF-OpenVCable/releases/download/latest-build/DF-OpenVCable-v1.0.0-x64.zip`
- **Fail-fast Packaging**: Step packaging memverifikasi keberadaan `mcvad.sys`, `mcvad.inf`, `mcvad.cat`, `control_panel.exe`, dan `MCVAD_TestCert.cer` sebelum zip dibuat.

### Fixed
- **LNK2001 unresolved external `CLSID_PortWaveRT` / `IID_IUnknown` / `IID_IMiniport*`**: `DEFINE_GUID` pada header PortCls hanya mendeklarasikan simbol `extern`. Diperbaiki dengan `#define PUT_GUIDS_HERE` + `#include <initguid.h>` sebelum `driver.h` pada `src/adapter.cpp` sehingga GUID COM terinstansiasi di binary driver.
- **Entry point Control Panel**: `WinMain` (ANSI) tidak cocok dengan build Unicode (`/DUNICODE`). Diubah menjadi `wWinMain` (`PWSTR`).
- **Step build Control Panel tidak deterministik**: Pemanggilan `build.bat` via `cmd` diganti pemanggilan langsung `cl.exe` dari PowerShell dengan pemeriksaan `$LASTEXITCODE`.
- **`mcvad.cat` tidak pernah terbentuk**: `Inf2Cat.exe` hanya disediakan dalam varian **x86** (`Windows Kits\10\bin\<ver>\x86\Inf2Cat.exe`), sedangkan pencarian sebelumnya difilter ke folder `x64`. Pencarian tool kini dilakukan rekursif lintas versi kit tanpa filter arsitektur.

### Changed
- **Migrasi alokasi memori kernel** dari `ExAllocatePoolWithTag(NonPagedPoolNx, ...)` ke `ExAllocatePool2(POOL_FLAG_NON_PAGED, ...)` pada `src/ringbuffer.cpp` dan `src/minwavert.cpp` (menghilangkan warning C4996 deprecated).
- **Rebranding artefak rilis**: nama paket dan artifact CI menjadi `DF-OpenVCable-v1.0.0-x64` (sebelumnya `MCVAD-v1.2.0`).

## [1.2.0] - 2026-11-18
### Added
- **WaveRT Pin & Filter Topologies** (`src/minwavert.cpp`):
  - Implementasi `PCPIN_DESCRIPTOR` untuk Pin 0 (Render Sink) dan Pin 1 (Capture Source).
  - Dukungan `KSDATARANGE_AUDIO` multi-format: 44.1 kHz s/d 192 kHz, 16 s/d 32-bit container, stereo.
  - `CMiniportWaveRT::GetDescription` sekarang menyediakan `PCFILTER_DESCRIPTOR` lengkap dengan kategori `KSCATEGORY_AUDIO`, `KSCATEGORY_RENDER`, `KSCATEGORY_CAPTURE`, dan `KSCATEGORY_REALTIME`.
  - `CMiniportWaveRT::DataRangeIntersection` untuk negosiasi format audio engine otomatis.
### Fixed & Improved
- **Anti-Glitch & Zero-Clock-Drift Streaming Engine** (`src/minwavert.cpp`):
  - Mengganti transfer chunk statis di `GetPosition` dengan streaming berbasis **delta bytes real-time** yang disinkronkan langsung via `KeQueryPerformanceCounter` (QPC).
  - Menangani wrapping boundary pada DMA cyclic buffer secara presisi agar tidak ada sample terpotong atau terduplikasi.
  - Menghitung `bytesPerSec` dinamis langsung dari `WaveFormatExt.Format.nAvgBytesPerSec`.

## [1.1.0] - 2026-11-18
### Added
- **IOCTL Communication Channel** (`src/mcvad_ioctl.h`, `src/adapter.cpp`):
  - Endpoint IOCTL `\Device\MCVAD_Control` dan symlink `\DosDevices\MCVAD_Control`.
  - `IOCTL_MCVAD_GET_CABLE_STATUS`: Polling status real-time 5 kabel.
  - `IOCTL_MCVAD_SET_CABLE_CONFIG`: Mengubah sample rate & durasi buffer secara dinamis.
  - `IOCTL_MCVAD_RESET_CABLE_STATS`: Mereset statistik glitch.
- **Dynamic Ring Buffer Resizing & Glitch Monitoring** (`src/ringbuffer.h`, `src/ringbuffer.cpp`):
  - Method `Resize(newSizeBytes)` dengan proteksi spinlock non-paged pool.
  - Counter `OverrunCount` dan `UnderrunCount`.
- **Control Panel GUI Helper** (`control_panel/`):
  - `control_panel/mcvad_control_panel.py`: GUI portable Tkinter dengan visual progress bar, tab 5 kabel, dan pilihan default (48kHz / 500ms).
  - `control_panel/control_panel.cpp`: Win32 native GUI C++ mandiri.
  - `control_panel/build.bat`: Helper kompilasi otomatis (MSVC / MinGW / Clang).
- **Automated Deployment & Signing Scripts** (`package/`):
  - `package/setup_cert.bat`: Otomasi pembuatan sertifikat test signing lokal dan registrasi ke Root & TrustedPublisher stores.
  - `package/sign_driver.bat`: Helper penandatanganan `mcvad.sys` dan `mcvad.cat`.
  - `package/create_device.ps1`: Enumerator node virtual PnP `ROOT\MCVAD` langsung via SetupAPI tanpa butuh devcon external.
  - `package/install.bat`: One-click installer untuk mengaktifkan test signing, mendaftarkan device node, dan install driver ke Windows.
  - `package/uninstall.bat`: One-click uninstaller bersih untuk mencabut device node dan driver store.

## [1.0.0-draft] - 2026-11-18
### Added
- **Single Driver Architecture**: Menggabungkan 5 pasang virtual audio endpoint (Virtual Cable 1/Main, A, B, C, D) ke dalam satu adapter driver kernel.
- **Circular Ring Buffer Engine** (`src/ringbuffer.h`, `src/ringbuffer.cpp`):
  - Alokasi memori non-paged (`NonPagedPoolNx`).
  - Proteksi spinlock thread-safe antar-core dan IRQL.
  - Mekanisme penanganan overflow (FIFO drop) dan underflow (digital silence padding).
- **WaveRT Miniport & Stream** (`src/minwavert.h`, `src/minwavert.cpp`):
  - Implementasi `IMiniportWaveRT` dan `IMiniportWaveRTStream`.
  - Direct DMA user-mode mapping via `IoAllocateMdl` dan `MmBuildMdlForNonPagedPool`.
  - Simulasi linear clock dan timestamp query via `KeQueryPerformanceCounter`.
- **Adapter & PnP Lifecycle** (`src/adapter.cpp`):
  - Registrasi driver entry via `PcInitializeAdapterDriver` dan `PcAddAdapterDevice`.
  - Instansiasi 5 pasang subdevice terpisah (`Cable1`, `CableA`, `CableB`, `CableC`, `CableD`) via `PcRegisterSubdevice`.
- **Packaging & INF** (`package/mcvad.inf`, `package/mcvad.vcxproj`):
  - File instalasi INF modern Windows 10/11 (Class MEDIA, ClassGUID `{4d36e96c-e325-11ce-bfc1-08002be10318}`).
  - Registrasi 5 interface KSCATEGORY (RENDER, CAPTURE, AUDIO, REALTIME).
  - MSBuild project file siap kompilasi WDK.
