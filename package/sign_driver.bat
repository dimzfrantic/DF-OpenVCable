@echo off
setlocal enabledelayedexpansion

echo ====================================================
echo    MCVAD Driver Signing Helper
echo ====================================================
echo.

set PKG_DIR=%~dp0
set CERT_NAME=MCVAD Development Test Certificate
set SYS_FILE=%PKG_DIR%mcvad.sys
set INF_FILE=%PKG_DIR%mcvad.inf
set CAT_FILE=%PKG_DIR%mcvad.cat

:: Cek file driver binary mcvad.sys
if not exist "%SYS_FILE%" (
    echo [PERINGATAN] File %SYS_FILE% belum ditemukan!
    echo Silakan build mcvad.vcxproj terlebih dahulu menggunakan Visual Studio / WDK.
    echo Menunggu pembuatan binary...
)

:: Cari signtool.exe di sistem / Windows Kits
set SIGNTOOL=
for /r "C:\Program Files (x86)\Windows Kits" %%f in (signtool.exe) do (
    if exist "%%f" (
        echo %%f | findstr /i "x64" >nul
        if !ERRORLEVEL! equ 0 (
            set SIGNTOOL=%%f
            goto :FOUND_SIGNTOOL
        )
    )
)
for /r "C:\Program Files\Windows Kits" %%f in (signtool.exe) do (
    if exist "%%f" (
        set SIGNTOOL=%%f
        goto :FOUND_SIGNTOOL
    )
)

where signtool.exe >nul 2>&1
if %ERRORLEVEL% equ 0 (
    set SIGNTOOL=signtool.exe
    goto :FOUND_SIGNTOOL
)

echo [INFO] Signtool.exe belum ditemukan di path standar Windows Kits.
echo Anda dapat menandatangani mcvad.sys secara manual dari Developer Command Prompt:
echo signtool sign /v /fd SHA256 /ph /s My /n "%CERT_NAME%" "%SYS_FILE%"
echo inf2cat /driver:"%PKG_DIR%." /os:10_X64
echo signtool sign /v /fd SHA256 /s My /n "%CERT_NAME%" "%CAT_FILE%"
exit /b 0

:FOUND_SIGNTOOL
echo [OK] Ditemukan SignTool: "%SIGNTOOL%"

:: 1. Tanda tangani mcvad.sys dengan page hashes (/ph).
::    /ph wajib untuk binary kernel-mode; tanpa itu kernel loader bisa menolak
::    memuat driver (Event 3004 "PageHash: false" / 0xC0000428).
if exist "%SYS_FILE%" (
    echo [1/3] Menandatangani %SYS_FILE%...
    "%SIGNTOOL%" sign /v /fd SHA256 /ph /s My /n "%CERT_NAME%" "%SYS_FILE%"
    if !ERRORLEVEL! neq 0 (
        echo [GAGAL] Penandatanganan mcvad.sys gagal.
        exit /b 1
    )
    echo [SUKSES] %SYS_FILE% berhasil ditandatangani.
)

:: 2. Buat ulang katalog SETELAH sys ditandatangani.
::    Inf2Cat menghitung hash file saat katalog dibuat. Kalau sys ditandatangani
::    setelah katalog lama dibuat, hash di mcvad.cat menjadi usang dan Windows
::    menolak driver (0xC0000428).
set INF2CAT=
for /r "C:\Program Files (x86)\Windows Kits" %%f in (inf2cat.exe) do (
    if exist "%%f" (
        set INF2CAT=%%f
        goto :FOUND_INF2CAT
    )
)
for /r "C:\Program Files\Windows Kits" %%f in (inf2cat.exe) do (
    if exist "%%f" (
        set INF2CAT=%%f
        goto :FOUND_INF2CAT
    )
)
where inf2cat.exe >nul 2>&1
if %ERRORLEVEL% equ 0 (
    set INF2CAT=inf2cat.exe
    goto :FOUND_INF2CAT
)

echo [PERINGATAN] Inf2Cat.exe tidak ditemukan - katalog mcvad.cat TIDAK diperbarui.
echo               Jalankan inf2cat /driver:"%PKG_DIR%." /os:10_X64 manual, lalu ulangi langkah 3.
goto :SIGN_CAT

:FOUND_INF2CAT
echo [2/3] Membuat ulang mcvad.cat dari binary yang sudah ditandatangani...
"%INF2CAT%" /driver:"%PKG_DIR%." /os:10_X64
if !ERRORLEVEL! neq 0 (
    echo [PERINGATAN] Inf2Cat gagal. Periksa mcvad.inf; katalog lama tidak bisa dipercaya.
)

:SIGN_CAT
:: 3. Tanda tangani katalog.
if exist "%CAT_FILE%" (
    echo [3/3] Menandatangani %CAT_FILE%...
    "%SIGNTOOL%" sign /v /fd SHA256 /s My /n "%CERT_NAME%" "%CAT_FILE%"
    if !ERRORLEVEL! neq 0 (
        echo [GAGAL] Penandatanganan mcvad.cat gagal.
        exit /b 1
    )
    echo [SUKSES] %CAT_FILE% berhasil ditandatangani.
) else (
    echo [PERINGATAN] %CAT_FILE% tidak ada - katalog belum dibuat.
)

echo.
echo Verifikasi cepat:
"%SIGNTOOL%" verify /v /pa "%SYS_FILE%"
if exist "%CAT_FILE%" "%SIGNTOOL%" verify /v /pa "%CAT_FILE%"

exit /b 0
