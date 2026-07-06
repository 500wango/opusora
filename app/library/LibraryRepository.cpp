#include "library/LibraryRepository.hpp"

#include "core/AppPaths.hpp"
#include "core/TextEncoding.hpp"
#include "db/Database.hpp"

#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QImageReader>
#include <QSet>
#include <QSize>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUrl>
#include <QVariant>

#include <algorithm>

namespace {

QString fromStd(const std::string& value)
{
    return opusora::metadataStringFromUtf8(value).trimmed();
}

QString displayText(const QVariant& value)
{
    return opusora::repairMojibake(value.toString()).trimmed();
}

QString storedMetadataText(const QVariant& value)
{
    return opusora::repairMojibake(value.toString()).trimmed();
}

QString usableStoredMetadataText(const QVariant& value)
{
    const QString repaired = storedMetadataText(value);
    return opusora::hasLikelyEncodingDamage(repaired) ? QString() : repaired;
}

QString readStatusText(opusora::MetadataReadStatus status)
{
    switch (status) {
    case opusora::MetadataReadStatus::Pending:
        return QStringLiteral("Pending");
    case opusora::MetadataReadStatus::Reading:
        return QStringLiteral("Reading");
    case opusora::MetadataReadStatus::Success:
        return QStringLiteral("Success");
    case opusora::MetadataReadStatus::Partial:
        return QStringLiteral("Partial");
    case opusora::MetadataReadStatus::Failed:
        return QStringLiteral("Failed");
    }

    return QStringLiteral("Failed");
}

QString encodingStatusText(opusora::MetadataEncodingStatus status)
{
    switch (status) {
    case opusora::MetadataEncodingStatus::Normal:
        return QStringLiteral("Normal");
    case opusora::MetadataEncodingStatus::SuspectedEncodingIssue:
        return QStringLiteral("SuspectedEncodingIssue");
    case opusora::MetadataEncodingStatus::Unknown:
        return QStringLiteral("Unknown");
    }

    return QStringLiteral("Unknown");
}

QString fallbackTitle(const QString& filePath, const QString& title)
{
    return title.isEmpty() ? QFileInfo(filePath).completeBaseName() : title;
}

QString fallbackFormat(const QString& filePath, const QString& format)
{
    return format.isEmpty() ? QFileInfo(filePath).suffix().toUpper() : format;
}

QString fileHash(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to hash audio file" << filePath << file.errorString();
        return {};
    }

    QCryptographicHash hash(QCryptographicHash::Sha1);
    while (!file.atEnd()) {
        const QByteArray chunk = file.read(1024 * 1024);
        if (chunk.isEmpty() && file.error() != QFile::NoError) {
            qWarning() << "Failed reading audio file for hash" << filePath << file.errorString();
            return {};
        }
        hash.addData(chunk);
    }

    return QString::fromLatin1(hash.result().toHex());
}

QList<QString> unhashedSameSizeTracks(QSqlDatabase& database, qint64 fileSize, const QString& excludedFilePath, bool* hasSameSizeTrack)
{
    if (hasSameSizeTrack) {
        *hasSameSizeTrack = false;
    }

    QList<QString> filePaths;
    if (fileSize < 0) {
        return filePaths;
    }

    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "SELECT file_path, file_hash FROM track "
        "WHERE file_size = :file_size AND file_path != :file_path"));
    query.bindValue(QStringLiteral(":file_size"), fileSize);
    query.bindValue(QStringLiteral(":file_path"), excludedFilePath);
    if (!query.exec()) {
        qWarning() << "Failed to load duplicate hash candidates" << query.lastError().text();
        return filePaths;
    }

    while (query.next()) {
        if (hasSameSizeTrack) {
            *hasSameSizeTrack = true;
        }

        const QString filePath = query.value(0).toString();
        const QString hash = query.value(1).toString().trimmed();
        if (!filePath.isEmpty() && hash.isEmpty()) {
            filePaths.append(filePath);
        }
    }

    return filePaths;
}

void updateTrackHashIfMissing(QSqlDatabase& database, const QString& filePath, const QString& hash)
{
    if (filePath.isEmpty() || hash.isEmpty()) {
        return;
    }

    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "UPDATE track SET file_hash = :file_hash, updated_at = strftime('%s','now') "
        "WHERE file_path = :file_path AND TRIM(IFNULL(file_hash, '')) = ''"));
    query.bindValue(QStringLiteral(":file_hash"), hash);
    query.bindValue(QStringLiteral(":file_path"), filePath);
    if (!query.exec()) {
        qWarning() << "Failed to update deferred track hash" << filePath << query.lastError().text();
    }
}

QString escapeLikePattern(const QString& value)
{
    QString escaped;
    escaped.reserve(value.size());
    for (const QChar character : value) {
        if (character == QLatin1Char('\\') || character == QLatin1Char('%') || character == QLatin1Char('_')) {
            escaped.append(QLatin1Char('\\'));
        }
        escaped.append(character);
    }
    return escaped;
}

QString coverFileExtension(const QString& mimeType)
{
    const QString normalized = mimeType.toLower();
    if (normalized.contains(QStringLiteral("jpeg")) || normalized.contains(QStringLiteral("jpg"))) {
        return QStringLiteral("jpg");
    }
    if (normalized.contains(QStringLiteral("png"))) {
        return QStringLiteral("png");
    }
    if (normalized.contains(QStringLiteral("webp"))) {
        return QStringLiteral("webp");
    }
    return QStringLiteral("bin");
}

QString coverUrlFromPath(const QString& imagePath)
{
    return imagePath.trimmed().isEmpty() ? QString() : QUrl::fromLocalFile(imagePath).toString();
}

QString trackOrderByClause(const QString& sortOrder)
{
    if (sortOrder == QStringLiteral("artist")) {
        return QStringLiteral(
            "ORDER BY track.display_artist COLLATE NOCASE, track.title COLLATE NOCASE, track.file_path COLLATE NOCASE");
    }
    if (sortOrder == QStringLiteral("album")) {
        return QStringLiteral(
            "ORDER BY track.display_album COLLATE NOCASE, IFNULL(track.disc_number, 0), "
            "IFNULL(track.track_number, 0), track.title COLLATE NOCASE, track.file_path COLLATE NOCASE");
    }
    if (sortOrder == QStringLiteral("recent")) {
        return QStringLiteral(
            "ORDER BY track.added_at DESC, track.title COLLATE NOCASE, track.file_path COLLATE NOCASE");
    }
    if (sortOrder == QStringLiteral("duration")) {
        return QStringLiteral(
            "ORDER BY track.duration_ms DESC, track.title COLLATE NOCASE, track.file_path COLLATE NOCASE");
    }
    return QStringLiteral("ORDER BY track.title COLLATE NOCASE, track.file_path COLLATE NOCASE");
}

QString trackSelectColumns()
{
    return QStringLiteral(
        "track.id, track.title, track.display_artist, track.display_album, track.file_path, "
        "cover.image_path, track.format, track.lyrics_text, track.duration_ms, track.track_number, track.disc_number, "
        "track.replaygain_track_gain, track.replaygain_album_gain ");
}

opusora::TrackRow readTrackRow(const QSqlQuery& query)
{
    opusora::TrackRow row;
    row.id = query.value(0).toInt();
    row.title = displayText(query.value(1));
    row.artist = displayText(query.value(2));
    row.album = displayText(query.value(3));
    row.filePath = query.value(4).toString();
    row.coverUrl = coverUrlFromPath(query.value(5).toString());
    row.format = displayText(query.value(6));
    row.lyricsText = displayText(query.value(7));
    row.durationMs = query.value(8).toInt();
    row.trackNumber = query.value(9).toInt();
    row.discNumber = query.value(10).toInt();
    if (!query.value(11).isNull()) {
        row.replayGainTrackGainDb = query.value(11).toDouble();
        row.hasReplayGainTrackGain = true;
    }
    if (!query.value(12).isNull()) {
        row.replayGainAlbumGainDb = query.value(12).toDouble();
        row.hasReplayGainAlbumGain = true;
    }
    return row;
}

