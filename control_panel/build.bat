@echo off
setlocal enabledelayedexpansion

echo ===================================================
echo    MCVAD Native Control Panel - Build Script
echo ===================================================
echo.

set SOURCE_DIR=%~dp0
set SRC_FILE=%SOURCE_DIR%control_panel.cpp
set OUT_EXE=%SOURCE_DIR%control_panel.exe

:: 1. Cek apakah MSVC (cl.exe) tersedia
where cl.exe >nul 2>&1
if %ERRORLEVEL% equ 0 (
    echo [OK] Ditemukan compiler MSVC (cl.exe).
    echo Mengompilasi control_panel.cpp...
    cl.exe /nologo /O2 /EHsc /W3 /D_UNICODE /DUNICODE "%SRC_FILE%" /Fe:"%OUT_EXE%" /link user32.lib gdi32.lib comctl32.lib /SUBSYSTEM:WINDOWS
    if %ERRORLEVEL% equ 0 (
        echo.
        echo [SUKSES] Binary berhasil dibuat: %OUT_EXE%
        del /q "%SOURCE_DIR%control_panel.obj" 2>nul
        goto :RUN_PROMPT
    ) else (
        echo [ERROR] Gagal kompilasi dengan MSVC.
        goto :FAILED
    )
)

:: 2. Cek apakah MinGW (g++.exe) tersedia
where g++.exe >nul 2>&1
if %ERRORLEVEL% equ 0 (
    echo [OK] Ditemukan compiler MinGW (g++.exe).
    echo Mengompilasi control_panel.cpp...
    g++.exe -O2 -municode -mwindows "%SRC_FILE%" -o "%OUT_EXE%" -lcomctl32 -lgdi32 -luser32
    if %ERRORLEVEL% equ 0 (
        echo.
        echo [SUKSES] Binary berhasil dibuat: %OUT_EXE%
        goto :RUN_PROMPT
    ) else (
        echo [ERROR] Gagal kompilasi dengan g++.
        goto :FAILED
    )
)

:: 3. Cek apakah Clang (clang++.exe) tersedia
where clang++.exe >nul 2>&1
if %ERRORLEVEL% equ 0 (
    echo [OK] Ditemukan compiler Clang (clang++.exe).
    echo Mengompilasi control_panel.cpp...
    clang++ -O2 -D_UNICODE -DUNICODE -mwindows "%SRC_FILE%" -o "%OUT_EXE%" -lcomctl32 -lgdi32 -luser32
    if %ERRORLEVEL% equ 0 (
        echo.
        echo [SUKSES] Binary berhasil dibuat: %OUT_EXE%
        goto :RUN_PROMPT
    ) else (
        echo [ERROR] Gagal kompilasi dengan Clang.
        goto :FAILED
    )
)

echo [PERINGATAN] Tidak ditemukan compiler C++ (cl.exe, g++.exe, clang++) di PATH aktif.
echo.
echo Solusi cara kompilasi:
echo  1. Buka 'x64 Native Tools Command Prompt for VS' (jika menginstall Visual Studio/WDK)
echo  2. Masuk ke direktori ini:
echo     cd /d "%SOURCE_DIR%"
echo  3. Jalankan script ini kembali: build.bat
echo.
goto :FAILED

:RUN_PROMPT
echo.
if defined CI goto :END
if "%1"=="--ci" goto :END
set /p JALANKAN="Jalankan control_panel.exe sekarang? (Y/N): "
if /i "!JALANKAN!"=="Y" (
    start "" "%OUT_EXE%"
)
goto :END

:FAILED
echo Kompilasi belum dapat diselesaikan.
exit /b 1

:END
exit /b 0
