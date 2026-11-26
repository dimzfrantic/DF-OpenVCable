@echo off
setlocal enabledelayedexpansion

echo ===================================================
echo    DF-OpenVCable Watermark Patcher - Build Script
echo ===================================================
echo.

set SOURCE_DIR=%~dp0
set OUT_EXE=%SOURCE_DIR%Remove-Watermark.exe
set OUT_ALIAS=%SOURCE_DIR%watermark_patcher.exe

:: 1. Prioritas MSVC (cl.exe)
where cl.exe >nul 2>&1
if %ERRORLEVEL% equ 0 (
    echo [OK] Mengompilasi C++ dengan MSVC (cl.exe)...
    cl.exe /nologo /O2 /EHsc /W3 /D_UNICODE /DUNICODE "%SOURCE_DIR%watermark_patcher.cpp" /Fe:"%OUT_EXE%" /link user32.lib gdi32.lib comctl32.lib shell32.lib advapi32.lib /SUBSYSTEM:WINDOWS
    if %ERRORLEVEL% equ 0 (
        echo [SUKSES] Binary dibuat: %OUT_EXE%
        copy /y "%OUT_EXE%" "%OUT_ALIAS%" >nul 2>&1
        del /q "%SOURCE_DIR%watermark_patcher.obj" 2>nul
        goto :DONE
    )
)

:: 2. Fallback .NET C# Compiler (csc.exe) bawaan Windows
set CSC=C:\Windows\Microsoft.NET\Framework644.0.30319\csc.exe
if exist "%CSC%" (
    echo [OK] Mengompilasi dengan C# Compiler bawaan Windows (csc.exe)...
    "%CSC%" /nologo /target:winexe /platform:x64 /optimize+ /out:"%OUT_EXE%" "%SOURCE_DIR%WatermarkPatcher.cs"
    if %ERRORLEVEL% equ 0 (
        echo [SUKSES] Binary dibuat: %OUT_EXE%
        copy /y "%OUT_EXE%" "%OUT_ALIAS%" >nul 2>&1
        goto :DONE
    )
)

echo [ERROR] Gagal mengompilasi patcher.
exit /b 1

:DONE
echo Selesai.
exit /b 0