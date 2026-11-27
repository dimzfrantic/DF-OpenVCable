@echo off
setlocal
cd /d "%~dp0"

net session >nul 2>&1
if %ERRORLEVEL% neq 0 goto elevate

echo ==========================================================
echo    DF-OpenVCable - INSTALL ENDPOINT SELF-HEAL
echo ==========================================================
echo.
echo Mendaftarkan scheduled task "DF-OpenVCable Endpoint Heal":
echo   - trigger : At startup
echo   - akun    : SYSTEM
echo   - tugas   : kalau endpoint DF-OpenVCable tidak terbentuk
echo               setelah boot, devnode di-restart otomatis
echo               (meniru disable/enable manual di Device Manager)
echo.
echo Kalau endpoint sudah normal, task tidak melakukan apa pun.
echo Log: C:\ProgramData\DF-OpenVCable\endpoint_heal.log
echo.
pause
echo.

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Repair-DFEndpoints.ps1" -Install
set RC=%ERRORLEVEL%

echo.
if "%RC%"=="0" echo [OK] Self-heal terpasang dan endpoint normal.
if "%RC%"=="2" echo [GAGAL] Butuh hak Administrator.
if "%RC%"=="1" echo [PERIKSA] Task terpasang tetapi endpoint belum lengkap. Lihat log di atas.
echo.
pause
exit /b %RC%

:elevate
echo [INFO] Meminta hak Administrator...
powershell -NoProfile -Command "Start-Process '%~f0' -Verb RunAs"
exit /b 0
