@echo off
REM ============================================================
REM  build.bat - Configures and builds FFmpegAudioEditor on Windows
REM
REM  Usage:
REM    build.bat "C:\Qt\6.7.0\msvc2019_64"          (Visual Studio / MSVC)
REM    build.bat "C:\Qt\6.7.0\mingw_64" mingw        (MinGW)
REM
REM  If no Qt path is given, the script assumes CMAKE_PREFIX_PATH is
REM  already set in your environment.
REM ============================================================

setlocal enabledelayedexpansion

set "QT_PATH=%~1"
set "TOOLCHAIN=%~2"

if "%QT_PATH%"=="" (
    echo No Qt path supplied. Assuming CMAKE_PREFIX_PATH is already set.
) else (
    echo Using Qt installation: %QT_PATH%
)

set "BUILD_DIR=build"

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo.
echo === Configuring project ===
if /I "%TOOLCHAIN%"=="mingw" (
    cmake -B "%BUILD_DIR%" -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%QT_PATH%"
) else (
    if "%QT_PATH%"=="" (
        cmake -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=Release
    ) else (
        cmake -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%QT_PATH%"
    )
)

if errorlevel 1 (
    echo.
    echo Configuration failed. See errors above.
    exit /b 1
)

echo.
echo === Building project (Release) ===
cmake --build "%BUILD_DIR%" --config Release --parallel

if errorlevel 1 (
    echo.
    echo Build failed. See errors above.
    exit /b 1
)

echo.
echo === Build succeeded ===
echo Executable should be located inside "%BUILD_DIR%" (e.g. build\Release\FFmpegAudioEditor.exe
echo for MSVC, or build\FFmpegAudioEditor.exe for MinGW).
echo.
echo Run deploy.bat next to gather the Qt runtime DLLs into a distributable folder.

endlocal
