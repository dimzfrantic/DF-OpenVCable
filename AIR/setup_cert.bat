@echo off
setlocal enabledelayedexpansion

echo ====================================================
echo    DF-OpenVCable Test Certificate Setup
echo ====================================================
echo.

set CER_FILE=%~dp0MCVAD_TestCert.cer
set CERT_NAME=MCVAD CI Test Certificate

:: Cek hak akses Administrator
net session >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Script ini wajib dijalankan sebagai Administrator!
    echo Silakan klik kanan file ini lalu pilih 'Run as administrator',
    echo atau jalankan dari PowerShell / CMD sebagai Administrator.
    echo.
    pause
    exit /b 1
)

if exist "%CER_FILE%" goto :CERT_FOUND
echo 1. Membuat sertifikat code signing lokal via PowerShell...
powershell -NoProfile -ExecutionPolicy Bypass -Command "$cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=%CERT_NAME%' -CertStoreLocation 'Cert:\LocalMachine\My' -NotAfter (Get-Date).AddYears(5); Export-Certificate -Cert $cert -FilePath '%CER_FILE%'"
:CERT_FOUND
echo [OK] File sertifikat ditemukan: %CER_FILE%

if not exist "%CER_FILE%" (
    echo [ERROR] Gagal menemukan atau membuat file sertifikat %CER_FILE%
    echo.
    pause
    exit /b 1
)

echo.
echo 2. Mengimpor sertifikat ke Trusted Root Certification Authorities...
certutil -addstore -f "Root" "%CER_FILE%"
if %ERRORLEVEL% equ 0 (
    echo [OK] Berhasil ditambahkan ke Root Store.
) else (
    echo [PERINGATAN] Gagal menambahkan ke Root Store.
)

echo.
echo 3. Mengimpor sertifikat ke Trusted Publishers...
certutil -addstore -f "TrustedPublisher" "%CER_FILE%"
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
echo.
pause
exit /b 0