bool cleanupOrphanRows(QSqlDatabase& database)
{
    QSqlQuery deleteOrphanAlbums(database);
    if (!deleteOrphanAlbums.exec(QStringLiteral(
            "DELETE FROM album "
            "WHERE id NOT IN (SELECT DISTINCT album_id FROM track WHERE album_id IS NOT NULL)"))) {
        qWarning() << "Failed to remove orphan albums" << deleteOrphanAlbums.lastError().text();
        return false;
    }

    QSqlQuery deleteOrphanArtists(database);
    if (!deleteOrphanArtists.exec(QStringLiteral(
            "DELETE FROM artist "
            "WHERE id NOT IN (SELECT DISTINCT artist_id FROM track_artist) "
            "AND id NOT IN (SELECT DISTINCT album_artist_id FROM album WHERE album_artist_id IS NOT NULL)"))) {
        qWarning() << "Failed to remove orphan artists" << deleteOrphanArtists.lastError().text();
        return false;
    }

    QSqlQuery clearOrphanCoverSources(database);
    if (!clearOrphanCoverSources.exec(QStringLiteral(
            "UPDATE cover SET source_track_id = NULL "
            "WHERE source_track_id IS NOT NULL "
            "AND source_track_id NOT IN (SELECT id FROM track)"))) {
        qWarning() << "Failed to clear orphan cover sources" << clearOrphanCoverSources.lastError().text();
        return false;
    }

    QSqlQuery deleteOrphanCovers(database);
    if (!deleteOrphanCovers.exec(QStringLiteral(
            "DELETE FROM cover "
            "WHERE id NOT IN (SELECT DISTINCT cover_id FROM track WHERE cover_id IS NOT NULL) "
            "AND id NOT IN (SELECT DISTINCT cover_id FROM album WHERE cover_id IS NOT NULL)"))) {
        qWarning() << "Failed to remove orphan covers" << deleteOrphanCovers.lastError().text();
        return false;
    }

    return true;
}

} // namespace

namespace opusora {

LibraryRepository::LibraryRepository(Database* database)
    : m_database(database)
{
}

QHash<QString, TrackFileStamp> LibraryRepository::trackFileStampsUnderRoot(const QString& rootPath) const
{
    QHash<QString, TrackFileStamp> stamps;
    if (!m_database || !m_database->isOpen()) {
        return stamps;
    }

    const QString cleanedRoot = QDir::cleanPath(rootPath);
    if (cleanedRoot.isEmpty()) {
        return stamps;
    }

    QSqlQuery query(m_database->connection());
    query.prepare(QStringLiteral(
        "SELECT file_path, file_size, modified_at FROM track "
        "WHERE file_path = :root_path OR file_path LIKE :root_prefix ESCAPE '\\'"));
    query.bindValue(QStringLiteral(":root_path"), cleanedRoot);
    query.bindValue(QStringLiteral(":root_prefix"), escapeLikePattern(cleanedRoot + QLatin1Char('/')) + QStringLiteral("%"));

    if (!query.exec()) {
        qWarning() << "Failed to load track file stamps under root" << cleanedRoot << query.lastError().text();
        return stamps;
    }

    while (query.next()) {
        const QString filePath = query.value(0).toString();
        if (filePath.isEmpty()) {
            continue;
        }

        TrackFileStamp stamp;
        if (!query.value(1).isNull()) {
            stamp.fileSize = query.value(1).toLongLong();
        }
        if (!query.value(2).isNull()) {
            stamp.modifiedAt = query.value(2).toLongLong();
        }
        stamps.insert(filePath, stamp);
    }

    return stamps;
}

bool LibraryRepository::upsertTrack(const QString& filePath, const TrackMetadata& metadata)
{
    if (!m_database || !m_database->isOpen()) {
        return false;
    }

    const QFileInfo fileInfo(filePath);
    const QString absoluteFilePath = fileInfo.absoluteFilePath();
    const qint64 fileSize = fileInfo.size();
    QSqlDatabase database = m_database->connection();
    const QString title = fallbackTitle(filePath, fromStd(metadata.title));
    const QString artist = fromStd(metadata.artist);
    const QString album = fromStd(metadata.album);
    const QString albumArtist = fromStd(metadata.albumArtist).isEmpty() ? artist : fromStd(metadata.albumArtist);
    const QString genre = fromStd(metadata.genre);
    const QString releaseDate = fromStd(metadata.releaseDate);
    const QString format = fallbackFormat(filePath, fromStd(metadata.format));
    bool hasSameSizeTrack = false;
    const QList<QString> candidatePaths = unhashedSameSizeTracks(database, fileSize, absoluteFilePath, &hasSameSizeTrack);
    QString hash;
    if (hasSameSizeTrack) {
        hash = fileHash(absoluteFilePath);
        for (const QString& candidatePath : candidatePaths) {
            const QFileInfo candidateInfo(candidatePath);
            if (!candidateInfo.exists() || !candidateInfo.isFile() || candidateInfo.size() != fileSize) {
                continue;
            }

            updateTrackHashIfMissing(database, candidatePath, fileHash(candidateInfo.absoluteFilePath()));
        }
    }

    const int artistId = upsertArtist(artist.isEmpty() ? QStringLiteral("Unknown Artist") : artist);
    const int albumArtistId = upsertArtist(albumArtist.isEmpty() ? QStringLiteral("Unknown Artist") : albumArtist);
    const int albumId = upsertAlbum(album.isEmpty() ? QStringLiteral("Unknown Album") : album, albumArtistId, albumArtist);

    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "INSERT INTO track("
        "file_path, file_hash, title, display_artist, display_album, display_album_artist, "
        "album_id, genre, release_date, track_number, disc_number, duration_ms, "
        "bitrate, sample_rate, channels, format, file_size, modified_at, added_at, "
        "composer, comment, bpm, replaygain_track_gain, replaygain_album_gain, lyrics_text, "
        "metadata_read_status, metadata_encoding_status, created_at, updated_at"
        ") VALUES ("
        ":file_path, :file_hash, :title, :display_artist, :display_album, :display_album_artist, "
        ":album_id, :genre, :release_date, :track_number, :disc_number, :duration_ms, "
        ":bitrate, :sample_rate, :channels, :format, :file_size, :modified_at, strftime('%s','now'), "
        ":composer, :comment, :bpm, :replaygain_track_gain, :replaygain_album_gain, :lyrics_text, "
        ":metadata_read_status, :metadata_encoding_status, strftime('%s','now'), strftime('%s','now')"
        ") ON CONFLICT(file_path) DO UPDATE SET "
        "file_hash = excluded.file_hash, title = excluded.title, display_artist = excluded.display_artist, display_album = excluded.display_album, "
        "display_album_artist = excluded.display_album_artist, album_id = excluded.album_id, genre = excluded.genre, "
        "release_date = excluded.release_date, track_number = excluded.track_number, disc_number = excluded.disc_number, "
        "duration_ms = excluded.duration_ms, bitrate = excluded.bitrate, sample_rate = excluded.sample_rate, "
        "channels = excluded.channels, format = excluded.format, file_size = excluded.file_size, modified_at = excluded.modified_at, "
        "composer = excluded.composer, comment = excluded.comment, bpm = excluded.bpm, "
        "replaygain_track_gain = excluded.replaygain_track_gain, replaygain_album_gain = excluded.replaygain_album_gain, "
        "lyrics_text = CASE "
        "WHEN TRIM(IFNULL(track.lyrics_text, '')) = '' AND TRIM(IFNULL(excluded.lyrics_text, '')) != '' THEN excluded.lyrics_text "
        "ELSE track.lyrics_text END, "
        "metadata_read_status = excluded.metadata_read_status, metadata_encoding_status = excluded.metadata_encoding_status, "
        "updated_at = strftime('%s','now')"));
    query.bindValue(QStringLiteral(":file_path"), absoluteFilePath);
    query.bindValue(QStringLiteral(":file_hash"), hash);
    query.bindValue(QStringLiteral(":title"), title);
    query.bindValue(QStringLiteral(":display_artist"), artist);
    query.bindValue(QStringLiteral(":display_album"), album);
    query.bindValue(QStringLiteral(":display_album_artist"), albumArtist);
    query.bindValue(QStringLiteral(":album_id"), albumId > 0 ? QVariant(albumId) : QVariant());
    query.bindValue(QStringLiteral(":genre"), genre);
    query.bindValue(QStringLiteral(":release_date"), releaseDate);
    query.bindValue(QStringLiteral(":track_number"), metadata.trackNumber);
    query.bindValue(QStringLiteral(":disc_number"), metadata.discNumber);
    query.bindValue(QStringLiteral(":duration_ms"), metadata.durationMs);
    query.bindValue(QStringLiteral(":bitrate"), metadata.bitrate);
    query.bindValue(QStringLiteral(":sample_rate"), metadata.sampleRate);
    query.bindValue(QStringLiteral(":channels"), metadata.channels);
    query.bindValue(QStringLiteral(":format"), format);
    query.bindValue(QStringLiteral(":file_size"), fileSize);
    query.bindValue(QStringLiteral(":modified_at"), fileInfo.lastModified().toSecsSinceEpoch());
    query.bindValue(QStringLiteral(":composer"), fromStd(metadata.composer));
    query.bindValue(QStringLiteral(":comment"), fromStd(metadata.comment));
    query.bindValue(QStringLiteral(":bpm"), metadata.bpm);
    query.bindValue(
        QStringLiteral(":replaygain_track_gain"),
        metadata.hasReplayGainTrackGain ? QVariant(metadata.replayGainTrackGainDb) : QVariant());
    query.bindValue(
        QStringLiteral(":replaygain_album_gain"),
        metadata.hasReplayGainAlbumGain ? QVariant(metadata.replayGainAlbumGainDb) : QVariant());
    query.bindValue(QStringLiteral(":lyrics_text"), fromStd(metadata.lyrics));
    query.bindValue(QStringLiteral(":metadata_read_status"), readStatusText(metadata.readStatus));
    query.bindValue(QStringLiteral(":metadata_encoding_status"), encodingStatusText(metadata.encodingStatus));

    if (!query.exec()) {
        qWarning() << "Failed to upsert track" << filePath << query.lastError().text();
        return false;
    }

    QSqlQuery idQuery(database);
    idQuery.prepare(QStringLiteral("SELECT id FROM track WHERE file_path = :file_path"));
    idQuery.bindValue(QStringLiteral(":file_path"), absoluteFilePath);
    if (idQuery.exec() && idQuery.next()) {
        const int trackId = idQuery.value(0).toInt();
        if (artistId > 0) {
            upsertTrackArtist(trackId, artistId);
        }

        if (metadata.cover.has_value()) {
            const int coverId = upsertCover(trackId, metadata.cover.value());
            if (coverId > 0) {
                QSqlQuery updateCover(m_database->connection());
                updateCover.prepare(QStringLiteral(
                    "UPDATE track SET cover_id = :cover_id, updated_at = strftime('%s','now') WHERE id = :track_id"));
                updateCover.bindValue(QStringLiteral(":cover_id"), coverId);
                updateCover.bindValue(QStringLiteral(":track_id"), trackId);
                if (!updateCover.exec()) {
                    qWarning() << "Failed to update track cover" << trackId << updateCover.lastError().text();
                }

                QSqlQuery updateAlbumCover(m_database->connection());
                updateAlbumCover.prepare(QStringLiteral(
                    "UPDATE album SET cover_id = :cover_id, updated_at = strftime('%s','now') "
                    "WHERE id = :album_id AND cover_id IS NULL"));
                updateAlbumCover.bindValue(QStringLiteral(":cover_id"), coverId);
                updateAlbumCover.bindValue(QStringLiteral(":album_id"), albumId);
                if (!updateAlbumCover.exec()) {
                    qWarning() << "Failed to update album cover" << albumId << updateAlbumCover.lastError().text();
                }
            }
        }
    }

    return true;
}

