#include "FFmpegWrapper.h"
#include "FileUtils.h"
#include "Logger.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QRegularExpression>
#include <QDir>

FFmpegWrapper::FFmpegWrapper(QObject *parent)
    : QObject(parent)
{
}

FFmpegWrapper::~FFmpegWrapper()
{
    cancelCurrentOperation();
}

void FFmpegWrapper::setFfmpegPath(const QString &path)  { m_ffmpegPath = path; }
void FFmpegWrapper::setFfprobePath(const QString &path) { m_ffprobePath = path; }

bool FFmpegWrapper::isBusy() const
{
    return m_process && m_process->state() != QProcess::NotRunning;
}

void FFmpegWrapper::cancelCurrentOperation()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(3000);
    }
}

// --------------------------------------------------------------------------
// Probing
// --------------------------------------------------------------------------

void FFmpegWrapper::probeAudioTracks(const QString &videoPath)
{
    if (!FileUtils::isValidExecutable(m_ffprobePath, QStringLiteral("ffprobe"))) {
        emit probeFinished(false, {}, 0.0, tr("ffprobe.exe path is not configured or invalid."));
        return;
    }
    if (!FileUtils::isValidFile(videoPath)) {
        emit probeFinished(false, {}, 0.0, tr("Input video file does not exist."));
        return;
    }

    auto *probeProcess = new QProcess(this);
    const QStringList args = {
        QStringLiteral("-v"), QStringLiteral("quiet"),
        QStringLiteral("-print_format"), QStringLiteral("json"),
        QStringLiteral("-show_format"),
        QStringLiteral("-show_streams"),
        videoPath
    };

    Logger::instance().command(QStringLiteral("%1 %2").arg(m_ffprobePath, args.join(QLatin1Char(' '))));

    connect(probeProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this, probeProcess](int exitCode, QProcess::ExitStatus status) {
                const QByteArray stdOut = probeProcess->readAllStandardOutput();
                const QByteArray stdErr = probeProcess->readAllStandardError();
                probeProcess->deleteLater();

                if (status != QProcess::NormalExit || exitCode != 0) {
                    emit probeFinished(false, {}, 0.0,
                                        tr("ffprobe failed: %1").arg(QString::fromUtf8(stdErr)));
                    return;
                }

                QJsonParseError parseError;
                const QJsonDocument doc = QJsonDocument::fromJson(stdOut, &parseError);
                if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
                    emit probeFinished(false, {}, 0.0,
                                        tr("Failed to parse ffprobe output: %1").arg(parseError.errorString()));
                    return;
                }

                const QJsonObject root = doc.object();
                double durationSeconds = 0.0;
                if (root.contains(QStringLiteral("format"))) {
                    const QJsonObject format = root.value(QStringLiteral("format")).toObject();
                    durationSeconds = format.value(QStringLiteral("duration")).toString().toDouble();
                }

                QVector<AudioTrackInfo> tracks;
                int audioCounter = 0;
                const QJsonArray streams = root.value(QStringLiteral("streams")).toArray();
                for (const QJsonValue &streamVal : streams) {
                    const QJsonObject stream = streamVal.toObject();
                    if (stream.value(QStringLiteral("codec_type")).toString() != QLatin1String("audio")) {
                        continue;
                    }

                    AudioTrackInfo info;
                    info.index = stream.value(QStringLiteral("index")).toInt(-1);
                    info.audioTrackNumber = ++audioCounter;
                    info.codec = stream.value(QStringLiteral("codec_name")).toString();
                    info.channels = stream.value(QStringLiteral("channels")).toInt();
                    info.channelLayout = stream.value(QStringLiteral("channel_layout")).toString();
                    info.sampleRate = stream.value(QStringLiteral("sample_rate")).toString().toInt();

                    // bit_rate may be missing on the stream; fall back to 0.
                    const QString bitRateStr = stream.value(QStringLiteral("bit_rate")).toString();
                    info.bitRate = bitRateStr.toLongLong();

                    if (stream.contains(QStringLiteral("duration"))) {
                        info.durationSeconds = stream.value(QStringLiteral("duration")).toString().toDouble();
                    } else {
                        info.durationSeconds = durationSeconds;
                    }

                    if (stream.contains(QStringLiteral("tags"))) {
                        const QJsonObject tags = stream.value(QStringLiteral("tags")).toObject();
                        info.language = tags.value(QStringLiteral("language")).toString();
                    }

                    if (stream.contains(QStringLiteral("disposition"))) {
                        const QJsonObject disposition = stream.value(QStringLiteral("disposition")).toObject();
                        info.isDefault = disposition.value(QStringLiteral("default")).toInt() == 1;
                    }

                    tracks.append(info);
                }

                emit probeFinished(true, tracks, durationSeconds, QString());
            });

    connect(probeProcess, &QProcess::errorOccurred, this,
            [this, probeProcess](QProcess::ProcessError) {
                const QString err = probeProcess->errorString();
                probeProcess->deleteLater();
                emit probeFinished(false, {}, 0.0, tr("Failed to start ffprobe: %1").arg(err));
            });

    probeProcess->start(m_ffprobePath, args);
}

