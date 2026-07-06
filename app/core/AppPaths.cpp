#include "core/AppPaths.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>

namespace {

QString writableLocation(QStandardPaths::StandardLocation location, const QString& fallback)
{
    const QString resolved = QStandardPaths::writableLocation(location);
    if (!resolved.isEmpty()) {
        return resolved;
    }

    return QDir::home().filePath(fallback);
}

} // namespace

namespace opusora {

bool AppPaths::ensureCreated()
{
    QDir dir;
    return dir.mkpath(configDir())
        && dir.mkpath(dataDir())
        && dir.mkpath(cacheDir())
        && dir.mkpath(QDir(dataDir()).filePath("covers"));
}

QString AppPaths::configDir()
{
    return writableLocation(QStandardPaths::AppConfigLocation, ".config/opusora");
}

QString AppPaths::dataDir()
{
    return writableLocation(QStandardPaths::AppDataLocation, ".local/share/opusora");
}

QString AppPaths::cacheDir()
{
    return writableLocation(QStandardPaths::CacheLocation, ".cache/opusora");
}

QString AppPaths::databasePath()
{
    return QDir(dataDir()).filePath("opusora.sqlite3");
}

QString AppPaths::logFilePath()
{
    return QDir(cacheDir()).filePath("opusora.log");
}

} // namespace opusora
