# Panduan Validasi & Instalasi DF-OpenVCable

Dokumen ini adalah prosedur uji bertahap untuk membuktikan driver `mcvad.sys`
benar-benar berfungsi di Windows 10/11 x64 — bukan hanya berhasil dikompilasi.

Prinsipnya: **satu langkah, satu bukti.** Jangan lanjut ke langkah berikutnya
sebelum langkah saat ini memberi hasil yang diharapkan.

---

## 0. Ringkasan alur

| Tahap | Aksi | Alat ukur | Bukti yang diharapkan |
|---|---|---|---|
| 1 | Cek prasyarat | `tests\mcvad_selftest.ps1` | Semua FAIL terjelaskan |
| 2 | Aktifkan Test Signing | `bcdedit` + restart | Selftest: Test Signing = PASS |
| 3 | Pasang sertifikat uji | `package\setup_cert.bat` | Selftest: Sertifikat = PASS |
| 4 | Pasang driver | `package\install.bat` | Selftest: DriverStore = PASS |
| 5 | Buat device node | `package\create_device.ps1` | Selftest: Devnode = PASS |
| 6 | Verifikasi endpoint | `tests\audio_loopback.ps1 -List` | Muncul endpoint DF-OpenVCable |
| 7 | Uji audio | `tests\audio_loopback.ps1 -Test` | `PASS - audio mengalir` |
| 8 | Uji 5 kabel | ulangi langkah 7 per kabel | 5× PASS |

---

## 1. Persiapan

Ambil paket rilis (sudah berisi binary, INF, katalog, sertifikat, dan skrip):

```
https://github.com/dimzfrantic/DF-OpenVCable/releases/download/latest-build/DF-OpenVCable-v1.0.0-x64.zip
```

Ekstrak ke folder biasa, misal `C:\DF-OpenVCable`. Jangan jalankan langsung dari
dalam file ZIP.

