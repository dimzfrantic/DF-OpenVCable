@echo off
setlocal enabledelayedexpansion

echo ==========================================================
echo    Multi-Cable Virtual Audio Driver (MCVAD) - BUILD SCRIPT
echo ==========================================================
echo.

set SCRIPT_DIR=%~dp0
set PROJ_FILE=%SCRIPT_DIR%mcvad.vcxproj
set PLATFORM=x64
set CONFIG=Release

:: 1. Cari vswhere.exe untuk mendeteksi instalasi Visual Studio / WDK
set MSBUILD_EXE=
set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

if exist %VSWHERE% (
    for /f "usebackq tokens=*" %%i in (`%VSWHERE% -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe 2^>nul`) do (
        if exist "%%i" (
            set "MSBUILD_EXE=%%i"
            goto :FOUND_MSBUILD
        )
    )
)

:: 2. Cek apakah MSBuild ada di PATH langsung
where msbuild.exe >nul 2>&1
if %ERRORLEVEL% equ 0 (
    set "MSBUILD_EXE=msbuild.exe"
    goto :FOUND_MSBUILD
)

:: 3. Path default Visual Studio 2022 / 2019 Community, Professional, Enterprise
for %%d in ("C:\Program Files\Microsoft Visual Studio\2022" "C:\Program Files (x86)\Microsoft Visual Studio\2019") do (
    for %%e in (Enterprise Professional Community BuildTools) do (
        if exist "%%~d\%%e\MSBuild\Current\Bin\MSBuild.exe" (
            set "MSBUILD_EXE=%%~d\%%e\MSBuild\Current\Bin\MSBuild.exe"
            goto :FOUND_MSBUILD
        )
    )
)

echo [PERINGATAN] MSBuild.exe belum ditemukan secara otomatis.
echo Pastikan Visual Studio dengan C++ Workload dan Windows Driver Kit (WDK) terpasang.
echo.
echo Jika Anda sudah membuka Developer Command Prompt for VS / WDK:
echo Jalankan perintah berikut secara langsung:
echo   msbuild.exe mcvad.vcxproj /p:Configuration=Release /p:Platform=x64
echo.
pause
exit /b 1

:FOUND_MSBUILD
echo [OK] Menggunakan MSBuild: "!MSBUILD_EXE!"
echo Mengompilasi %PROJ_FILE% (%CONFIG%^|%PLATFORM%)...
echo.

"!MSBUILD_EXE!" "%PROJ_FILE%" /p:Configuration=%CONFIG% /p:Platform=%PLATFORM% /m /v:m
if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] Kompilasi driver gagal. Periksa log error di atas.
    pause
    exit /b 1
)

echo.
echo [SUKSES] Kompilasi berhasil!

:: Salin binary mcvad.sys ke folder package jika output di x64\Release
if exist "%SCRIPT_DIR%x64\Release\mcvad.sys" (
    copy /y "%SCRIPT_DIR%x64\Release\mcvad.sys" "%SCRIPT_DIR%mcvad.sys" >nul
    echo [OK] File mcvad.sys disalin ke folder package.
)

:: 4. Tandatangani driver dengan test cert jika tersedia
if exist "%SCRIPT_DIR%sign_driver.bat" (
    echo.
    echo Menjalankan auto-signing driver...
    call "%SCRIPT_DIR%sign_driver.bat"
)

echo.
echo ==========================================================
echo Driver kernel siap diuji!
echo Jalankan 'install.bat' sebagai Administrator untuk memasang.
echo ==========================================================
pause
exit /b 0
