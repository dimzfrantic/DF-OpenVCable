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
echo    DF-OpenVCable - ENABLE (perangkat hilang dari Sound)
echo ==========================================================
echo.
echo Pakai ini kalau perangkat DF-OpenVCable tidak muncul di Sound
echo Settings / Device Manager setelah update driver.
echo.
echo Penyebab paling umum: devnode tertinggal dalam status DISABLED
echo (CM_PROB_DISABLED). Devnode seperti itu tidak ditampilkan Windows,
echo jadi perangkat tampak "hilang" padahal masih terpasang.
echo.
echo Skrip ini TIDAK menghapus dan TIDAK membuat ulang devnode.
echo Dia hanya:
echo   1. menyalakan kembali devnode yang disabled
echo   2. memastikan service mcvad berjalan
echo   3. restart Audio Service + AudioEndpointBuilder
echo.
pause
echo.

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0create_device.ps1" -Repair

echo.
echo ==========================================================
echo    VERIFIKASI
echo ==========================================================
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0create_device.ps1" -Verify

echo.
echo Kalau jumlahnya sudah 5 Render + 5 Capture, buka Sound Settings.
echo Kalau perangkat MASIH tidak muncul, jalankan reload_driver.bat (rebuild devnode).
echo.
pause
endlocal
