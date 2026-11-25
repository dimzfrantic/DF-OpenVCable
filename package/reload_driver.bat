@echo off
setlocal
cd /d "%~dp0"

net session >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [INFO] Meminta hak Administrator...
    powershell -NoProfile -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b 0
)

echo ==========================================================
echo    DF-OpenVCable - RELOAD DRIVER
echo ==========================================================
echo.
echo Pakai ini kalau endpoint masih memakai nama atau form factor dari
echo build lama, walaupun binary di DriverStore sudah versi terbaru.
echo.
echo Skrip akan:
echo   1. memaksa unload driver mcvad, tunggu sampai image dilepas
echo   2. hapus devnode dan cache endpoint di MMDevices
echo   3. buat ulang devnode supaya binary baru benar-benar dimuat
echo   4. restart Audio Service dan AudioEndpointBuilder
echo.
pause
echo.

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0create_device.ps1" -Cleanup

echo.
echo ==========================================================
echo    VERIFIKASI
echo ==========================================================
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0create_device.ps1" -Verify

echo.
echo Kalau jumlahnya sudah 5 Render + 5 Capture, buka Sound Settings
echo dan pastikan endpoint Playback bernama DF-OpenVCable A sampai E.
echo.
pause
endlocal
