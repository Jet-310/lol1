#include "SettingsManager.h"
#include <QSettings>

namespace {
constexpr auto kKeyFfmpegPath      = "Paths/FfmpegPath";
constexpr auto kKeyFfprobePath     = "Paths/FfprobePath";
constexpr auto kKeyOutputFolder    = "Paths/OutputFolder";
constexpr auto kKeyWindowGeometry  = "Window/Geometry";
constexpr auto kKeyWindowState     = "Window/State";
constexpr auto kKeyTheme           = "UI/Theme";
constexpr auto kKeyRecentFiles     = "Recent/Files";
constexpr auto kKeyRecentFolders   = "Recent/Folders";
}

SettingsManager::SettingsManager()
    : m_settings(std::make_unique<QSettings>(QStringLiteral("Codex"), QStringLiteral("FFmpegAudioEditor")))
{
}

SettingsManager::~SettingsManager() = default;

QString SettingsManager::ffmpegPath() const
{
    return m_settings->value(kKeyFfmpegPath).toString();
}

void SettingsManager::setFfmpegPath(const QString &path)
{
    m_settings->setValue(kKeyFfmpegPath, path);
}

QString SettingsManager::ffprobePath() const
{
    return m_settings->value(kKeyFfprobePath).toString();
}

void SettingsManager::setFfprobePath(const QString &path)
{
    m_settings->setValue(kKeyFfprobePath, path);
}

QString SettingsManager::outputFolder() const
{
    return m_settings->value(kKeyOutputFolder).toString();
}

void SettingsManager::setOutputFolder(const QString &folder)
{
    m_settings->setValue(kKeyOutputFolder, folder);
}

QByteArray SettingsManager::windowGeometry() const
{
    return m_settings->value(kKeyWindowGeometry).toByteArray();
}

void SettingsManager::setWindowGeometry(const QByteArray &geometry)
{
    m_settings->setValue(kKeyWindowGeometry, geometry);
}

QByteArray SettingsManager::windowState() const
{
    return m_settings->value(kKeyWindowState).toByteArray();
}

void SettingsManager::setWindowState(const QByteArray &state)
{
    m_settings->setValue(kKeyWindowState, state);
}

QString SettingsManager::theme() const
{
    return m_settings->value(kKeyTheme, QStringLiteral("dark")).toString();
}

void SettingsManager::setTheme(const QString &theme)
{
    m_settings->setValue(kKeyTheme, theme);
}

QStringList SettingsManager::recentFiles() const
{
    return m_settings->value(kKeyRecentFiles).toStringList();
}

void SettingsManager::addRecentFile(const QString &filePath)
{
    QStringList files = recentFiles();
    files.removeAll(filePath);
    files.prepend(filePath);
    while (files.size() > kMaxRecentEntries) {
        files.removeLast();
    }
    m_settings->setValue(kKeyRecentFiles, files);
}

void SettingsManager::clearRecentFiles()
{
    m_settings->setValue(kKeyRecentFiles, QStringList());
}

QStringList SettingsManager::recentFolders() const
{
    return m_settings->value(kKeyRecentFolders).toStringList();
}

void SettingsManager::addRecentFolder(const QString &folderPath)
{
    QStringList folders = recentFolders();
    folders.removeAll(folderPath);
    folders.prepend(folderPath);
    while (folders.size() > kMaxRecentEntries) {
        folders.removeLast();
    }
    m_settings->setValue(kKeyRecentFolders, folders);
}

void SettingsManager::clearRecentFolders()
{
    m_settings->setValue(kKeyRecentFolders, QStringList());
}
