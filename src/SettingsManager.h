#pragma once

#include <QString>
#include <QStringList>
#include <QSize>
#include <QPoint>
#include <QByteArray>
#include <memory>

class QSettings;

// SettingsManager centralizes all persistent application state using
// QSettings (stored in the registry on Windows). It is owned by
// MainWindow as a member (RAII), not a global variable.
class SettingsManager
{
public:
    SettingsManager();
    ~SettingsManager();

    // FFmpeg / FFprobe executable paths.
    QString ffmpegPath() const;
    void setFfmpegPath(const QString &path);

    QString ffprobePath() const;
    void setFfprobePath(const QString &path);

    // Output folder.
    QString outputFolder() const;
    void setOutputFolder(const QString &folder);

    // Window geometry / state.
    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray &geometry);

    QByteArray windowState() const;
    void setWindowState(const QByteArray &state);

    // Theme ("dark" or "light").
    QString theme() const;
    void setTheme(const QString &theme);

    // Recent files (videos) and recent folders (output destinations).
    QStringList recentFiles() const;
    void addRecentFile(const QString &filePath);
    void clearRecentFiles();

    QStringList recentFolders() const;
    void addRecentFolder(const QString &folderPath);
    void clearRecentFolders();

    static constexpr int kMaxRecentEntries = 10;

private:
    std::unique_ptr<QSettings> m_settings;
};
