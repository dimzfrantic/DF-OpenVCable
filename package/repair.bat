@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

:: ============================================================
::  DF-OpenVCable - REPAIR / CLEANUP
::  Merapikan devnode duplikat ROOT\MCVAD menjadi 1 instance,
::  lalu menawarkan restart agar binary driver baru dimuat.
:: ============================================================

:: --- Self-elevate ke Administrator ---
net session >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [INFO] Meminta hak Administrator...
    powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b 0
)

echo ==========================================================
echo    DF-OpenVCable - REPAIR / CLEANUP DEVNODE
echo ==========================================================
echo.

if not exist "%~dp0create_device.ps1" (
    echo [ERROR] create_device.ps1 tidak ditemukan di folder ini.
    echo         Pastikan repair.bat dijalankan dari folder paket AIR.
    echo.
    pause
    exit /b 1
)

echo [LANGKAH 1/3] Memeriksa devnode ROOT\MCVAD...
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0create_device.ps1" -Cleanup
set CLEAN_RC=%ERRORLEVEL%
if not "%CLEAN_RC%"=="0" (
    echo [WARN] create_device.ps1 keluar dengan kode %CLEAN_RC%
)

echo.
echo [LANGKAH 2/3] Memverifikasi hasil...
powershell -NoProfile -Command ^
  "$d = @(Get-PnpDevice -ErrorAction SilentlyContinue | Where-Object { $_.InstanceId -like 'ROOT\MEDIA\*' -and $_.FriendlyName -like '*OpenVCable*' });" ^
  "Write-Host ('  Devnode aktif : ' + $d.Count);" ^
  "foreach ($x in $d) { Write-Host ('  ' + $x.InstanceId + '  ' + $x.Status) }"

echo.
echo [LANGKAH 3/3] Restart
echo   Binary driver baru (19.352 bytes, dengan Topology) hanya dimuat
echo   setelah Windows di-restart. Tanpa restart, kode Topology belum jalan.
echo.
choice /C YN /M "  Restart sekarang"
if errorlevel 2 goto :done
echo.
echo [INFO] Restart dalam 10 detik... (batalkan dengan: shutdown /a)
shutdown /r /t 10
goto :end

:done
echo.
echo [INFO] Restart dibatalkan. Jalankan 'shutdown /r /t 0' saat siap.

:end
echo.
pause
endlocal