Skrip uji ada di repo (`tests\`). Kalau hanya punya ZIP, salin dua file berikut
ke folder `tests\` di samping folder `package\`:

- `tests\mcvad_selftest.ps1`
- `tests\audio_loopback.ps1`

Jalankan awal untuk melihat kondisi nyata mesin:

```powershell
powershell -ExecutionPolicy Bypass -File tests\mcvad_selftest.ps1
```

Pada mesin yang belum pernah diinstal, hasil normalnya: banyak `FAIL` untuk
Test Signing, Sertifikat, DriverStore, Service, Devnode, dan Driver berjalan.
Itu bukan masalah — itu daftar kerja Anda.

> Catatan: bila di PC sudah terpasang VB-Audio Virtual Cable atau Voicemeeter,
> keduanya bisa hidup berdampingan. Skrip uji sengaja mengecualikan nama
> `VB-Audio` dan `Voicemeeter` agar tidak salah hitung.

---

## 2. Aktifkan Test Signing

Driver ini ditandatangani dengan sertifikat uji (bukan WHQL), jadi Windows
mensyaratkan mode Test Signing.

Buka **PowerShell sebagai Administrator**:

```powershell
bcdedit /set testsigning on
```

Lalu **restart**. Setelah restart, pojok kanan bawah desktop akan menampilkan
tulisan *"Test Mode"* — itu tanda mode sudah aktif.

Verifikasi:

```powershell
powershell -ExecutionPolicy Bypass -File tests\mcvad_selftest.ps1
```

Harus muncul `[ OK ] Windows Test Signing   Aktif`.

**Jika Secure Boot aktif** dan firmware menolak Test Signing, matikan Secure
Boot sementara di BIOS/UEFI. Tanpa ini driver tidak akan bisa dimuat.

---

## 3. Pasang sertifikat uji

Klik kanan `package\setup_cert.bat` → **Run as administrator**.

Skrip memasang `MCVAD_TestCert.cer` ke dua store:

- `LocalMachine\Root` — agar rantai sertifikat dipercaya
- `LocalMachine\TrustedPublisher` — agar Windows mengizinkan pemuatan driver

Verifikasi (tanpa perlu restart):

```powershell
powershell -ExecutionPolicy Bypass -File tests\mcvad_selftest.ps1
```

Harus muncul `[ OK ] Sertifikat test-signing   Terpasang di Root + TrustedPublisher`.

---

## 4. Pasang driver

Klik kanan `package\install.bat` → **Run as administrator**.

Verifikasi:

```powershell
powershell -ExecutionPolicy Bypass -File tests\mcvad_selftest.ps1
```

Harus muncul:

```
[ OK ] Driver di DriverStore    mcvad.inf_amd64_xxxx (sys + cat ada)
[ OK ] Service registry         ImagePath=... Start=3
```

---

## 5. Buat device node

Driver audio virtual tidak punya hardware, jadi node PnP-nya dibuat manual.
Ini dilakukan via SetupAPI (tanpa `devcon.exe`).

Klik kanan `package\create_device.ps1` → **Run as administrator**, atau:

```powershell
powershell -ExecutionPolicy Bypass -File package\create_device.ps1
```

Verifikasi:

```powershell
powershell -ExecutionPolicy Bypass -File tests\mcvad_selftest.ps1
```

Harus muncul:

```
[ OK ] Devnode ROOT\MCVAD        ROOT\MCVAD\0000 status OK
[ OK ] Driver berjalan           State=Running
```

Bila `Status=Error`, cek kode masalahnya:

```powershell
pnputil /enum-devices /problem
```

| ProblemCode | Arti | Tindakan |
|---|---|---|
| 52 | Signature tidak terverifikasi | Test Signing belum aktif, atau sertifikat belum di Root |
| 39 | Driver corrupt / tidak bisa load | Cek `C:\Windows\INF\setupapi.dev.log` bagian akhir |
| 10 | Device tidak bisa start | Driver load tapi inisialisasi gagal — kirim log `DbgView` |
| 28 | Driver tidak terpasang | Ulangi langkah 4 |

---

## 6. Verifikasi endpoint audio

```powershell
powershell -ExecutionPolicy Bypass -File tests\audio_loopback.ps1 -List
```

Cari baris bertanda `<== cocok` pada daftar **PLAYBACK** dan **RECORDING**.

Target: **10 endpoint** — 5 playback + 5 recording (satu pasang per kabel).

> Nama endpoint dipotong Windows di 31 karakter pada enumerasi WinMM. Itu
> normal dan bukan bug driver.

Kalau muncul **10 endpoint**, lanjut ke langkah 7.
Kalau muncul **5 saja**, atau **0**, jangan lanjut — catat jumlahnya dan
laporkan. Ini menandakan pin render/capture belum teregistrasi sebagai dua
endpoint terpisah, dan itu masalah arsitektur yang harus diperbaiki dulu.

---

## 7. Uji audio (bukti utama)

Uji ini memainkan tone 1 kHz ke endpoint playback kabel, merekam dari endpoint
recording kabel yang sama, lalu mengukur level sinyal.

```powershell
# Kabel 1 (Main)
powershell -ExecutionPolicy Bypass -File tests\audio_loopback.ps1 -Test -Pattern "DF-OpenVCable 1"

# Kabel A
powershell -ExecutionPolicy Bypass -File tests\audio_loopback.ps1 -Test -Pattern "DF-OpenVCable A"
```

Hasil PASS terlihat seperti ini:

```
Menguji tone 1000 Hz selama 1500 ms ...

Bytes terekam  : 288000
RMS            : 0.17071  (ambang 0.020)
Magnitudo 1000 Hz: 0.23303

[ OK ] PASS - audio mengalir dari playback ke recording pada kabel yang sama.
```

Cara membaca angkanya:

- **RMS** adalah level sinyal. Tone uji beramplitudo 0.25, sehingga RMS
  teoretisnya 0.25 / √2 ≈ **0.177**. Nilai 0.15–0.18 berarti sinyal utuh.
- **Magnitudo 1 kHz** mendekati 0.25 berarti tone dominan dan tidak terdistorsi
  berat.
- RMS di bawah 0.02 = senyap → routing render→capture belum bekerja.
- RMS ada tapi magnitudo 1 kHz sangat kecil = ada noise, kemungkinan resampling
  atau buffer tidak sinkron.

Exit code: `0` = PASS, `1` = FAIL, `2` = endpoint tidak ditemukan.

---

## 8. Uji kelima kabel

Ulangi langkah 7 untuk `1`, `A`, `B`, `C`, `D`. Syarat lulus: **5 dari 5 PASS**.

Yang perlu diperhatikan: pastikan tone pada kabel A tidak bocor ke kabel B.
Uji isolasi cepat — mainkan tone ke kabel A, lalu probe kabel B:

```powershell
# Terminal 1: mainkan tone ke kabel A selama 5 detik
powershell -ExecutionPolicy Bypass -File tests\audio_loopback.ps1 -Test -Pattern "DF-OpenVCable A" -DurationMs 5000

# Terminal 2 (bersamaan): ukur level di kabel B, harus senyap
powershell -ExecutionPolicy Bypass -File tests\audio_loopback.ps1 -Probe -Pattern "DF-OpenVCable B" -DurationMs 3000
```

`-Probe` pada kabel B harus melaporkan RMS mendekati 0. Kalau tidak, ring buffer
antar kabel tercampur — bug serius pada indeks kabel.

---

## 9. Uji Control Panel

```powershell
control_panel.exe
```

Yang harus terjadi:

- Muncul jendela dengan 5 tab (Cable 1, A, B, C, D).
- Dropdown sample rate default **48000 Hz**.
- Dropdown buffer latency default **500 ms**.
- Klik **Apply Configuration** → bila driver aktif, muncul pesan berhasil;
  bila driver belum jalan, muncul pesan "Mode Desain/Offline".

Verifikasi IOCTL benar-benar sampai ke kernel — ubah buffer ke 100 ms, klik
Apply, lalu cek status lewat selftest: `BufferDurationMs` pada kabel tersebut
harus berubah.

---

## 10. Rollback / uninstall

Klik kanan `package\uninstall.bat` → **Run as administrator**. Lalu:

```powershell
bcdedit /set testsigning off
```

Restart. Sertifikat uji boleh dibiarkan; ia hanya dipercaya untuk sertifikat
dengan subject `MCVAD CI Test Certificate`.

---

## 11. Kalau terjadi BSOD

Ini mungkin terjadi pada driver kernel baru — itu informasi berharga, bukan
akhir segalanya. Yang dibutuhkan untuk diagnosa:

1. **Kode bugcheck** (mis. `0x000000D1 IRQL_NOT_LESS_OR_EQUAL`). Muncul di
   layar biru, atau:
   ```powershell
   Get-WinEvent -FilterHashtable @{LogName='System'; Id=1001} -MaxEvents 3 |
     Format-List TimeCreated, Message
   ```
2. **File dump**: `C:\Windows\Minidump\*.dmp`
3. **Log driver**: jalankan `DbgView` (Sysinternals) sebagai admin, filter
   `MCVAD`, reproduksi, simpan log.

Selftest otomatis menandai bugcheck dalam 24 jam terakhir sebagai `WARN`.

---

## 12. Ringkasan perintah

```powershell
# Status lengkap
powershell -ExecutionPolicy Bypass -File tests\mcvad_selftest.ps1

# Status dalam JSON (untuk otomasi)
powershell -ExecutionPolicy Bypass -File tests\mcvad_selftest.ps1 -Json

# Daftar endpoint audio
powershell -ExecutionPolicy Bypass -File tests\audio_loopback.ps1 -List

# Uji satu kabel
powershell -ExecutionPolicy Bypass -File tests\audio_loopback.ps1 -Test -Pattern "DF-OpenVCable 1"

# Ukur level saja (tanpa playback)
powershell -ExecutionPolicy Bypass -File tests\audio_loopback.ps1 -Probe -Pattern "DF-OpenVCable 1"
```