bool LibraryRepository::removeTracksUnderRoot(const QString& rootPath)
{
    if (!m_database || !m_database->isOpen()) {
        return false;
    }

    const QString cleanedRoot = QDir::cleanPath(rootPath);
    if (cleanedRoot.isEmpty()) {
        return false;
    }

    QSqlDatabase database = m_database->connection();
    const bool hasTransaction = database.transaction();

    QSqlQuery clearCoverSources(database);
    clearCoverSources.prepare(QStringLiteral(
        "UPDATE cover SET source_track_id = NULL "
        "WHERE source_track_id IN ("
        "SELECT id FROM track WHERE file_path = :root_path OR file_path LIKE :root_prefix ESCAPE '\\')"));
    clearCoverSources.bindValue(QStringLiteral(":root_path"), cleanedRoot);
    clearCoverSources.bindValue(QStringLiteral(":root_prefix"), escapeLikePattern(cleanedRoot + QLatin1Char('/')) + QStringLiteral("%"));
    if (!clearCoverSources.exec()) {
        qWarning() << "Failed to clear cover sources under library root" << cleanedRoot << clearCoverSources.lastError().text();
        if (hasTransaction) {
            database.rollback();
        }
        return false;
    }

    QSqlQuery deleteTracks(database);
    deleteTracks.prepare(QStringLiteral(
        "DELETE FROM track "
        "WHERE file_path = :root_path OR file_path LIKE :root_prefix ESCAPE '\\'"));
    deleteTracks.bindValue(QStringLiteral(":root_path"), cleanedRoot);
    deleteTracks.bindValue(QStringLiteral(":root_prefix"), escapeLikePattern(cleanedRoot + QLatin1Char('/')) + QStringLiteral("%"));
    if (!deleteTracks.exec()) {
        qWarning() << "Failed to remove tracks under library root" << cleanedRoot << deleteTracks.lastError().text();
        if (hasTransaction) {
            database.rollback();
        }
        return false;
    }

    if (!cleanupOrphanRows(database)) {
        if (hasTransaction) {
            database.rollback();
        }
        return false;
    }

    if (hasTransaction && !database.commit()) {
        qWarning() << "Failed to commit library root removal cleanup" << database.lastError().text();
        return false;
    }

    return true;
}

