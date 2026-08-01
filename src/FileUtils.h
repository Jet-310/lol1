#pragma once

#include <QString>
#include <QStringList>

// FileUtils groups small, stateless, static helper functions used across
// the application: path validation, filename generation, and formatting
// helpers for durations / bitrates / sample rates.
class FileUtils
{
public:
    FileUtils() = delete; // Purely static utility class.

    // Supported input video container extensions (lower-case, no dot).
    static QStringList supportedVideoExtensions();

    // Returns a Qt file dialog filter string built from supportedVideoExtensions().
    static QString videoFileDialogFilter();

    // True if the path exists and is a regular, readable file.
    static bool isValidFile(const QString &path);

    // True if the path exists and is a directory.
    static bool isValidDirectory(const QString &path);

    // True if the path looks like ffmpeg.exe / ffprobe.exe (or ffmpeg/ffprobe
    // on non-Windows systems) and exists on disk.
    static bool isValidExecutable(const QString &path, const QString &expectedBaseName);

    // Builds an output file path inside outputFolder using the base name of
    // sourceVideoPath, a descriptive suffix and a target extension.
    // Example: buildOutputFileName("C:/vids/movie.mkv", "/out", "track1", "wav")
    //          -> "/out/movie_track1.wav"
    static QString buildOutputFileName(const QString &sourceVideoPath,
                                        const QString &outputFolder,
                                        const QString &suffix,
                                        const QString &extension);

    // Returns the base name of a path without its directory or extension.
    static QString baseNameWithoutExtension(const QString &path);

    // Formats a duration given in seconds as HH:MM:SS.
    static QString formatDuration(double seconds);

    // Formats a bitrate given in bits/second as a human readable string
    // (e.g. "192 kb/s").
    static QString formatBitrate(qint64 bitsPerSecond);

    // Ensures a directory exists, creating it (and parents) if necessary.
    static bool ensureDirectoryExists(const QString &path);
};
