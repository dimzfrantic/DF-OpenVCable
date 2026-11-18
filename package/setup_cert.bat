@echo off
setlocal enabledelayedexpansion

echo ====================================================
echo    MCVAD Self-Signed Certificate Setup
echo ====================================================
echo.

set CER_FILE=%~dp0MCVAD_TestCert.cer
set CERT_NAME=MCVAD Development Test Certificate

:: Cek hak akses Administrator
net session >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Script ini wajib dijalankan sebagai Administrator!
    echo Silakan klik kanan file ini lalu pilih 'Run as administrator'.
    pause
    exit /b 1
)

echo 1. Membuat sertifikat code signing lokal via PowerShell...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "$cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=%CERT_NAME%' -CertStoreLocation 'Cert:\LocalMachine\My' -NotAfter (Get-Date).AddYears(5); ^
     Export-Certificate -Cert $cert -FilePath '%CER_FILE%' | Out-Null; ^
     Write-Host '[OK] Sertifikat dibuat:' $cert.Thumbprint"

if not exist "%CER_FILE%" (
    echo [ERROR] Gagal membuat file sertifikat %CER_FILE%
    pause
    exit /b 1
)

echo 2. Mengimpor sertifikat ke Trusted Root Certification Authorities...
certutil -addstore -f "Root" "%CER_FILE%" >nul 2>&1
if %ERRORLEVEL% equ 0 (
    echo [OK] Berhasil ditambahkan ke Root Store.
) else (
    echo [PERINGATAN] Gagal menambahkan ke Root Store.
)

echo 3. Mengimpor sertifikat ke Trusted Publishers...
certutil -addstore -f "TrustedPublisher" "%CER_FILE%" >nul 2>&1
if %ERRORLEVEL% equ 0 (
    echo [OK] Berhasil ditambahkan ke TrustedPublisher Store.
) else (
    echo [PERINGATAN] Gagal menambahkan ke TrustedPublisher Store.
)

echo.
echo ====================================================
echo [SUKSES] Sertifikat Test Signing siap digunakan!
echo File: %CER_FILE%
echo ====================================================

exit /b 0