bool LibraryRepository::removeMissingTracksUnderRoot(const QString& rootPath, const QSet<QString>& existingFilePaths)
{
    if (!m_database || !m_database->isOpen()) {
        return false;
    }

    const QString cleanedRoot = QDir::cleanPath(rootPath);
    if (cleanedRoot.isEmpty()) {
        return false;
    }

    QList<int> missingTrackIds;
    QSqlQuery select(m_database->connection());
    select.prepare(QStringLiteral(
        "SELECT id, file_path FROM track "
        "WHERE file_path = :root_path OR file_path LIKE :root_prefix ESCAPE '\\'"));
    select.bindValue(QStringLiteral(":root_path"), cleanedRoot);
    select.bindValue(QStringLiteral(":root_prefix"), escapeLikePattern(cleanedRoot + QLatin1Char('/')) + QStringLiteral("%"));
    if (!select.exec()) {
        qWarning() << "Failed to load tracks for incremental cleanup" << cleanedRoot << select.lastError().text();
        return false;
    }

    while (select.next()) {
        const QString filePath = select.value(1).toString();
        if (!existingFilePaths.contains(filePath)) {
            missingTrackIds.append(select.value(0).toInt());
        }
    }

    if (missingTrackIds.isEmpty()) {
        return true;
    }

    QSqlDatabase database = m_database->connection();
    const bool hasTransaction = database.transaction();

    QSqlQuery clearCover(database);
    QSqlQuery deleteTrack(database);
    clearCover.prepare(QStringLiteral("UPDATE cover SET source_track_id = NULL WHERE source_track_id = :track_id"));
    deleteTrack.prepare(QStringLiteral("DELETE FROM track WHERE id = :track_id"));
    for (int trackId : missingTrackIds) {
        clearCover.bindValue(QStringLiteral(":track_id"), trackId);
        if (!clearCover.exec()) {
            qWarning() << "Failed to clear missing track cover source" << trackId << clearCover.lastError().text();
            if (hasTransaction) {
                database.rollback();
            }
            return false;
        }
        clearCover.finish();

        deleteTrack.bindValue(QStringLiteral(":track_id"), trackId);
        if (!deleteTrack.exec()) {
            qWarning() << "Failed to remove missing track" << trackId << deleteTrack.lastError().text();
            if (hasTransaction) {
                database.rollback();
            }
            return false;
        }
        deleteTrack.finish();
    }

    if (!cleanupOrphanRows(database)) {
        if (hasTransaction) {
            database.rollback();
        }
        return false;
    }

    if (hasTransaction && !database.commit()) {
        qWarning() << "Failed to commit missing track cleanup" << database.lastError().text();
        return false;
    }

    qInfo() << "Removed missing tracks under root" << cleanedRoot << missingTrackIds.size();
    return true;
}

bool LibraryRepository::clearLibraryIndex()
{
    if (!m_database || !m_database->isOpen()) {
        return false;
    }

    QSqlDatabase database = m_database->connection();
    if (!database.transaction()) {
        qWarning() << "Failed to start library rebuild cleanup" << database.lastError().text();
        return false;
    }

    const QStringList statements = {
        QStringLiteral("UPDATE track SET cover_id = NULL"),
        QStringLiteral("UPDATE album SET cover_id = NULL"),
        QStringLiteral("UPDATE cover SET source_track_id = NULL"),
        QStringLiteral("DELETE FROM play_queue"),
        QStringLiteral("DELETE FROM track_artist"),
        QStringLiteral("DELETE FROM playlist_track"),
        QStringLiteral("DELETE FROM track"),
        QStringLiteral("DELETE FROM album"),
        QStringLiteral("DELETE FROM artist"),
        QStringLiteral("DELETE FROM cover"),
    };

    for (const QString& statement : statements) {
        QSqlQuery query(database);
        if (!query.exec(statement)) {
            qWarning() << "Failed to clear library index" << statement << query.lastError().text();
            database.rollback();
            return false;
        }
    }

    if (!database.commit()) {
        qWarning() << "Failed to commit library rebuild cleanup" << database.lastError().text();
        return false;
    }

    return true;
}

int LibraryRepository::repairStoredMetadataText()
{
    if (!m_database || !m_database->isOpen()) {
        return 0;
    }

    struct StoredTrack {
        int id = 0;
        QString filePath;
        QString title;
        QString artist;
        QString album;
        QString albumArtist;
        QString genre;
        QString composer;
        QString comment;
        QString lyricsText;
        QVariant coverId;
    };

    QList<StoredTrack> tracks;
    QSqlDatabase database = m_database->connection();
    QSqlQuery select(database);
    if (!select.exec(QStringLiteral(
            "SELECT id, file_path, title, display_artist, display_album, display_album_artist, "
            "genre, composer, comment, lyrics_text, cover_id "
            "FROM track"))) {
        qWarning() << "Failed to load tracks for metadata text repair" << select.lastError().text();
        return 0;
    }

    while (select.next()) {
        StoredTrack track;
        track.id = select.value(0).toInt();
        track.filePath = select.value(1).toString();
        track.title = select.value(2).toString();
        track.artist = select.value(3).toString();
        track.album = select.value(4).toString();
        track.albumArtist = select.value(5).toString();
        track.genre = select.value(6).toString();
        track.composer = select.value(7).toString();
        track.comment = select.value(8).toString();
        track.lyricsText = select.value(9).toString();
        track.coverId = select.value(10);
        tracks.append(track);
    }

    if (tracks.isEmpty()) {
        return 0;
    }

    const bool hasTransaction = database.transaction();
    QSqlQuery updateTrack(database);
    updateTrack.prepare(QStringLiteral(
        "UPDATE track SET "
        "title = :title, display_artist = :artist, display_album = :album, "
        "display_album_artist = :album_artist, album_id = :album_id, genre = :genre, "
        "composer = :composer, comment = :comment, lyrics_text = :lyrics_text, "
        "updated_at = strftime('%s','now') "
        "WHERE id = :track_id"));

    QSqlQuery deleteArtists(database);
    deleteArtists.prepare(QStringLiteral("DELETE FROM track_artist WHERE track_id = :track_id AND role = 'primary'"));

    QSqlQuery updateAlbumCover(database);
    updateAlbumCover.prepare(QStringLiteral(
        "UPDATE album SET cover_id = COALESCE(cover_id, :cover_id), updated_at = strftime('%s','now') "
        "WHERE id = :album_id AND :cover_id IS NOT NULL"));

    int repairedCount = 0;
    for (const StoredTrack& track : tracks) {
        QString title = usableStoredMetadataText(track.title);
        if (title.isEmpty()) {
            title = QFileInfo(track.filePath).completeBaseName().trimmed();
        }
        if (title.isEmpty()) {
            title = track.filePath;
        }

        const QString artist = usableStoredMetadataText(track.artist);
        const QString album = usableStoredMetadataText(track.album);
        QString albumArtist = usableStoredMetadataText(track.albumArtist);
        if (albumArtist.isEmpty()) {
            albumArtist = artist;
        }
        const QString genre = usableStoredMetadataText(track.genre);
        const QString composer = usableStoredMetadataText(track.composer);
        const QString comment = storedMetadataText(track.comment);
        const QString lyricsText = track.lyricsText;

        const bool textChanged = title != track.title.trimmed()
            || artist != track.artist.trimmed()
            || album != track.album.trimmed()
            || albumArtist != track.albumArtist.trimmed()
            || genre != track.genre.trimmed()
            || composer != track.composer.trimmed()
            || comment != track.comment.trimmed()
            || lyricsText != track.lyricsText.trimmed();
        if (!textChanged) {
            continue;
        }

        const int artistId = upsertArtist(artist.isEmpty() ? QStringLiteral("Unknown Artist") : artist);
        const int albumArtistId = upsertArtist(albumArtist.isEmpty() ? QStringLiteral("Unknown Artist") : albumArtist);
        const int albumId = upsertAlbum(album.isEmpty() ? QStringLiteral("Unknown Album") : album, albumArtistId, albumArtist);

        updateTrack.bindValue(QStringLiteral(":title"), title);
        updateTrack.bindValue(QStringLiteral(":artist"), artist);
        updateTrack.bindValue(QStringLiteral(":album"), album);
        updateTrack.bindValue(QStringLiteral(":album_artist"), albumArtist);
        updateTrack.bindValue(QStringLiteral(":album_id"), albumId > 0 ? QVariant(albumId) : QVariant());
        updateTrack.bindValue(QStringLiteral(":genre"), genre);
        updateTrack.bindValue(QStringLiteral(":composer"), composer);
        updateTrack.bindValue(QStringLiteral(":comment"), comment);
        updateTrack.bindValue(QStringLiteral(":lyrics_text"), lyricsText);
        updateTrack.bindValue(QStringLiteral(":track_id"), track.id);
        if (!updateTrack.exec()) {
            qWarning() << "Failed to repair stored metadata text for track" << track.id << updateTrack.lastError().text();
            if (hasTransaction) {
                database.rollback();
            }
            return repairedCount;
        }
        updateTrack.finish();

        deleteArtists.bindValue(QStringLiteral(":track_id"), track.id);
        if (!deleteArtists.exec()) {
            qWarning() << "Failed to rebuild repaired track artists for track" << track.id << deleteArtists.lastError().text();
            if (hasTransaction) {
                database.rollback();
            }
            return repairedCount;
        }
        deleteArtists.finish();

        if (artistId > 0) {
            upsertTrackArtist(track.id, artistId);
        }

        if (albumId > 0 && !track.coverId.isNull()) {
            updateAlbumCover.bindValue(QStringLiteral(":cover_id"), track.coverId);
            updateAlbumCover.bindValue(QStringLiteral(":album_id"), albumId);
            if (!updateAlbumCover.exec()) {
                qWarning() << "Failed to preserve repaired album cover" << albumId << updateAlbumCover.lastError().text();
                if (hasTransaction) {
                    database.rollback();
                }
                return repairedCount;
            }
            updateAlbumCover.finish();
        }

        ++repairedCount;
    }

    if (repairedCount <= 0) {
        if (hasTransaction) {
            database.rollback();
        }
        return 0;
    }

    if (!cleanupOrphanRows(database)) {
        if (hasTransaction) {
            database.rollback();
        }
        return repairedCount;
    }

    if (hasTransaction && !database.commit()) {
        qWarning() << "Failed to commit metadata text repair" << database.lastError().text();
        return repairedCount;
    }

    if (repairedCount > 0) {
        qInfo() << "Repaired stored metadata text rows" << repairedCount;
    }
    return repairedCount;
}

