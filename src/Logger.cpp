#include "Logger.h"

Logger::Logger(QObject *parent)
    : QObject(parent)
{
}

Logger::~Logger()
{
    stopFileLogging();
}

Logger &Logger::instance()
{
    // Function-local static: constructed once, on first use, thread-safe
    // in C++11 and later. This is intentionally not a global variable.
    static Logger sharedInstance;
    return sharedInstance;
}

QString Logger::levelToPrefix(LogLevel level)
{
    switch (level) {
    case LogLevel::Info:      return QStringLiteral("INFO");
    case LogLevel::Success:   return QStringLiteral("OK");
    case LogLevel::Warning:   return QStringLiteral("WARN");
    case LogLevel::Error:     return QStringLiteral("ERROR");
    case LogLevel::Command:   return QStringLiteral("CMD");
    }
    return QStringLiteral("INFO");
}

void Logger::log(LogLevel level, const QString &message)
{
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    const QString line = QStringLiteral("[%1] [%2] %3").arg(timestamp, levelToPrefix(level), message);

    m_sessionLines.append(line);

    if (m_logStream) {
        (*m_logStream) << line << Qt::endl;
        m_logStream->flush();
    }

    emit messageLogged(level, line);
}

void Logger::info(const QString &message)    { log(LogLevel::Info, message); }
void Logger::success(const QString &message) { log(LogLevel::Success, message); }
void Logger::warning(const QString &message) { log(LogLevel::Warning, message); }
void Logger::error(const QString &message)   { log(LogLevel::Error, message); }
void Logger::command(const QString &message) { log(LogLevel::Command, message); }

bool Logger::startFileLogging(const QString &filePath)
{
    stopFileLogging();

    auto file = std::make_unique<QFile>(filePath);
    if (!file->open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
        return false;
    }

    m_logFile = std::move(file);
    m_logStream = std::make_unique<QTextStream>(m_logFile.get());
    return true;
}

void Logger::stopFileLogging()
{
    if (m_logStream) {
        m_logStream->flush();
        m_logStream.reset();
    }
    if (m_logFile) {
        m_logFile->close();
        m_logFile.reset();
    }
}

bool Logger::saveSessionLog(const QString &filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }
    QTextStream stream(&file);
    for (const QString &line : m_sessionLines) {
        stream << line << Qt::endl;
    }
    return true;
}

QString Logger::sessionLogText() const
{
    return m_sessionLines.join(QLatin1Char('\n'));
}
