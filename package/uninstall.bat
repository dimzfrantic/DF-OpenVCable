@echo off
setlocal enabledelayedexpansion

echo ============================================================
echo   Multi-Cable Virtual Audio Driver (MCVAD) - UNINSTALLER
echo ============================================================
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

:: 2. Hapus Device Node ROOT\MCVAD dari Device Manager
echo [LANGKAH 1/2] Menghapus Device Node dari Device Manager...
powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%create_device.ps1" -Remove

:: 3. Cari dan hapus published INF driver oem*.inf dari Driver Store
echo.
echo [LANGKAH 2/2] Mencabut driver MCVAD dari Windows Driver Store...
set FOUND_OEM=
for /f "tokens=1,2 delims=:" %%a in ('pnputil.exe /enum-drivers') do (
    echo %%a | findstr /i "Published Name" >nul
    if !ERRORLEVEL! equ 0 (
        set CUR_OEM=%%b
        set CUR_OEM=!CUR_OEM: =!
    )
    echo %%a | findstr /i "Original Name" >nul
    if !ERRORLEVEL! equ 0 (
        echo %%b | findstr /i "mcvad.inf" >nul
        if !ERRORLEVEL! equ 0 (
            echo Menemukan driver terpasang: !CUR_OEM!
            pnputil.exe /delete-driver !CUR_OEM! /uninstall /force
            set FOUND_OEM=1
        )
    )
)

if not defined FOUND_OEM (
    echo [INFO] Tidak ada driver mcvad.inf yang aktif di Driver Store.
)

echo.
echo ============================================================
echo [SELESAI] Driver MCVAD telah dicabut dari sistem.
echo ============================================================

pause
exit /b 0
