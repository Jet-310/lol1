@echo off
REM ============================================================
REM  deploy.bat - Creates a standalone distributable folder using
REM               windeployqt.
REM
REM  Usage:
REM    deploy.bat "C:\Qt\6.7.0\msvc2019_64\bin" "build\Release\FFmpegAudioEditor.exe"
REM    deploy.bat "C:\Qt\6.7.0\mingw_64\bin" "build\FFmpegAudioEditor.exe"
REM ============================================================

setlocal

set "QT_BIN=%~1"
set "EXE_PATH=%~2"

if "%QT_BIN%"=="" (
    echo Usage: deploy.bat "path\to\Qt\bin" "path\to\FFmpegAudioEditor.exe"
    exit /b 1
)
if "%EXE_PATH%"=="" (
    echo Usage: deploy.bat "path\to\Qt\bin" "path\to\FFmpegAudioEditor.exe"
    exit /b 1
)

if not exist "%EXE_PATH%" (
    echo Could not find executable at: %EXE_PATH%
    echo Build the project first with build.bat.
    exit /b 1
)

set "DIST_DIR=dist"

if exist "%DIST_DIR%" rd /s /q "%DIST_DIR%"
mkdir "%DIST_DIR%"

echo.
echo === Copying executable ===
copy "%EXE_PATH%" "%DIST_DIR%\" >nul

echo.
echo === Running windeployqt ===
"%QT_BIN%\windeployqt.exe" --release --no-translations --dir "%DIST_DIR%" "%DIST_DIR%\FFmpegAudioEditor.exe"

if errorlevel 1 (
    echo.
    echo windeployqt reported an error. Check that QT_BIN points to the
    echo correct Qt "bin" folder for the toolchain you built with.
    exit /b 1
)

echo.
echo === Deployment complete ===
echo Distributable application is ready in the "%DIST_DIR%" folder.
echo You can zip this folder and share it; it does not require Qt to be
echo installed on the target machine. FFmpeg / FFprobe are NOT bundled -
echo point the app at them on first run.

endlocal
