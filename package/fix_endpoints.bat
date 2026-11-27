@echo off
setlocal
cd /d "%~dp0"

net session >nul 2>&1
if %ERRORLEVEL% neq 0 goto elevate

echo ==========================================================
echo    DF-OpenVCable - FIX ENDPOINTS (sekali jalan)
echo ==========================================================
echo.
echo Dipakai kalau setelah boot driver DF-OpenVCable ada di Device
echo Manager tetapi endpoint-nya tidak muncul di Sound Settings.
echo.
echo Skrip ini hanya me-restart devnode DF-OpenVCable supaya
echo AudioEndpointBuilder membangun ulang endpoint-nya.
echo Tidak menghapus driver, tidak menghapus device.
echo.
pause
echo.

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Repair-DFEndpoints.ps1"
set RC=%ERRORLEVEL%

echo.
echo ==========================================================
echo    VERIFIKASI
echo ==========================================================
if exist "%~dp0create_device.ps1" (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0create_device.ps1" -Verify
)
echo.
if "%RC%"=="0" echo [OK] Endpoint DF-OpenVCable normal. Buka Sound Settings.
if not "%RC%"=="0" echo [GAGAL] Endpoint belum muncul. Jalankan install_endpoint_heal.bat lalu reboot, atau kirim isi C:\ProgramData\DF-OpenVCable\endpoint_heal.log
echo.
echo Supaya tidak terulang setiap boot, jalankan install_endpoint_heal.bat.
echo.
pause
exit /b %RC%

:elevate
echo [INFO] Meminta hak Administrator...
powershell -NoProfile -Command "Start-Process '%~f0' -Verb RunAs"
exit /b 0