bool LibraryRepository::updateTrackDetails(int trackId, const QString& title, const QString& artist, const QString& album, const QString& lyricsText)
{
    if (!m_database || !m_database->isOpen() || trackId <= 0) {
        return false;
    }

    const QString trimmedTitle = title.trimmed();
    if (trimmedTitle.isEmpty()) {
        return false;
    }

    QSqlDatabase database = m_database->connection();
    const bool hasTransaction = database.transaction();
    const QString trimmedArtist = artist.trimmed();
    const QString trimmedAlbum = album.trimmed();
    const int artistId = trimmedArtist.isEmpty() ? 0 : upsertArtist(trimmedArtist);
    const int albumId = trimmedAlbum.isEmpty() ? 0 : upsertAlbum(trimmedAlbum, artistId, trimmedArtist);

    QSqlQuery query(database);
    if (trimmedAlbum.isEmpty()) {
        query.prepare(QStringLiteral(
            "UPDATE track SET "
            "title = :title, display_artist = :artist, "
            "lyrics_text = :lyrics_text, "
            "updated_at = strftime('%s','now') "
            "WHERE id = :track_id"));
    } else {
        query.prepare(QStringLiteral(
            "UPDATE track SET "
            "title = :title, display_artist = :artist, "
            "display_album = :album, display_album_artist = :album_artist, album_id = :album_id, "
            "lyrics_text = :lyrics_text, "
            "updated_at = strftime('%s','now') "
            "WHERE id = :track_id"));
        query.bindValue(QStringLiteral(":album"), trimmedAlbum);
        query.bindValue(QStringLiteral(":album_artist"), trimmedArtist);
        query.bindValue(QStringLiteral(":album_id"), albumId > 0 ? QVariant(albumId) : QVariant());
    }
    query.bindValue(QStringLiteral(":title"), trimmedTitle);
    query.bindValue(QStringLiteral(":artist"), trimmedArtist);
    query.bindValue(QStringLiteral(":lyrics_text"), lyricsText);
    query.bindValue(QStringLiteral(":track_id"), trackId);

    if (!query.exec()) {
        qWarning() << "Failed to update track details" << trackId << query.lastError().text();
        if (hasTransaction) {
            database.rollback();
        }
        return false;
    }

    if (query.numRowsAffected() <= 0) {
        if (hasTransaction) {
            database.rollback();
        }
        return false;
    }

    QSqlQuery deleteArtists(database);
    deleteArtists.prepare(QStringLiteral("DELETE FROM track_artist WHERE track_id = :track_id AND role = 'primary'"));
    deleteArtists.bindValue(QStringLiteral(":track_id"), trackId);
    if (!deleteArtists.exec()) {
        qWarning() << "Failed to clear track primary artists" << trackId << deleteArtists.lastError().text();
        if (hasTransaction) {
            database.rollback();
        }
        return false;
    }

    if (artistId > 0) {
        upsertTrackArtist(trackId, artistId);
    }

    if (!cleanupOrphanRows(database)) {
        if (hasTransaction) {
            database.rollback();
        }
        return false;
    }

    if (hasTransaction && !database.commit()) {
        qWarning() << "Failed to commit track details update" << trackId << database.lastError().text();
        return false;
    }

    return true;
}

bool LibraryRepository::markTrackPlayed(int trackId)
{
    if (!m_database || !m_database->isOpen() || trackId <= 0) {
        return false;
    }

    QSqlQuery query(m_database->connection());
    query.prepare(QStringLiteral(
        "UPDATE track SET "
        "last_played_at = strftime('%s','now'), "
        "play_count = IFNULL(play_count, 0) + 1, "
        "updated_at = strftime('%s','now') "
        "WHERE id = :track_id"));
    query.bindValue(QStringLiteral(":track_id"), trackId);
    if (!query.exec()) {
        qWarning() << "Failed to mark track played" << trackId << query.lastError().text();
        return false;
    }

    return query.numRowsAffected() > 0;
}

QString LibraryRepository::lyricsForTrack(int trackId) const
{
    if (!m_database || !m_database->isOpen() || trackId <= 0) {
        return {};
    }

    QSqlQuery query(m_database->connection());
    query.prepare(QStringLiteral("SELECT lyrics_text FROM track WHERE id = :track_id"));
    query.bindValue(QStringLiteral(":track_id"), trackId);
    if (!query.exec()) {
        qWarning() << "Failed to load track lyrics" << trackId << query.lastError().text();
        return {};
    }

    return query.next() ? query.value(0).toString() : QString();
}

QList<TrackRow> LibraryRepository::tracks(const QString& sortOrder) const
{
    QList<TrackRow> rows;
    if (!m_database || !m_database->isOpen()) {
        return rows;
    }

    QSqlQuery query(m_database->connection());
    const QString statement = QStringLiteral("SELECT ")
        + trackSelectColumns()
        + QStringLiteral(
            "FROM track "
            "LEFT JOIN cover ON cover.id = track.cover_id ")
        + trackOrderByClause(sortOrder);
    if (!query.exec(statement)) {
        qWarning() << "Failed to load tracks" << query.lastError().text();
        return rows;
    }

    while (query.next()) {
        rows.append(readTrackRow(query));
    }

    return rows;
}

