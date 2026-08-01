#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QProcess>
#include <QElapsedTimer>
#include <QPair>
#include <memory>

// Describes a single audio stream found inside a video container, as
// reported by ffprobe.
struct AudioTrackInfo
{
    int index = -1;                 // Stream index as reported by ffprobe (absolute stream index).
    int audioTrackNumber = -1;      // 1-based position among audio streams only (Track 1, Track 2, ...).
    QString codec;                  // e.g. "aac", "ac3"
    qint64 bitRate = 0;             // bits per second, 0 if unknown
    int channels = 0;               // number of audio channels
    QString channelLayout;          // e.g. "5.1", "stereo"
    int sampleRate = 0;             // Hz
    QString language;               // ISO 639 language tag, empty if not present
    double durationSeconds = 0.0;   // stream duration in seconds, 0 if unknown
    bool isDefault = false;         // whether the DISPOSITION:default flag is set
};

// The set of high level operations the application can perform on a video.
enum class OperationType
{
    ExtractTrack1,
    ExtractTrack2,
    ExtractAllTracks,
    SwapTrack1And2,
    DeleteTrack1,
    DeleteTrack2,
    DeleteAllAudioTracks,
    MuteVideo,
    ReplaceAudio,
    MergeExternalAudio
};

// FFmpegWrapper is a thin, asynchronous QObject wrapper around ffmpeg.exe
// and ffprobe.exe. It never blocks the UI thread: all process execution
// happens through QProcess in asynchronous mode, driven by the Qt event
// loop, so no manual QThread is required. All state is owned via RAII
// (std::unique_ptr<QProcess>).
class FFmpegWrapper : public QObject
{
    Q_OBJECT

public:
    explicit FFmpegWrapper(QObject *parent = nullptr);
    ~FFmpegWrapper() override;

    void setFfmpegPath(const QString &path);
    void setFfprobePath(const QString &path);

    QString ffmpegPath() const { return m_ffmpegPath; }
    QString ffprobePath() const { return m_ffprobePath; }

    // Starts an asynchronous ffprobe query on videoPath. Emits
    // probeFinished() when done. Non-blocking.
    void probeAudioTracks(const QString &videoPath);

    // Starts an asynchronous ffmpeg operation. Emits operationStarted(),
    // repeated progressChanged(), and finally operationFinished().
    // extraInputPath is used for operations that need a second input file
    // (ReplaceAudio, MergeExternalAudio); ignored otherwise.
    void runOperation(OperationType operation,
                       const QString &videoPath,
                       const QString &outputFolder,
                       const QVector<AudioTrackInfo> &tracks,
                       double totalDurationSeconds,
                       const QString &extraInputPath = QString());

    // Cancels the currently running ffmpeg operation, if any.
    void cancelCurrentOperation();

    bool isBusy() const;

signals:
    void probeFinished(bool success, const QVector<AudioTrackInfo> &tracks,
                        double durationSeconds, const QString &errorMessage);

    void operationStarted(const QString &commandLine);
    void outputLine(const QString &line, bool isStderr);
    void progressChanged(int percent, qint64 elapsedMs, qint64 remainingMsEstimate);
    void operationFinished(bool success, const QString &message, const QString &outputFilePath);

private:
    void startProcess(const QStringList &args);
    QStringList buildArgsForOperation(OperationType operation,
                                       const QString &videoPath,
                                       const QString &outputFolder,
                                       const QVector<AudioTrackInfo> &tracks,
                                       const QString &extraInputPath,
                                       QString &outPrimaryOutputFile) const;
    void parseProgressLine(const QString &line);

    QString m_ffmpegPath;
    QString m_ffprobePath;

    std::unique_ptr<QProcess> m_process;
    QElapsedTimer m_elapsedTimer;
    double m_currentTotalDuration = 0.0;
    QString m_currentOutputFile;

    // Queue of (args, outputFile) pairs used to chain multiple ffmpeg
    // invocations for operations such as "extract all tracks", which need
    // one ffmpeg call per audio stream.
    QVector<QPair<QStringList, QString>> m_operationQueue;
};
