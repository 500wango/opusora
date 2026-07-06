#include "library/LibraryScanner.hpp"

#include <QCoreApplication>
#include <QDirIterator>
#include <QEventLoop>
#include <QFileInfo>

namespace opusora {

LibraryScanner::LibraryScanner(QObject* parent)
    : QObject(parent)
{
}

bool LibraryScanner::isSupportedAudioFile(const QString& filePath)
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    return supportedExtensions().contains(suffix);
}

QStringList LibraryScanner::supportedExtensions()
{
    return {
        QStringLiteral("mp3"),
        QStringLiteral("flac"),
        QStringLiteral("wav"),
        QStringLiteral("m4a"),
        QStringLiteral("aac"),
        QStringLiteral("ogg"),
        QStringLiteral("opus"),
    };
}

ScanSummary LibraryScanner::scan(const QString& rootPath) const
{
    ScanSummary summary;
    audioFiles(rootPath, &summary);
    return summary;
}

QStringList LibraryScanner::audioFiles(
    const QString& rootPath,
    ScanSummary* summary,
    const std::function<bool()>& shouldCancel) const
{
    ScanSummary localSummary;
    ScanSummary& target = summary ? *summary : localSummary;
    QStringList files;

    QDirIterator iterator(
        rootPath,
        QDir::Files | QDir::Readable | QDir::NoSymLinks,
        QDirIterator::Subdirectories);

    while (iterator.hasNext()) {
        if (shouldCancel && shouldCancel()) {
            target.cancelled = true;
            break;
        }

        const QString filePath = iterator.next();
        ++target.totalFiles;
        if (isSupportedAudioFile(filePath)) {
            ++target.audioFiles;
            files.append(filePath);
        }

        if (target.totalFiles % 100 == 0) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        }
    }

    return files;
}

} // namespace opusora
