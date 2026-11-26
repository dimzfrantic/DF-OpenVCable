@echo off
setlocal enabledelayedexpansion

echo ==========================================================
echo    Multi-Cable Virtual Audio Driver (MCVAD) - INSTALLER
echo ==========================================================
echo.

:: 1. Verifikasi hak akses Administrator
net session >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Hak akses Administrator diperlukan!
    echo Klik kanan file ini lalu pilih 'Run as administrator'.
    echo.
    pause
    exit /b 1
)

set SCRIPT_DIR=%~dp0
set INF_PATH=%SCRIPT_DIR%mcvad.inf
set SYS_PATH=%SCRIPT_DIR%mcvad.sys

:: 2. Periksa keberadaan file driver binary
if not exist "%SYS_PATH%" (
    echo [PERINGATAN] File driver binary 'mcvad.sys' belum ditemukan di folder ini.
    echo Kompilasi mcvad.vcxproj dengan WDK terlebih dahulu sebelum instalasi.
    echo.
    set /p LANJUT="Tetap lanjutkan penyiapan sertifikat dan test signing? [Y/N]: "
    if /i not "!LANJUT!"=="Y" exit /b 0
)

:: 3. Siapkan sertifikat Test Signing lokal
echo [LANGKAH 1/4] Menyiapkan sertifikat Test Signing...
call "%SCRIPT_DIR%setup_cert.bat"

:: 4. Aktifkan Windows Test Signing Mode jika belum aktif
echo.
echo [LANGKAH 2/4] Memeriksa status Windows Test Signing...
bcdedit | findstr /i "testsigning.*Yes" >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo Mengaktifkan Windows Testsigning Mode...
    bcdedit /set testsigning on >nul 2>&1
    echo [INFO] Test Signing Mode telah diaktifkan di BCD Windows.
    echo [CATATAN] Jika ini pertama kali diaktifkan, Windows perlu di-restart agar driver test-signed dimuat.
) else (
    echo [OK] Windows Test Signing sudah aktif.
)

:: 5. Buat Root Device Node di Device Manager
echo.
echo [LANGKAH 3/4] Mendaftarkan Root Device Node (ROOT\MCVAD)...
powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%create_device.ps1"

:: 6. Pasang driver ke Driver Store & Device
echo.
echo [LANGKAH 4/4] Memasang INF Driver ke Windows via PnPUtil...
pnputil.exe /add-driver "%INF_PATH%" /install
if %ERRORLEVEL% equ 0 (
    echo.
    echo ==========================================================
echo [SUKSES] MCVAD berhasil dipasang ke sistem!
echo Ke-5 Virtual Audio Cable kini terdaftar di Sound Settings:
echo   - Virtual Cable A
echo   - Virtual Cable B
echo   - Virtual Cable C
echo   - Virtual Cable D
echo   - Virtual Cable E
echo ==========================================================
) else (
    echo.
    echo [INFO] pnputil menambahkan driver dengan kode: %ERRORLEVEL%.
    echo Jika device belum langsung aktif, periksa Device Manager di kategori 'Sound, video and game controllers'.
)

echo.
pause
exit /b 0
