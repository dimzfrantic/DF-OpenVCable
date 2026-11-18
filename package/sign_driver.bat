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
echo signtool sign /v /s My /n "%CERT_NAME%" "%SYS_FILE%"
exit /b 0

:FOUND_SIGNTOOL
echo [OK] Ditemukan SignTool: "%SIGNTOOL%"

:: Tanda tangani mcvad.sys jika ada
if exist "%SYS_FILE%" (
    echo Menandatangani %SYS_FILE%...
    "%SIGNTOOL%" sign /v /s My /n "%CERT_NAME%" "%SYS_FILE%"
    if !ERRORLEVEL! equ 0 (
        echo [SUKSES] %SYS_FILE% berhasil ditandatangani!
    )
)

:: Tanda tangani mcvad.cat jika ada
if exist "%CAT_FILE%" (
    echo Menandatangani %CAT_FILE%...
    "%SIGNTOOL%" sign /v /s My /n "%CERT_NAME%" "%CAT_FILE%"
    if !ERRORLEVEL! equ 0 (
        echo [SUKSES] %CAT_FILE% berhasil ditandatangani!
    )
)

exit /b 0
