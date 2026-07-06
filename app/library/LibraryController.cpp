#include "library/LibraryController.hpp"

#include "db/Database.hpp"
#include "library/LibraryRepository.hpp"
#include "library/LibraryScanner.hpp"
#include "metadata/MetadataReaderFactory.hpp"

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QFileInfo>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace opusora {

LibraryController::LibraryController(Database* database, QObject* parent)
    : QObject(parent)
    , m_database(database)
{
    m_autoScanTimer.setInterval(1500);
    m_autoScanTimer.setSingleShot(true);
    connect(&m_autoScanTimer, &QTimer::timeout, this, &LibraryController::runAutoScan);
    connect(&m_fileWatcher, &QFileSystemWatcher::directoryChanged, this, &LibraryController::scheduleAutoScan);
    connect(&m_fileWatcher, &QFileSystemWatcher::fileChanged, this, &LibraryController::scheduleAutoScan);
    m_scanWorkTimer.setInterval(0);
    connect(&m_scanWorkTimer, &QTimer::timeout, this, &LibraryController::processScanBatch);

    loadRoots();
    refreshFileWatchers();
}

QStringList LibraryController::roots() const
{
    return m_roots;
}

QString LibraryController::scanStatus() const
{
    return m_scanStatus;
}

int LibraryController::totalFiles() const
{
    return m_totalFiles;
}

int LibraryController::audioFiles() const
{
    return m_audioFiles;
}

int LibraryController::failedFiles() const
{
    return m_failedFiles;
}

bool LibraryController::isScanning() const
{
    return m_scanStatus == QStringLiteral("scanning");
}

void LibraryController::requestAddFolder()
{
    const QString path = QFileDialog::getExistingDirectory(
        nullptr,
        tr("Add Music Folder"),
        QDir::homePath());

    if (!path.isEmpty()) {
        addRoot(path);
    }
}

void LibraryController::addRoot(const QString& path)
{
    const QString cleaned = QDir::cleanPath(path);
    if (cleaned.isEmpty() || m_roots.contains(cleaned) || !QDir(cleaned).exists()) {
        return;
    }

    if (!persistRoot(cleaned)) {
        return;
    }

    m_roots.append(cleaned);
    emit rootsChanged();
    refreshFileWatchers();
    scanRoot(cleaned);
}

void LibraryController::removeRoot(const QString& path)
{
    const QString cleaned = QDir::cleanPath(path);
    if (cleaned.isEmpty()) {
        return;
    }

    LibraryRepository repository(m_database);
    if (!repository.removeTracksUnderRoot(cleaned)) {
        return;
    }

    if (!deleteRoot(cleaned)) {
        return;
    }

    if (m_roots.removeAll(cleaned) == 0) {
        return;
    }

    emit rootsChanged();
    refreshFileWatchers();
    emit libraryChanged();
    if (m_roots.isEmpty()) {
        m_totalFiles = 0;
        m_audioFiles = 0;
        m_failedFiles = 0;
        emit scanSummaryChanged();
        setScanStatus(QStringLiteral("idle"));
    }
}

void LibraryController::scanRoot(const QString& path)
{
    if (!QDir(path).exists()) {
        m_failedFiles = 1;
        emit scanSummaryChanged();
        setScanStatus(QStringLiteral("error"));
        return;
    }

    startScanQueue(QStringList { QDir(path).absolutePath() });
}

void LibraryController::scanAll()
{
    if (m_roots.isEmpty()) {
        m_totalFiles = 0;
        m_audioFiles = 0;
        m_failedFiles = 0;
        emit scanSummaryChanged();
        setScanStatus(QStringLiteral("idle"));
        return;
    }

    startScanQueue(m_roots);
}

void LibraryController::cancelScan()
{
    if (m_scanStatus == QStringLiteral("scanning")) {
        m_cancelRequested = true;
    }
}

void LibraryController::rebuildLibrary()
{
    if (m_scanStatus == QStringLiteral("scanning")) {
        return;
    }

    LibraryRepository repository(m_database);
    if (!repository.clearLibraryIndex()) {
        setScanStatus(QStringLiteral("error"));
        return;
    }

    m_totalFiles = 0;
    m_audioFiles = 0;
    m_failedFiles = 0;
    emit scanSummaryChanged();
    emit libraryChanged();
    scanAll();
}

void LibraryController::loadRoots()
{
    if (!m_database || !m_database->isOpen()) {
        return;
    }

    QSqlQuery query(m_database->connection());
    if (!query.exec(QStringLiteral("SELECT path FROM library_root WHERE enabled = 1 ORDER BY path"))) {
        qWarning() << "Failed to load library roots" << query.lastError().text();
        return;
    }

    QStringList roots;
    while (query.next()) {
        roots.append(query.value(0).toString());
    }

    m_roots = roots;
    emit rootsChanged();
}

bool LibraryController::persistRoot(const QString& path)
{
    if (!m_database || !m_database->isOpen()) {
        return true;
    }

    QSqlQuery query(m_database->connection());
    query.prepare(QStringLiteral(
        "INSERT INTO library_root(path, enabled, created_at, updated_at) "
        "VALUES(:path, 1, strftime('%s','now'), strftime('%s','now')) "
        "ON CONFLICT(path) DO UPDATE SET enabled = 1, updated_at = strftime('%s','now')"));
    query.bindValue(QStringLiteral(":path"), path);

    if (!query.exec()) {
        qWarning() << "Failed to persist library root" << query.lastError().text();
        return false;
    }

    return true;
}

bool LibraryController::deleteRoot(const QString& path)
{
    if (!m_database || !m_database->isOpen()) {
        return true;
    }

    QSqlQuery query(m_database->connection());
    query.prepare(QStringLiteral(
        "UPDATE library_root SET enabled = 0, updated_at = strftime('%s','now') WHERE path = :path"));
    query.bindValue(QStringLiteral(":path"), path);

    if (!query.exec()) {
        qWarning() << "Failed to disable library root" << query.lastError().text();
        return false;
    }

    return true;
}

void LibraryController::startScanQueue(const QStringList& roots)
{
    if (m_scanStatus == QStringLiteral("scanning")) {
        qInfo() << "Library scan already running; ignoring new scan request";
        return;
    }

    QStringList existingRoots;
    for (const QString& root : roots) {
        const QFileInfo info(root);
        if (info.exists() && info.isDir()) {
            existingRoots.append(info.absoluteFilePath());
        }
    }

    m_totalFiles = 0;
    m_audioFiles = 0;
    m_failedFiles = existingRoots.size() == roots.size() ? 0 : roots.size() - existingRoots.size();
    emit scanSummaryChanged();

    if (existingRoots.isEmpty()) {
        setScanStatus(QStringLiteral("error"));
        return;
    }

    resetScanState();
    m_pendingScanRoots = existingRoots;
    m_cancelRequested = false;
    m_metadataReader = createMetadataReader();
    setScanStatus(QStringLiteral("scanning"));
    startNextScanRoot();
}

void LibraryController::startNextScanRoot()
{
    if (m_cancelRequested) {
        finishScanQueue(QStringLiteral("cancelled"));
        return;
    }

    if (m_pendingScanRoots.isEmpty()) {
        finishScanQueue(QStringLiteral("completed"));
        return;
    }

    m_currentScanRoot = m_pendingScanRoots.takeFirst();
    m_currentRootSummary = ScanSummary {};
    m_currentRootSkippedUnchangedFiles = 0;
    m_currentRootAudioPaths.clear();
    m_currentRootFileStamps.clear();
    if (m_database && m_database->isOpen()) {
        LibraryRepository repository(m_database);
        m_currentRootFileStamps = repository.trackFileStampsUnderRoot(m_currentScanRoot);
    }

    m_scanIterator = std::make_unique<QDirIterator>(
        m_currentScanRoot,
        QDir::Files | QDir::Readable | QDir::NoSymLinks,
        QDirIterator::Subdirectories);

    m_scanTransactionStarted = false;
    if (m_database && m_database->isOpen()) {
        QSqlDatabase database = m_database->connection();
        m_scanTransactionStarted = database.transaction();
        if (!m_scanTransactionStarted) {
            qWarning() << "Failed to start media library scan transaction" << database.lastError().text();
            ++m_currentRootSummary.failedFiles;
        }
    }

    qInfo() << "Started library scan root" << m_currentScanRoot;
    m_scanWorkTimer.start();
}

void LibraryController::processScanBatch()
{
    if (m_scanStatus != QStringLiteral("scanning")) {
        m_scanWorkTimer.stop();
        return;
    }

    if (m_cancelRequested) {
        finishScanQueue(QStringLiteral("cancelled"));
        return;
    }

    if (!m_scanIterator) {
        startNextScanRoot();
        return;
    }

    QElapsedTimer elapsed;
    elapsed.start();
    int processed = 0;
    LibraryRepository repository(m_database);

    while (m_scanIterator && m_scanIterator->hasNext()) {
        if (m_cancelRequested) {
            finishScanQueue(QStringLiteral("cancelled"));
            return;
        }

        const QString filePath = m_scanIterator->next();
        ++m_currentRootSummary.totalFiles;
        ++m_totalFiles;

        if (LibraryScanner::isSupportedAudioFile(filePath)) {
            ++m_currentRootSummary.audioFiles;
            ++m_audioFiles;
            const QFileInfo fileInfo(filePath);
            const QString absolutePath = fileInfo.absoluteFilePath();
            m_currentRootAudioPaths.insert(absolutePath);

            bool isUnchanged = false;
            const auto stamp = m_currentRootFileStamps.constFind(absolutePath);
            if (stamp != m_currentRootFileStamps.cend()) {
                isUnchanged = stamp->fileSize == fileInfo.size()
                    && stamp->modifiedAt == fileInfo.lastModified().toSecsSinceEpoch();
            }

            if (isUnchanged) {
                ++m_currentRootSkippedUnchangedFiles;
            } else if (m_metadataReader && m_database && m_database->isOpen()) {
                const TrackMetadata metadata = m_metadataReader->read(absolutePath.toStdString());
                if (metadata.readStatus == MetadataReadStatus::Failed) {
                    ++m_currentRootSummary.failedFiles;
                    ++m_failedFiles;
                }
                if (!repository.upsertTrack(absolutePath, metadata)) {
                    ++m_currentRootSummary.failedFiles;
                    ++m_failedFiles;
                }
            }
        }

        ++processed;
        if (processed >= 12 || elapsed.elapsed() >= 35) {
            emit scanSummaryChanged();
            m_scanWorkTimer.start();
            return;
        }
    }

    finishCurrentScanRoot();
    emit scanSummaryChanged();
    startNextScanRoot();
}

void LibraryController::finishCurrentScanRoot()
{
    if (m_currentScanRoot.isEmpty()) {
        return;
    }

    LibraryRepository repository(m_database);
    QSqlDatabase database = m_database && m_database->isOpen()
        ? m_database->connection()
        : QSqlDatabase();

    if (m_scanTransactionStarted && database.isValid() && !database.commit()) {
        qWarning() << "Failed to commit media library scan transaction" << database.lastError().text();
        ++m_currentRootSummary.failedFiles;
        ++m_failedFiles;
    }

    m_scanTransactionStarted = false;
    if (!m_cancelRequested && m_database && m_database->isOpen()
        && !repository.removeMissingTracksUnderRoot(m_currentScanRoot, m_currentRootAudioPaths)) {
        ++m_currentRootSummary.failedFiles;
        ++m_failedFiles;
    }

    qInfo() << "Scanned library root" << m_currentScanRoot
            << "total files:" << m_currentRootSummary.totalFiles
            << "audio files:" << m_currentRootSummary.audioFiles
            << "unchanged files skipped:" << m_currentRootSkippedUnchangedFiles
            << "failed files:" << m_currentRootSummary.failedFiles;

    m_scanIterator.reset();
    m_currentRootAudioPaths.clear();
    m_currentRootFileStamps.clear();
    m_currentRootSummary = ScanSummary {};
    m_currentRootSkippedUnchangedFiles = 0;
    m_currentScanRoot.clear();
}

void LibraryController::finishScanQueue(const QString& status)
{
    m_scanWorkTimer.stop();

    if (m_scanTransactionStarted && m_database && m_database->isOpen()) {
        QSqlDatabase database = m_database->connection();
        database.rollback();
        m_scanTransactionStarted = false;
    }

    if (!m_currentScanRoot.isEmpty() && status != QStringLiteral("cancelled")) {
        finishCurrentScanRoot();
    }

    resetScanState();
    emit scanSummaryChanged();
    emit libraryChanged();
    setScanStatus(status);
    if (status == QStringLiteral("completed")) {
        refreshFileWatchers();
    }
}

void LibraryController::resetScanState()
{
    m_scanIterator.reset();
    m_metadataReader.reset();
    m_pendingScanRoots.clear();
    m_currentScanRoot.clear();
    m_currentRootAudioPaths.clear();
    m_currentRootFileStamps.clear();
    m_currentRootSummary = ScanSummary {};
    m_currentRootSkippedUnchangedFiles = 0;
    m_scanTransactionStarted = false;
}

void LibraryController::refreshFileWatchers()
{
    const QStringList watchedFiles = m_fileWatcher.files();
    if (!watchedFiles.isEmpty()) {
        m_fileWatcher.removePaths(watchedFiles);
    }

    const QStringList watchedDirectories = m_fileWatcher.directories();
    if (!watchedDirectories.isEmpty()) {
        m_fileWatcher.removePaths(watchedDirectories);
    }

    QStringList paths;
    for (const QString& root : m_roots) {
        const QFileInfo rootInfo(root);
        if (!rootInfo.exists() || !rootInfo.isDir()) {
            continue;
        }

        paths.append(rootInfo.absoluteFilePath());
    }

    paths.removeDuplicates();
    const QStringList nextPaths = paths;
    if (!nextPaths.isEmpty()) {
        const QStringList failed = m_fileWatcher.addPaths(nextPaths);
        if (!failed.isEmpty()) {
            qInfo() << "File watcher skipped paths" << failed.size();
        }
        qInfo() << "Watching library roots" << (nextPaths.size() - failed.size()) << "of" << nextPaths.size();
    }
}

void LibraryController::scheduleAutoScan(const QString& changedPath)
{
    Q_UNUSED(changedPath);
    if (m_roots.isEmpty()) {
        return;
    }

    m_autoScanPending = true;
    m_autoScanTimer.start();
}

void LibraryController::runAutoScan()
{
    if (!m_autoScanPending) {
        return;
    }
    if (m_scanStatus == QStringLiteral("scanning")) {
        m_autoScanTimer.start();
        return;
    }

    m_autoScanPending = false;
    qInfo() << "Running automatic incremental library scan";
    scanAll();
}

void LibraryController::setScanStatus(const QString& scanStatus)
{
    if (m_scanStatus == scanStatus) {
        return;
    }

    m_scanStatus = scanStatus;
    emit scanStatusChanged();
}

} // namespace opusora
