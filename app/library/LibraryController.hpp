#pragma once

#include "library/LibraryRepository.hpp"
#include "library/LibraryScanner.hpp"
#include "metadata/MetadataReader.hpp"

#include <QDirIterator>
#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QFileSystemWatcher>
#include <QSet>
#include <QStringList>
#include <QTimer>

#include <memory>

namespace opusora {

class Database;
class LibraryRepository;

class LibraryController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList roots READ roots NOTIFY rootsChanged)
    Q_PROPERTY(QString scanStatus READ scanStatus NOTIFY scanStatusChanged)
    Q_PROPERTY(int totalFiles READ totalFiles NOTIFY scanSummaryChanged)
    Q_PROPERTY(int audioFiles READ audioFiles NOTIFY scanSummaryChanged)
    Q_PROPERTY(int failedFiles READ failedFiles NOTIFY scanSummaryChanged)
    Q_PROPERTY(bool isScanning READ isScanning NOTIFY scanStatusChanged)

public:
    explicit LibraryController(Database* database, QObject* parent = nullptr);

    QStringList roots() const;
    QString scanStatus() const;
    int totalFiles() const;
    int audioFiles() const;
    int failedFiles() const;
    bool isScanning() const;

    Q_INVOKABLE void requestAddFolder();
    Q_INVOKABLE void addRoot(const QString& path);
    Q_INVOKABLE void removeRoot(const QString& path);
    Q_INVOKABLE void scanRoot(const QString& path);
    Q_INVOKABLE void scanAll();
    Q_INVOKABLE void cancelScan();
    Q_INVOKABLE void rebuildLibrary();

signals:
    void rootsChanged();
    void scanStatusChanged();
    void scanSummaryChanged();
    void libraryChanged();

private:
    void loadRoots();
    bool persistRoot(const QString& path);
    bool deleteRoot(const QString& path);
    void startScanQueue(const QStringList& roots);
    void startNextScanRoot();
    void processScanBatch();
    void finishCurrentScanRoot();
    void finishScanQueue(const QString& status);
    void resetScanState();
    void refreshFileWatchers();
    void scheduleAutoScan(const QString& changedPath);
    void runAutoScan();
    void setScanStatus(const QString& scanStatus);

    Database* m_database = nullptr;
    QStringList m_roots;
    QString m_scanStatus = QStringLiteral("idle");
    int m_totalFiles = 0;
    int m_audioFiles = 0;
    int m_failedFiles = 0;
    bool m_cancelRequested = false;
    QFileSystemWatcher m_fileWatcher;
    QTimer m_autoScanTimer;
    QTimer m_scanWorkTimer;
    bool m_autoScanPending = false;
    QStringList m_pendingScanRoots;
    QString m_currentScanRoot;
    std::unique_ptr<QDirIterator> m_scanIterator;
    std::unique_ptr<MetadataReader> m_metadataReader;
    QSet<QString> m_currentRootAudioPaths;
    QHash<QString, TrackFileStamp> m_currentRootFileStamps;
    ScanSummary m_currentRootSummary;
    int m_currentRootSkippedUnchangedFiles = 0;
    bool m_scanTransactionStarted = false;
};

} // namespace opusora
