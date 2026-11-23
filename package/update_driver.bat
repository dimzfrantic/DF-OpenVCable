@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

:: ============================================================
::  DF-OpenVCable - UPDATE DRIVER (satu klik)
::  Menjalankan seluruh rantai update dalam satu perintah:
::    1. Impor sertifikat test-signing build ini (Root + TrustedPublisher)
::    2. Tambah/refresh paket driver ke DriverStore
::    3. Rapikan devnode ROOT\MCVAD menjadi 1 instance
::    4. Verifikasi signature & status devnode
::    5. Tawarkan restart
:: ============================================================

net session >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [INFO] Meminta hak Administrator...
    powershell -NoProfile -Command "Start-Process cmd.exe -ArgumentList '/k `"%~f0`"' -Verb RunAs"
    exit /b 0
)

echo ==========================================================
echo    DF-OpenVCable - UPDATE DRIVER
echo ==========================================================
echo.

if not exist "%~dp0mcvad.inf" (
    echo [ERROR] mcvad.inf tidak ditemukan di folder ini.
    echo         Jalankan update_driver.bat dari folder paket (AIR).
    pause & exit /b 1
)
if not exist "%~dp0mcvad.sys" (
    echo [ERROR] mcvad.sys tidak ditemukan di folder ini.
    pause & exit /b 1
)

echo [1/5] Mengimpor sertifikat test-signing...
certutil -addstore -f Root "%~dp0MCVAD_TestCert.cer" >nul 2>&1
certutil -addstore -f TrustedPublisher "%~dp0MCVAD_TestCert.cer" >nul 2>&1
powershell -NoProfile -Command ^
  "$s=(Get-AuthenticodeSignature '%~dp0mcvad.sys').Status;" ^
  "Write-Host ('      mcvad.sys signature : ' + $s);" ^
  "if ($s -ne 'Valid') { Write-Host '      [WARN] signature belum Valid - driver mungkin ditolak saat load' -ForegroundColor Yellow }"

echo.
echo [2/5] Menambahkan paket driver ke DriverStore...
pnputil /add-driver "%~dp0mcvad.inf" /install
echo       pnputil exit code: %ERRORLEVEL%

echo.
echo [3/5] Merapikan devnode ROOT\MCVAD...
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0create_device.ps1" -Cleanup

echo.
echo [4/5] Verifikasi status...
powershell -NoProfile -Command ^
  "$d=@(Get-PnpDevice -ErrorAction SilentlyContinue | Where-Object { $_.InstanceId -like 'ROOT\MEDIA\*' -and $_.FriendlyName -like '*OpenVCable*' });" ^
  "Write-Host ('      Devnode : ' + $d.Count);" ^
  "foreach ($x in $d) {" ^
  "  $p=Get-PnpDeviceProperty -InstanceId $x.InstanceId -KeyName DEVPKEY_Device_DriverVersion,DEVPKEY_Device_DriverInfPath -ErrorAction SilentlyContinue;" ^
  "  Write-Host ('      ' + $x.InstanceId + '  ' + $x.Status + '  ' + (($p | ForEach-Object { $_.Data }) -join ' / ')) };" ^
  "$svc=Get-Service mcvad -ErrorAction SilentlyContinue;" ^
  "if ($svc) { Write-Host ('      Service : ' + $svc.Status) }"

echo.
echo [5/5] Restart
echo       Binary driver hanya dimuat ulang setelah Windows restart.
echo.
choice /C YN /M "  Restart sekarang"
if errorlevel 2 goto :done
echo.
echo [INFO] Restart dalam 10 detik... (batalkan: shutdown /a)
shutdown /r /t 10
goto :end

:done
echo.
echo [INFO] Restart dibatalkan. Jalankan 'shutdown /r /t 0' saat siap.

:end
echo.
pause
endlocal
