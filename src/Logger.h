#pragma once

#include <QObject>
#include <QString>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <memory>

// Severity levels used to colorize console output and filter log entries.
enum class LogLevel {
    Info,
    Success,
    Warning,
    Error,
    Command
};

// Logger is a QObject based logging utility. It is NOT a global variable;
// instead a single instance is owned by MainWindow and passed around via
// dependency injection (constructor / setter) or accessed through
// Logger::instance() which lazily creates a function-local static instance.
// This avoids the "no global variables" restriction while still providing
// convenient app-wide access.
class Logger : public QObject
{
    Q_OBJECT

public:
    explicit Logger(QObject *parent = nullptr);
    ~Logger() override;

    // Returns the single shared Logger instance (function-local static,
    // not a global variable).
    static Logger &instance();

    // Logs a message with the given severity. Emits messageLogged() so the
    // UI console can append a colorized line in real time.
    void log(LogLevel level, const QString &message);

    // Convenience wrappers.
    void info(const QString &message);
    void success(const QString &message);
    void warning(const QString &message);
    void error(const QString &message);
    void command(const QString &message);

    // Enables writing every log entry to disk as it is produced.
    bool startFileLogging(const QString &filePath);
    void stopFileLogging();

    // Saves everything logged so far in this session to a text file.
    bool saveSessionLog(const QString &filePath) const;

    // Returns the full in-memory session log (plain text, timestamped).
    QString sessionLogText() const;

signals:
    // Emitted for every log entry. The UI listens to this to append
    // colorized lines to the dockable console.
    void messageLogged(LogLevel level, const QString &timestampedMessage);

private:
    static QString levelToPrefix(LogLevel level);

    QStringList m_sessionLines;
    std::unique_ptr<QFile> m_logFile;
    std::unique_ptr<QTextStream> m_logStream;
};
