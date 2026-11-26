@echo off
setlocal enabledelayedexpansion

echo ===================================================
echo    DF-OpenVCable Setup GUI - Build Script
echo ===================================================
echo.

set SOURCE_DIR=%~dp0
set OUT_EXE=%SOURCE_DIR%DF-OpenVCable_Setup.exe

:: 1. Prioritas MSVC [cl.exe]
where cl.exe >nul 2>&1
if %ERRORLEVEL% equ 0 (
    echo [OK] Mengompilasi installer C++ dengan MSVC...
    cl.exe /nologo /O2 /EHsc /W3 /D_UNICODE /DUNICODE "%SOURCE_DIR%installer.cpp" /Fe:"%OUT_EXE%" /link user32.lib gdi32.lib comctl32.lib shell32.lib advapi32.lib /SUBSYSTEM:WINDOWS
    if not errorlevel 1 (
        echo [SUKSES] Binary dibuat: %OUT_EXE%
        copy /y "%OUT_EXE%" "%SOURCE_DIR%DF-OpenVCable-Setup.exe" >nul 2>&1
        del /q "%SOURCE_DIR%installer.obj" 2>nul
        goto :DONE
    )
)

:: 2. Fallback .NET C# Compiler [csc.exe] bawaan Windows
set CSC=C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe
if exist "%CSC%" (
    echo [OK] Mengompilasi dengan C# Compiler bawaan Windows...
    "%CSC%" /nologo /target:winexe /platform:x64 /optimize+ /out:"%OUT_EXE%" "%SOURCE_DIR%Installer.cs"
    if not errorlevel 1 (
        echo [SUKSES] Binary dibuat: %OUT_EXE%
        copy /y "%OUT_EXE%" "%SOURCE_DIR%DF-OpenVCable-Setup.exe" >nul 2>&1
        goto :DONE
    )
)

echo [ERROR] Gagal mengompilasi installer.
exit /b 1

:DONE
echo Selesai.
exit /b 0
