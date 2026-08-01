#include "FileUtils.h"

#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>

QStringList FileUtils::supportedVideoExtensions()
{
    return { QStringLiteral("mp4"), QStringLiteral("mkv"), QStringLiteral("mov"),
             QStringLiteral("avi"), QStringLiteral("webm"), QStringLiteral("flv"),
             QStringLiteral("ts") };
}

QString FileUtils::videoFileDialogFilter()
{
    QStringList patterns;
    for (const QString &ext : supportedVideoExtensions()) {
        patterns << QStringLiteral("*.%1").arg(ext);
    }
    return QStringLiteral("Video Files (%1);;All Files (*.*)").arg(patterns.join(QLatin1Char(' ')));
}

bool FileUtils::isValidFile(const QString &path)
{
    if (path.isEmpty()) {
        return false;
    }
    const QFileInfo info(path);
    return info.exists() && info.isFile() && info.isReadable();
}

bool FileUtils::isValidDirectory(const QString &path)
{
    if (path.isEmpty()) {
        return false;
    }
    const QFileInfo info(path);
    return info.exists() && info.isDir();
}

bool FileUtils::isValidExecutable(const QString &path, const QString &expectedBaseName)
{
    if (!isValidFile(path)) {
        return false;
    }
    const QFileInfo info(path);
    const QString baseName = info.completeBaseName().toLower();
    return baseName.contains(expectedBaseName.toLower());
}

QString FileUtils::baseNameWithoutExtension(const QString &path)
{
    return QFileInfo(path).completeBaseName();
}

QString FileUtils::buildOutputFileName(const QString &sourceVideoPath,
                                        const QString &outputFolder,
                                        const QString &suffix,
                                        const QString &extension)
{
    const QString base = baseNameWithoutExtension(sourceVideoPath);
    // Sanitize the base name: strip characters that are illegal in Windows
    // file names to keep the generated file safe to create.
    QString safeBase = base;
    safeBase.remove(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")));

    const QString fileName = QStringLiteral("%1_%2.%3").arg(safeBase, suffix, extension);
    QDir dir(outputFolder);
    return dir.filePath(fileName);
}

QString FileUtils::formatDuration(double seconds)
{
    if (seconds < 0) {
        seconds = 0;
    }
    const qint64 totalSeconds = static_cast<qint64>(seconds);
    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds % 3600) / 60;
    const qint64 secs = totalSeconds % 60;
    return QStringLiteral("%1:%2:%3")
        .arg(hours, 2, 10, QLatin1Char('0'))
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(secs, 2, 10, QLatin1Char('0'));
}

QString FileUtils::formatBitrate(qint64 bitsPerSecond)
{
    if (bitsPerSecond <= 0) {
        return QStringLiteral("N/A");
    }
    if (bitsPerSecond >= 1000000) {
        return QStringLiteral("%1 Mb/s").arg(bitsPerSecond / 1000000.0, 0, 'f', 2);
    }
    return QStringLiteral("%1 kb/s").arg(bitsPerSecond / 1000);
}

bool FileUtils::ensureDirectoryExists(const QString &path)
{
    QDir dir(path);
    if (dir.exists()) {
        return true;
    }
    return dir.mkpath(QStringLiteral("."));
}