QList<TrackRow> LibraryRepository::recentlyAddedTracks(int limit) const
{
    QList<TrackRow> rows;
    if (!m_database || !m_database->isOpen()) {
        return rows;
    }

    QSqlQuery query(m_database->connection());
    query.prepare(QStringLiteral("SELECT ")
        + trackSelectColumns()
        + QStringLiteral(
        "FROM track "
        "LEFT JOIN cover ON cover.id = track.cover_id "
        "ORDER BY track.added_at DESC, track.title COLLATE NOCASE, track.file_path COLLATE NOCASE "
        "LIMIT :limit"));
    query.bindValue(QStringLiteral(":limit"), std::max(1, std::min(limit, 50)));
    if (!query.exec()) {
        qWarning() << "Failed to load recently added tracks" << query.lastError().text();
        return rows;
    }

    while (query.next()) {
        rows.append(readTrackRow(query));
    }

    return rows;
}

QList<TrackRow> LibraryRepository::recentlyPlayedTracks(int limit) const
{
    QList<TrackRow> rows;
    if (!m_database || !m_database->isOpen()) {
        return rows;
    }

    QSqlQuery query(m_database->connection());
    query.prepare(QStringLiteral("SELECT ")
        + trackSelectColumns()
        + QStringLiteral(
        "FROM track "
        "LEFT JOIN cover ON cover.id = track.cover_id "
        "WHERE track.last_played_at IS NOT NULL "
        "ORDER BY track.last_played_at DESC, track.title COLLATE NOCASE, track.file_path COLLATE NOCASE "
        "LIMIT :limit"));
    query.bindValue(QStringLiteral(":limit"), std::max(1, std::min(limit, 50)));
    if (!query.exec()) {
        qWarning() << "Failed to load recently played tracks" << query.lastError().text();
        return rows;
    }

    while (query.next()) {
        rows.append(readTrackRow(query));
    }

    return rows;
}

QList<TrackRow> LibraryRepository::duplicateTracks(int limit) const
{
    QList<TrackRow> rows;
    if (!m_database || !m_database->isOpen()) {
        return rows;
    }

    QSqlQuery query(m_database->connection());
    query.prepare(QStringLiteral("SELECT ")
        + trackSelectColumns()
        + QStringLiteral(
        "FROM track "
        "LEFT JOIN cover ON cover.id = track.cover_id "
        "WHERE TRIM(IFNULL(track.file_hash, '')) != '' "
        "AND track.file_hash IN ("
        "SELECT file_hash FROM track "
        "WHERE TRIM(IFNULL(file_hash, '')) != '' "
        "GROUP BY file_hash HAVING COUNT(*) > 1"
        ") "
        "ORDER BY track.file_hash, track.title COLLATE NOCASE, track.file_path COLLATE NOCASE "
        "LIMIT :limit"));
    query.bindValue(QStringLiteral(":limit"), std::max(1, std::min(limit, 500)));
    if (!query.exec()) {
        qWarning() << "Failed to load duplicate tracks" << query.lastError().text();
        return rows;
    }

    while (query.next()) {
        rows.append(readTrackRow(query));
    }

    return rows;
}

QList<TrackRow> LibraryRepository::searchTracks(const QString& queryText) const
{
    QList<TrackRow> rows;
    const QString trimmed = queryText.trimmed();
    if (!m_database || !m_database->isOpen() || trimmed.isEmpty()) {
        return rows;
    }

    const QString escaped = escapeLikePattern(trimmed);
    const QString pattern = QStringLiteral("%") + escaped + QStringLiteral("%");
    const QString prefix = escaped + QStringLiteral("%");

    QSqlQuery query(m_database->connection());
    query.prepare(QStringLiteral("SELECT ")
        + trackSelectColumns()
        + QStringLiteral(
        "FROM track "
        "LEFT JOIN cover ON cover.id = track.cover_id "
        "WHERE track.title LIKE :pattern ESCAPE '\\' "
        "OR track.display_artist LIKE :pattern ESCAPE '\\' "
        "OR track.display_album LIKE :pattern ESCAPE '\\' "
        "OR track.file_path LIKE :pattern ESCAPE '\\' "
        "ORDER BY CASE "
        "WHEN track.title LIKE :prefix ESCAPE '\\' THEN 0 "
        "WHEN track.display_artist LIKE :prefix ESCAPE '\\' THEN 1 "
        "WHEN track.display_album LIKE :prefix ESCAPE '\\' THEN 2 "
        "ELSE 3 END, track.title COLLATE NOCASE, track.file_path COLLATE NOCASE"));
    query.bindValue(QStringLiteral(":pattern"), pattern);
    query.bindValue(QStringLiteral(":prefix"), prefix);

    if (!query.exec()) {
        qWarning() << "Failed to search tracks" << queryText << query.lastError().text();
        return rows;
    }

    while (query.next()) {
        rows.append(readTrackRow(query));
    }

    return rows;
}

QList<AlbumRow> LibraryRepository::searchAlbums(const QString& queryText) const
{
    QList<AlbumRow> rows;
    const QString trimmed = queryText.trimmed();
    if (!m_database || !m_database->isOpen() || trimmed.isEmpty()) {
        return rows;
    }

    const QString escaped = escapeLikePattern(trimmed);
    const QString pattern = QStringLiteral("%") + escaped + QStringLiteral("%");
    const QString prefix = escaped + QStringLiteral("%");

    QSqlQuery query(m_database->connection());
    query.prepare(QStringLiteral(
        "SELECT album.id, album.title, album.display_album_artist, album.genre, cover.image_path, album.year, "
        "COUNT(track.id) AS track_count, IFNULL(SUM(track.duration_ms), 0) AS duration_ms "
        "FROM album "
        "LEFT JOIN track ON track.album_id = album.id "
        "LEFT JOIN cover ON cover.id = album.cover_id "
        "WHERE album.title LIKE :pattern ESCAPE '\\' "
        "OR album.display_album_artist LIKE :pattern ESCAPE '\\' "
        "GROUP BY album.id, album.title, album.display_album_artist, album.genre, cover.image_path, album.year "
        "ORDER BY CASE "
        "WHEN album.title LIKE :prefix ESCAPE '\\' THEN 0 "
        "WHEN album.display_album_artist LIKE :prefix ESCAPE '\\' THEN 1 "
        "ELSE 2 END, album.title COLLATE NOCASE"));
    query.bindValue(QStringLiteral(":pattern"), pattern);
    query.bindValue(QStringLiteral(":prefix"), prefix);

    if (!query.exec()) {
        qWarning() << "Failed to search albums" << queryText << query.lastError().text();
        return rows;
    }

    while (query.next()) {
        AlbumRow row;
        row.id = query.value(0).toInt();
        row.title = displayText(query.value(1));
        row.artist = displayText(query.value(2));
        row.genre = displayText(query.value(3));
        row.coverUrl = coverUrlFromPath(query.value(4).toString());
        row.year = query.value(5).toInt();
        row.trackCount = query.value(6).toInt();
        row.durationMs = query.value(7).toInt();
        rows.append(row);
    }

    return rows;
}

