#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

namespace opusora {

struct ScanSummary {
    int totalFiles = 0;
    int audioFiles = 0;
    int failedFiles = 0;
    bool cancelled = false;
};

class LibraryScanner : public QObject {
    Q_OBJECT

public:
    explicit LibraryScanner(QObject* parent = nullptr);

    static bool isSupportedAudioFile(const QString& filePath);
    static QStringList supportedExtensions();

    ScanSummary scan(const QString& rootPath) const;
    QStringList audioFiles(
        const QString& rootPath,
        ScanSummary* summary = nullptr,
        const std::function<bool()>& shouldCancel = {}) const;
};

} // namespace opusora
