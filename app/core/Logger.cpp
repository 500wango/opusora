#include "core/Logger.hpp"

#include "core/AppPaths.hpp"

#include <QDateTime>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>
#include <QtGlobal>

#include <cstdlib>
#include <cstdio>

namespace {

QMutex& logMutex()
{
    static QMutex mutex;
    return mutex;
}

QString messageTypeName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return QStringLiteral("DEBUG");
    case QtInfoMsg:
        return QStringLiteral("INFO");
    case QtWarningMsg:
        return QStringLiteral("WARN");
    case QtCriticalMsg:
        return QStringLiteral("ERROR");
    case QtFatalMsg:
        return QStringLiteral("FATAL");
    }

    return QStringLiteral("UNKNOWN");
}

void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    const QString line = QStringLiteral("%1 [%2] %3:%4 %5\n")
        .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs))
        .arg(messageTypeName(type))
        .arg(context.file ? context.file : "-")
        .arg(context.line)
        .arg(message);

    {
        QMutexLocker locker(&logMutex());
        QFile file(opusora::AppPaths::logFilePath());
        if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QTextStream stream(&file);
            stream << line;
        }
    }

    std::fputs(line.toLocal8Bit().constData(), stderr);
    std::fflush(stderr);

    if (type == QtFatalMsg) {
        std::abort();
    }
}

} // namespace

namespace opusora {

void Logger::install()
{
    qInstallMessageHandler(messageHandler);
}

} // namespace opusora