QList<ArtistRow> LibraryRepository::searchArtists(const QString& queryText) const
{
    QList<ArtistRow> rows;
    const QString trimmed = queryText.trimmed();
    if (!m_database || !m_database->isOpen() || trimmed.isEmpty()) {
        return rows;
    }

    const QString escaped = escapeLikePattern(trimmed);
    const QString pattern = QStringLiteral("%") + escaped + QStringLiteral("%");
    const QString prefix = escaped + QStringLiteral("%");

    QSqlQuery query(m_database->connection());
    query.prepare(QStringLiteral(
        "SELECT artist.id, artist.name, "
        "COUNT(DISTINCT track.album_id) AS album_count, "
        "COUNT(DISTINCT track_artist.track_id) AS track_count "
        "FROM artist "
        "LEFT JOIN track_artist ON track_artist.artist_id = artist.id "
        "LEFT JOIN track ON track.id = track_artist.track_id "
        "WHERE artist.name LIKE :pattern ESCAPE '\\' "
        "GROUP BY artist.id, artist.name "
        "ORDER BY CASE WHEN artist.name LIKE :prefix ESCAPE '\\' THEN 0 ELSE 1 END, artist.name COLLATE NOCASE"));
    query.bindValue(QStringLiteral(":pattern"), pattern);
    query.bindValue(QStringLiteral(":prefix"), prefix);

    if (!query.exec()) {
        qWarning() << "Failed to search artists" << queryText << query.lastError().text();
        return rows;
    }

    while (query.next()) {
        ArtistRow row;
        row.id = query.value(0).toInt();
        row.name = displayText(query.value(1));
        row.albumCount = query.value(2).toInt();
        row.trackCount = query.value(3).toInt();
        rows.append(row);
    }

    return rows;
}

QList<TrackRow> LibraryRepository::tracksForAlbum(int albumId) const
{
    QList<TrackRow> rows;
    if (!m_database || !m_database->isOpen() || albumId <= 0) {
        return rows;
    }

    QSqlQuery query(m_database->connection());
    query.prepare(QStringLiteral("SELECT ")
        + trackSelectColumns()
        + QStringLiteral(
        "FROM track "
        "LEFT JOIN cover ON cover.id = track.cover_id "
        "WHERE track.album_id = :album_id "
        "ORDER BY IFNULL(track.disc_number, 0), IFNULL(track.track_number, 0), track.title COLLATE NOCASE, track.file_path COLLATE NOCASE"));
    query.bindValue(QStringLiteral(":album_id"), albumId);

    if (!query.exec()) {
        qWarning() << "Failed to load album tracks" << albumId << query.lastError().text();
        return rows;
    }

    while (query.next()) {
        rows.append(readTrackRow(query));
    }

    return rows;
}

QList<TrackRow> LibraryRepository::tracksForArtist(int artistId) const
{
    QList<TrackRow> rows;
    if (!m_database || !m_database->isOpen() || artistId <= 0) {
        return rows;
    }

    QSqlQuery query(m_database->connection());
    query.prepare(QStringLiteral("SELECT DISTINCT ")
        + trackSelectColumns()
        + QStringLiteral(
        "FROM track "
        "INNER JOIN track_artist ON track_artist.track_id = track.id "
        "LEFT JOIN cover ON cover.id = track.cover_id "
        "WHERE track_artist.artist_id = :artist_id "
        "ORDER BY track.display_album COLLATE NOCASE, IFNULL(track.disc_number, 0), "
        "IFNULL(track.track_number, 0), track.title COLLATE NOCASE, track.file_path COLLATE NOCASE"));
    query.bindValue(QStringLiteral(":artist_id"), artistId);

    if (!query.exec()) {
        qWarning() << "Failed to load artist tracks" << artistId << query.lastError().text();
        return rows;
    }

    while (query.next()) {
        rows.append(readTrackRow(query));
    }

    return rows;
}

QList<TrackRow> LibraryRepository::tracksForGenre(const QString& genre) const
{
    QList<TrackRow> rows;
    const QString trimmed = genre.trimmed();
    if (!m_database || !m_database->isOpen() || trimmed.isEmpty()) {
        return rows;
    }

    QSqlQuery query(m_database->connection());
    query.prepare(QStringLiteral("SELECT ")
        + trackSelectColumns()
        + QStringLiteral(
        "FROM track "
        "LEFT JOIN cover ON cover.id = track.cover_id "
        "WHERE track.genre = :genre "
        "ORDER BY track.display_album COLLATE NOCASE, IFNULL(track.disc_number, 0), "
        "IFNULL(track.track_number, 0), track.title COLLATE NOCASE, track.file_path COLLATE NOCASE"));
    query.bindValue(QStringLiteral(":genre"), trimmed);

    if (!query.exec()) {
        qWarning() << "Failed to load genre tracks" << trimmed << query.lastError().text();
        return rows;
    }

    while (query.next()) {
        rows.append(readTrackRow(query));
    }

    return rows;
}

QList<TrackRow> LibraryRepository::tracksForPlaylist(int playlistId) const
{
    QList<TrackRow> rows;
    if (!m_database || !m_database->isOpen() || playlistId <= 0) {
        return rows;
    }

    QSqlQuery query(m_database->connection());
    query.prepare(QStringLiteral("SELECT ")
        + trackSelectColumns()
        + QStringLiteral(
        ", playlist_track.rowid, playlist_track.position "
        "FROM playlist_track "
        "INNER JOIN track ON track.id = playlist_track.track_id "
        "LEFT JOIN cover ON cover.id = track.cover_id "
        "WHERE playlist_track.playlist_id = :playlist_id "
        "ORDER BY playlist_track.position, playlist_track.added_at, playlist_track.rowid"));
    query.bindValue(QStringLiteral(":playlist_id"), playlistId);

    if (!query.exec()) {
        qWarning() << "Failed to load playlist tracks" << playlistId << query.lastError().text();
        return rows;
    }

    while (query.next()) {
        TrackRow row = readTrackRow(query);
        row.playlistEntryId = query.value(13).toLongLong();
        row.playlistPosition = query.value(14).toInt();
        rows.append(row);
    }

    return rows;
}

QList<AlbumRow> LibraryRepository::albums() const
{
    QList<AlbumRow> rows;
    if (!m_database || !m_database->isOpen()) {
        return rows;
    }

    QSqlQuery query(m_database->connection());
    if (!query.exec(QStringLiteral(
            "SELECT album.id, album.title, album.display_album_artist, album.genre, cover.image_path, album.year, "
            "COUNT(track.id) AS track_count, IFNULL(SUM(track.duration_ms), 0) AS duration_ms "
            "FROM album "
            "LEFT JOIN track ON track.album_id = album.id "
            "LEFT JOIN cover ON cover.id = album.cover_id "
            "GROUP BY album.id, album.title, album.display_album_artist, album.genre, cover.image_path, album.year "
            "ORDER BY album.title COLLATE NOCASE"))) {
        qWarning() << "Failed to load albums" << query.lastError().text();
        return rows;
    }

    while (query.next()) {
        AlbumRow row;
        row.id = query.value(0).toInt();
        row.title = displayText(query.value(1));
        row.artist = displayText(query.value(2));
        row.genre = displayText(query.value(3));
        row.coverUrl = coverUrlFromPath(query.value(4).toString());
        row.year = query.value(5).toInt();
        row.trackCount = query.value(6).toInt();
        row.durationMs = query.value(7).toInt();
        rows.append(row);
    }

    return rows;
}