// --------------------------------------------------------------------------
// Operations
// --------------------------------------------------------------------------

QStringList FFmpegWrapper::buildArgsForOperation(OperationType operation,
                                                  const QString &videoPath,
                                                  const QString &outputFolder,
                                                  const QVector<AudioTrackInfo> &tracks,
                                                  const QString &extraInputPath,
                                                  QString &outPrimaryOutputFile) const
{
    QStringList args;
    args << QStringLiteral("-y") << QStringLiteral("-i") << videoPath;

    const QString sourceExt = QFileInfo(videoPath).suffix().toLower();

    switch (operation) {
    case OperationType::ExtractTrack1: {
        outPrimaryOutputFile = FileUtils::buildOutputFileName(videoPath, outputFolder, QStringLiteral("track1"), QStringLiteral("wav"));
        args << QStringLiteral("-map") << QStringLiteral("0:a:0")
             << QStringLiteral("-vn") << outPrimaryOutputFile;
        break;
    }
    case OperationType::ExtractTrack2: {
        outPrimaryOutputFile = FileUtils::buildOutputFileName(videoPath, outputFolder, QStringLiteral("track2"), QStringLiteral("wav"));
        args << QStringLiteral("-map") << QStringLiteral("0:a:1")
             << QStringLiteral("-vn") << outPrimaryOutputFile;
        break;
    }
    case OperationType::ExtractAllTracks: {
        // Handled specially by caller (one ffmpeg invocation per track is
        // simplest & most robust). This branch produces track 1 only; the
        // wrapper's runOperation() loops for the remaining tracks.
        outPrimaryOutputFile = FileUtils::buildOutputFileName(videoPath, outputFolder, QStringLiteral("track1"), QStringLiteral("wav"));
        args << QStringLiteral("-map") << QStringLiteral("0:a:0")
             << QStringLiteral("-vn") << outPrimaryOutputFile;
        break;
    }
    case OperationType::SwapTrack1And2: {
        outPrimaryOutputFile = FileUtils::buildOutputFileName(videoPath, outputFolder, QStringLiteral("swapped"), sourceExt);
        args << QStringLiteral("-map") << QStringLiteral("0:v")
             << QStringLiteral("-map") << QStringLiteral("0:a:1")
             << QStringLiteral("-map") << QStringLiteral("0:a:0");
        // Preserve any additional audio tracks beyond the first two, unchanged.
        for (int i = 2; i < tracks.size(); ++i) {
            args << QStringLiteral("-map") << QStringLiteral("0:a:%1").arg(i);
        }
        args << QStringLiteral("-map") << QStringLiteral("0:s?")
             << QStringLiteral("-c") << QStringLiteral("copy")
             << outPrimaryOutputFile;
        break;
    }
    case OperationType::DeleteTrack1: {
        outPrimaryOutputFile = FileUtils::buildOutputFileName(videoPath, outputFolder, QStringLiteral("no_track1"), sourceExt);
        args << QStringLiteral("-map") << QStringLiteral("0:v");
        for (int i = 1; i < tracks.size(); ++i) {
            args << QStringLiteral("-map") << QStringLiteral("0:a:%1").arg(i);
        }
        args << QStringLiteral("-map") << QStringLiteral("0:s?")
             << QStringLiteral("-c") << QStringLiteral("copy")
             << outPrimaryOutputFile;
        break;
    }
    case OperationType::DeleteTrack2: {
        outPrimaryOutputFile = FileUtils::buildOutputFileName(videoPath, outputFolder, QStringLiteral("no_track2"), sourceExt);
        args << QStringLiteral("-map") << QStringLiteral("0:v")
             << QStringLiteral("-map") << QStringLiteral("0:a:0");
        for (int i = 2; i < tracks.size(); ++i) {
            args << QStringLiteral("-map") << QStringLiteral("0:a:%1").arg(i);
        }
        args << QStringLiteral("-map") << QStringLiteral("0:s?")
             << QStringLiteral("-c") << QStringLiteral("copy")
             << outPrimaryOutputFile;
        break;
    }
    case OperationType::DeleteAllAudioTracks:
    case OperationType::MuteVideo: {
        outPrimaryOutputFile = FileUtils::buildOutputFileName(videoPath, outputFolder, QStringLiteral("muted"), sourceExt);
        args << QStringLiteral("-map") << QStringLiteral("0:v")
             << QStringLiteral("-map") << QStringLiteral("0:s?")
             << QStringLiteral("-an")
             << QStringLiteral("-c") << QStringLiteral("copy")
             << outPrimaryOutputFile;
        break;
    }
    case OperationType::ReplaceAudio: {
        outPrimaryOutputFile = FileUtils::buildOutputFileName(videoPath, outputFolder, QStringLiteral("replaced_audio"), sourceExt);
        args << QStringLiteral("-i") << extraInputPath
             << QStringLiteral("-map") << QStringLiteral("0:v")
             << QStringLiteral("-map") << QStringLiteral("1:a")
             << QStringLiteral("-c:v") << QStringLiteral("copy")
             << QStringLiteral("-c:a") << QStringLiteral("aac")
             << QStringLiteral("-shortest")
             << outPrimaryOutputFile;
        break;
    }
    case OperationType::MergeExternalAudio: {
        outPrimaryOutputFile = FileUtils::buildOutputFileName(videoPath, outputFolder, QStringLiteral("merged_audio"), sourceExt);
        args << QStringLiteral("-i") << extraInputPath
             << QStringLiteral("-map") << QStringLiteral("0:v")
             << QStringLiteral("-map") << QStringLiteral("0:a")
             << QStringLiteral("-map") << QStringLiteral("1:a")
             << QStringLiteral("-c:v") << QStringLiteral("copy")
             << QStringLiteral("-c:a") << QStringLiteral("aac")
             << outPrimaryOutputFile;
        break;
    }
    }

    return args;
}

