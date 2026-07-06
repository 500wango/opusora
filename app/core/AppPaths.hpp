#pragma once

#include <QString>

namespace opusora {

class AppPaths {
public:
    static bool ensureCreated();

    static QString configDir();
    static QString dataDir();
    static QString cacheDir();
    static QString databasePath();
    static QString logFilePath();
};

} // namespace opusora
