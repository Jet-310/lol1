# FFmpeg Audio Track Editor

A Windows desktop GUI, built with C++20 and Qt 6 Widgets, that acts as a
graphical front end for `ffmpeg.exe` / `ffprobe.exe`. It lets you inspect
the audio tracks inside a video file and perform common audio-track edits
(extract, swap, delete, mute, replace, merge) without touching the command
line, copying streams (`-c copy`) whenever possible for fast, lossless
output.

## Features

- Browse / remember paths to `ffmpeg.exe` and `ffprobe.exe`
- Open videos via Browse, drag-and-drop, or a Recent Files list
- Detect and display every audio track: codec, bitrate, channels, sample
  rate, language, duration, default flag
- Operations: extract track 1 / track 2 / all tracks, swap tracks 1 & 2,
  delete track 1 / track 2 / all audio, mute video, replace audio, merge
  external audio
- Dockable, color-coded FFmpeg console (errors red, warnings orange,
  success green) with log saving
- Live progress bar, elapsed/remaining time, current command display, and
  cancel button — all fully asynchronous (QProcess + the Qt event loop),
  so the UI never freezes
- Modern dark theme (toggle to light with `Ctrl+T`), toolbar, status bar,
  context menus, keyboard shortcuts
- Persistent settings (QSettings): window geometry, theme, recent files
  and folders, last-used FFmpeg/FFprobe paths, last output folder
- Defensive error handling: missing FFmpeg/FFprobe, missing input,
  invalid output folder, single-track or audio-less videos are all
  detected and reported with clear dialogs instead of crashing

## Project Structure

```
FFmpegAudioEditor/
├── CMakeLists.txt
├── build.bat
├── deploy.bat
├── resources.qrc
├── resources/icons/app.svg
├── README.md
└── src/
    ├── main.cpp
    ├── MainWindow.h / .cpp
    ├── FFmpegWrapper.h / .cpp
    ├── SettingsManager.h / .cpp
    ├── FileUtils.h / .cpp
    └── Logger.h / .cpp
```

The entire UI is built in code (no separate `.ui` file) so the project
depends only on CMake + Qt6, with no IDE-specific `.ui` tooling required.

## 0. Fastest Path: Build in the Cloud (No Local Install Needed)

This project includes `.github/workflows/build-windows.yml`, a GitHub
Actions workflow that builds the app on a real Windows runner and hands
you back a ready-to-run, self-contained folder — you don't need Qt,
CMake, or Visual Studio on your own machine at all.

1. Create a new (empty) repository on https://github.com
2. Upload/push this entire project folder to it
3. Go to the repo's **Actions** tab — the workflow runs automatically
4. Open the completed run and download the **FFmpegAudioEditor-windows**
   artifact (a zip containing `FFmpegAudioEditor.exe` plus all required
   Qt DLLs)
5. Unzip anywhere on Windows and run `FFmpegAudioEditor.exe`

If you'd rather build locally instead, follow the sections below.

## 1. Installing Qt

1. Download the Qt Online Installer from https://www.qt.io/download-qt-installer
2. Install **Qt 6.5 or newer**. During component selection, choose one (or
   both) of:
   - **MSVC 2019/2022 64-bit** — for building with Visual Studio
   - **MinGW 64-bit** — for building with MinGW (no Visual Studio needed)
3. Also install the matching **CMake** and **Ninja** components offered by
   the Qt installer (or install CMake separately from https://cmake.org).
4. Note the installation path, e.g. `C:\Qt\6.7.0\msvc2019_64` or
   `C:\Qt\6.7.0\mingw_64` — you will need it below.

## 2. Building with Visual Studio (MSVC)

Open a **"x64 Native Tools Command Prompt for VS"** (or a regular
`cmd.exe`, as long as `cl.exe` is on PATH), then:

```bat
cd FFmpegAudioEditor
build.bat "C:\Qt\6.7.0\msvc2019_64"
```

Or manually:

```bat
cmake -B build -DCMAKE_PREFIX_PATH="C:\Qt\6.7.0\msvc2019_64" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

The executable is produced at `build\Release\FFmpegAudioEditor.exe`.

### Using Qt Creator / Visual Studio IDE

You can also just open the folder in **Qt Creator** (`File > Open File or
Project... > CMakeLists.txt`) or in **Visual Studio** (`File > Open >
CMake...`), select a Qt6 kit, and build — no extra configuration is
needed beyond pointing the kit at your Qt installation.

## 3. Building with MinGW

```bat
cd FFmpegAudioEditor
build.bat "C:\Qt\6.7.0\mingw_64" mingw
```

Or manually:

```bat
cmake -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:\Qt\6.7.0\mingw_64" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

The executable is produced at `build\FFmpegAudioEditor.exe`.

## 4. Creating a Distributable Folder (windeployqt)

After building, run `deploy.bat`, pointing it at the Qt `bin` directory you
built against and the resulting executable:

```bat
REM MSVC build:
deploy.bat "C:\Qt\6.7.0\msvc2019_64\bin" "build\Release\FFmpegAudioEditor.exe"

REM MinGW build:
deploy.bat "C:\Qt\6.7.0\mingw_64\bin" "build\FFmpegAudioEditor.exe"
```

This copies the executable into a `dist\` folder and runs
`windeployqt.exe` to pull in every required Qt DLL, platform plugin, and
style plugin. The `dist\` folder is then fully self-contained and can be
zipped and shared with users who do **not** have Qt installed.

> **Note:** FFmpeg and FFprobe themselves are *not* bundled. On first
> launch, point the application at your own `ffmpeg.exe` / `ffprobe.exe`
> (e.g. downloaded from https://www.gyan.dev/ffmpeg/builds/) using the
> toolbar buttons; the paths are remembered automatically afterward.

## 5. Usage Summary

1. Set the `ffmpeg.exe` and `ffprobe.exe` paths (toolbar buttons).
2. Browse for a video, or drag-and-drop it onto the window.
3. Choose an output folder.
4. Review the detected audio tracks in the table.
5. Click the desired operation button. Progress, elapsed/remaining time,
   and the exact FFmpeg command are shown live; full output streams into
   the dockable console below.
6. Cancel at any time with the Cancel button.

## Notes on Implementation

- All FFmpeg/FFprobe execution is asynchronous via `QProcess`, driven by
  the Qt event loop — there is no blocking `waitForFinished()` call on the
  UI thread, so the interface remains responsive at all times.
- Stream operations (swap, delete track, mute) use `-c copy` to avoid
  re-encoding whenever the operation only rearranges or drops streams.
  Operations that mix multiple sources (replace/merge audio) re-encode the
  audio stream to AAC while still copying the video stream untouched.
- Settings are stored via `QSettings` under `Codex / FFmpegAudioEditor`
  (Windows Registry).