QList<ArtistRow> LibraryRepository::artists() const
{
    QList<ArtistRow> rows;
    if (!m_database || !m_database->isOpen()) {
        return rows;
    }

    QSqlQuery query(m_database->connection());
    if (!query.exec(QStringLiteral(
            "SELECT artist.id, artist.name, "
            "COUNT(DISTINCT track.album_id) AS album_count, "
            "COUNT(DISTINCT track_artist.track_id) AS track_count "
            "FROM artist "
            "LEFT JOIN track_artist ON track_artist.artist_id = artist.id "
            "LEFT JOIN track ON track.id = track_artist.track_id "
            "GROUP BY artist.id, artist.name "
            "ORDER BY artist.name COLLATE NOCASE"))) {
        qWarning() << "Failed to load artists" << query.lastError().text();
        return rows;
    }

    while (query.next()) {
        ArtistRow row;
        row.id = query.value(0).toInt();
        row.name = displayText(query.value(1));
        row.albumCount = query.value(2).toInt();
        row.trackCount = query.value(3).toInt();
        rows.append(row);
    }

    return rows;
}

QList<GenreRow> LibraryRepository::genres() const
{
    QList<GenreRow> rows;
    if (!m_database || !m_database->isOpen()) {
        return rows;
    }

    QSqlQuery query(m_database->connection());
    if (!query.exec(QStringLiteral(
            "SELECT genre, COUNT(id) AS track_count, IFNULL(SUM(duration_ms), 0) AS duration_ms "
            "FROM track "
            "WHERE TRIM(IFNULL(genre, '')) != '' "
            "GROUP BY genre "
            "ORDER BY track_count DESC, genre COLLATE NOCASE"))) {
        qWarning() << "Failed to load genres" << query.lastError().text();
        return rows;
    }

    while (query.next()) {
        GenreRow row;
        row.name = displayText(query.value(0));
        row.trackCount = query.value(1).toInt();
        row.durationMs = query.value(2).toInt();
        rows.append(row);
    }

    return rows;
}

int LibraryRepository::upsertArtist(const QString& name)
{
    if (name.trimmed().isEmpty()) {
        return 0;
    }

    QSqlQuery insert(m_database->connection());
    insert.prepare(QStringLiteral(
        "INSERT INTO artist(name, created_at, updated_at) "
        "VALUES(:name, strftime('%s','now'), strftime('%s','now')) "
        "ON CONFLICT(name) DO UPDATE SET updated_at = strftime('%s','now')"));
    insert.bindValue(QStringLiteral(":name"), name.trimmed());
    if (!insert.exec()) {
        qWarning() << "Failed to upsert artist" << name << insert.lastError().text();
        return 0;
    }

    QSqlQuery select(m_database->connection());
    select.prepare(QStringLiteral("SELECT id FROM artist WHERE name = :name"));
    select.bindValue(QStringLiteral(":name"), name.trimmed());
    return select.exec() && select.next() ? select.value(0).toInt() : 0;
}

int LibraryRepository::upsertAlbum(const QString& title, int albumArtistId, const QString& albumArtistName)
{
    if (title.trimmed().isEmpty()) {
        return 0;
    }

    QSqlQuery selectExisting(m_database->connection());
    selectExisting.prepare(QStringLiteral(
        "SELECT id FROM album WHERE title = :title AND IFNULL(album_artist_id, 0) = :album_artist_id LIMIT 1"));
    selectExisting.bindValue(QStringLiteral(":title"), title.trimmed());
    selectExisting.bindValue(QStringLiteral(":album_artist_id"), albumArtistId);
    if (selectExisting.exec() && selectExisting.next()) {
        return selectExisting.value(0).toInt();
    }

    QSqlQuery insert(m_database->connection());
    insert.prepare(QStringLiteral(
        "INSERT INTO album(title, album_artist_id, display_album_artist, created_at, updated_at) "
        "VALUES(:title, :album_artist_id, :display_album_artist, strftime('%s','now'), strftime('%s','now'))"));
    insert.bindValue(QStringLiteral(":title"), title.trimmed());
    insert.bindValue(QStringLiteral(":album_artist_id"), albumArtistId > 0 ? QVariant(albumArtistId) : QVariant());
    insert.bindValue(QStringLiteral(":display_album_artist"), albumArtistName);

    if (!insert.exec()) {
        qWarning() << "Failed to upsert album" << title << insert.lastError().text();
        return 0;
    }

    return insert.lastInsertId().toInt();
}

int LibraryRepository::upsertCover(int sourceTrackId, const EmbeddedCover& cover)
{
    if (cover.data.empty()) {
        return 0;
    }

    const QByteArray bytes(
        reinterpret_cast<const char*>(cover.data.data()),
        static_cast<qsizetype>(cover.data.size()));
    const QString hash = QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha1).toHex());
    const QString coversDir = QDir(AppPaths::dataDir()).filePath(QStringLiteral("covers"));
    QDir().mkpath(coversDir);

    const QString mimeType = fromStd(cover.mimeType);
    const QString imagePath = QDir(coversDir).filePath(hash + QLatin1Char('.') + coverFileExtension(mimeType));
    if (!QFileInfo::exists(imagePath)) {
        QFile file(imagePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            qWarning() << "Failed to write cover image" << imagePath << file.errorString();
            return 0;
        }
        if (file.write(bytes) != bytes.size()) {
            qWarning() << "Failed to write complete cover image" << imagePath << file.errorString();
            return 0;
        }
    }

    QSqlQuery select(m_database->connection());
    select.prepare(QStringLiteral("SELECT id FROM cover WHERE image_hash = :image_hash LIMIT 1"));
    select.bindValue(QStringLiteral(":image_hash"), hash);
    if (select.exec() && select.next()) {
        const int coverId = select.value(0).toInt();
        QSqlQuery update(m_database->connection());
        update.prepare(QStringLiteral(
            "UPDATE cover SET source_track_id = COALESCE(source_track_id, :source_track_id), image_path = :image_path "
            "WHERE id = :cover_id"));
        update.bindValue(QStringLiteral(":source_track_id"), sourceTrackId);
        update.bindValue(QStringLiteral(":image_path"), imagePath);
        update.bindValue(QStringLiteral(":cover_id"), coverId);
        if (!update.exec()) {
            qWarning() << "Failed to update existing cover" << coverId << update.lastError().text();
        }
        return coverId;
    }

    QImageReader reader(imagePath);
    const QSize imageSize = reader.size();

    QSqlQuery insert(m_database->connection());
    insert.prepare(QStringLiteral(
        "INSERT INTO cover(source_track_id, image_hash, image_path, mime_type, width, height, created_at) "
        "VALUES(:source_track_id, :image_hash, :image_path, :mime_type, :width, :height, strftime('%s','now'))"));
    insert.bindValue(QStringLiteral(":source_track_id"), sourceTrackId);
    insert.bindValue(QStringLiteral(":image_hash"), hash);
    insert.bindValue(QStringLiteral(":image_path"), imagePath);
    insert.bindValue(QStringLiteral(":mime_type"), mimeType);
    insert.bindValue(QStringLiteral(":width"), imageSize.isValid() ? imageSize.width() : QVariant());
    insert.bindValue(QStringLiteral(":height"), imageSize.isValid() ? imageSize.height() : QVariant());
    if (!insert.exec()) {
        qWarning() << "Failed to insert cover" << imagePath << insert.lastError().text();
        return 0;
    }

    return insert.lastInsertId().toInt();
}

void LibraryRepository::upsertTrackArtist(int trackId, int artistId)
{
    QSqlQuery query(m_database->connection());
    query.prepare(QStringLiteral(
        "INSERT INTO track_artist(track_id, artist_id, role, position) "
        "VALUES(:track_id, :artist_id, 'primary', 0) "
        "ON CONFLICT(track_id, artist_id, role) DO NOTHING"));
    query.bindValue(QStringLiteral(":track_id"), trackId);
    query.bindValue(QStringLiteral(":artist_id"), artistId);
    if (!query.exec()) {
        qWarning() << "Failed to upsert track artist" << query.lastError().text();
    }
}

} // namespace opusora