void FFmpegWrapper::runOperation(OperationType operation,
                                  const QString &videoPath,
                                  const QString &outputFolder,
                                  const QVector<AudioTrackInfo> &tracks,
                                  double totalDurationSeconds,
                                  const QString &extraInputPath)
{
    if (isBusy()) {
        emit operationFinished(false, tr("Another operation is already running."), QString());
        return;
    }

    if (!FileUtils::isValidExecutable(m_ffmpegPath, QStringLiteral("ffmpeg"))) {
        emit operationFinished(false, tr("ffmpeg.exe path is not configured or invalid."), QString());
        return;
    }
    if (!FileUtils::isValidFile(videoPath)) {
        emit operationFinished(false, tr("Input video file does not exist."), QString());
        return;
    }
    if (!FileUtils::isValidDirectory(outputFolder)) {
        emit operationFinished(false, tr("Output folder is invalid."), QString());
        return;
    }

    QString primaryOutput;
    QStringList args = buildArgsForOperation(operation, videoPath, outputFolder, tracks, extraInputPath, primaryOutput);

    m_currentTotalDuration = totalDurationSeconds;
    m_currentOutputFile = primaryOutput;
    m_operationQueue.clear();

    // "Extract all tracks" requires one ffmpeg call per audio stream; the
    // first call was already built above for track 1, so queue the rest
    // (track 2..N) as complete, ready-to-run (args, outputFile) pairs.
    if (operation == OperationType::ExtractAllTracks) {
        for (int i = 1; i < tracks.size(); ++i) {
            const QString outFile = FileUtils::buildOutputFileName(
                videoPath, outputFolder, QStringLiteral("track%1").arg(i + 1), QStringLiteral("wav"));
            const QStringList trackArgs = {
                QStringLiteral("-y"), QStringLiteral("-i"), videoPath,
                QStringLiteral("-map"), QStringLiteral("0:a:%1").arg(i),
                QStringLiteral("-vn"), outFile
            };
            m_operationQueue.append(qMakePair(trackArgs, outFile));
        }
    }

    startProcess(args);
}

void FFmpegWrapper::startProcess(const QStringList &args)
{
    m_process = std::make_unique<QProcess>();
    m_elapsedTimer.start();

    const QString commandLine = QStringLiteral("\"%1\" %2").arg(m_ffmpegPath, args.join(QLatin1Char(' ')));
    Logger::instance().command(commandLine);
    emit operationStarted(commandLine);

    connect(m_process.get(), &QProcess::readyReadStandardError, this, [this]() {
        const QByteArray data = m_process->readAllStandardError();
        const QString text = QString::fromUtf8(data);
        for (const QString &line : text.split(QRegularExpression(QStringLiteral("[\\r\\n]")), Qt::SkipEmptyParts)) {
            emit outputLine(line, true);
            parseProgressLine(line);
        }
    });

    connect(m_process.get(), &QProcess::readyReadStandardOutput, this, [this]() {
        const QByteArray data = m_process->readAllStandardOutput();
        const QString text = QString::fromUtf8(data);
        for (const QString &line : text.split(QRegularExpression(QStringLiteral("[\\r\\n]")), Qt::SkipEmptyParts)) {
            emit outputLine(line, false);
        }
    });

    connect(m_process.get(), qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int exitCode, QProcess::ExitStatus status) {
                const bool success = (status == QProcess::NormalExit && exitCode == 0);
                const QString finishedFile = m_currentOutputFile;

                if (success && !m_operationQueue.isEmpty()) {
                    // Chain the next queued "extract all tracks" sub-operation.
                    const auto next = m_operationQueue.takeFirst();
                    m_currentOutputFile = next.second;
                    startProcess(next.first);
                    return;
                }

                if (success) {
                    Logger::instance().success(tr("Operation completed successfully: %1").arg(finishedFile));
                } else {
                    Logger::instance().error(tr("Operation failed (exit code %1).").arg(exitCode));
                }

                emit operationFinished(success,
                                        success ? tr("Done.") : tr("ffmpeg exited with an error. See console for details."),
                                        finishedFile);
            });

    connect(m_process.get(), &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        Logger::instance().error(tr("Failed to start ffmpeg: %1").arg(m_process->errorString()));
        emit operationFinished(false, tr("Failed to start ffmpeg: %1").arg(m_process->errorString()), QString());
    });

    m_process->start(m_ffmpegPath, args);
}

void FFmpegWrapper::parseProgressLine(const QString &line)
{
    // ffmpeg prints lines like:
    // frame=  120 fps= 30 q=-1.0 size=    512kB time=00:00:04.00 bitrate= 1048.6kbits/s speed=1.2x
    static const QRegularExpression timeRe(QStringLiteral("time=(\\d+):(\\d+):(\\d+\\.\\d+)"));
    const auto match = timeRe.match(line);
    if (!match.hasMatch() || m_currentTotalDuration <= 0.0) {
        return;
    }

    const double hours = match.captured(1).toDouble();
    const double minutes = match.captured(2).toDouble();
    const double seconds = match.captured(3).toDouble();
    const double currentSeconds = hours * 3600.0 + minutes * 60.0 + seconds;

    int percent = static_cast<int>((currentSeconds / m_currentTotalDuration) * 100.0);
    percent = qBound(0, percent, 100);

    const qint64 elapsedMs = m_elapsedTimer.elapsed();
    qint64 remainingMs = 0;
    if (percent > 0) {
        const qint64 estimatedTotalMs = static_cast<qint64>(elapsedMs * 100.0 / percent);
        remainingMs = qMax<qint64>(0, estimatedTotalMs - elapsedMs);
    }

    emit progressChanged(percent, elapsedMs, remainingMs);
}
