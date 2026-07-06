#include "playlist/PlaylistRepository.hpp"

#include "db/Database.hpp"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>
#include <QUrl>

namespace {

bool updatePlaylistTimestamp(QSqlDatabase& database, int playlistId)
{
    QSqlQuery update(database);
    update.prepare(QStringLiteral("UPDATE playlist SET updated_at = strftime('%s','now') WHERE id = :playlist_id"));
    update.bindValue(QStringLiteral(":playlist_id"), playlistId);
    if (!update.exec()) {
        qWarning() << "Failed to update playlist timestamp" << playlistId << update.lastError().text();
        return false;
    }

    return true;
}

bool renumberPlaylistPositions(QSqlDatabase& database, int playlistId)
{
    QList<qint64> entryIds;

    QSqlQuery select(database);
    select.prepare(QStringLiteral(
        "SELECT rowid FROM playlist_track "
        "WHERE playlist_id = :playlist_id "
        "ORDER BY position, added_at, rowid"));
    select.bindValue(QStringLiteral(":playlist_id"), playlistId);
    if (!select.exec()) {
        qWarning() << "Failed to load playlist entries for renumbering" << playlistId << select.lastError().text();
        return false;
    }

    while (select.next()) {
        entryIds.append(select.value(0).toLongLong());
    }

    for (int index = 0; index < entryIds.size(); ++index) {
        QSqlQuery tempUpdate(database);
        tempUpdate.prepare(QStringLiteral("UPDATE playlist_track SET position = :position WHERE rowid = :entry_id"));
        tempUpdate.bindValue(QStringLiteral(":position"), -index - 1);
        tempUpdate.bindValue(QStringLiteral(":entry_id"), entryIds.at(index));
        if (!tempUpdate.exec()) {
            qWarning() << "Failed to stage playlist position" << playlistId << tempUpdate.lastError().text();
            return false;
        }
    }

    for (int index = 0; index < entryIds.size(); ++index) {
        QSqlQuery finalUpdate(database);
        finalUpdate.prepare(QStringLiteral("UPDATE playlist_track SET position = :position WHERE rowid = :entry_id"));
        finalUpdate.bindValue(QStringLiteral(":position"), index);
        finalUpdate.bindValue(QStringLiteral(":entry_id"), entryIds.at(index));
        if (!finalUpdate.exec()) {
            qWarning() << "Failed to write playlist position" << playlistId << finalUpdate.lastError().text();
            return false;
        }
    }

    return true;
}

QString localPathFromM3uLine(const QString& line, const QDir& baseDir)
{
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#'))) {
        return {};
    }

    QString path = trimmed;
    const QUrl url(trimmed);
    if (url.isValid() && url.isLocalFile()) {
        path = url.toLocalFile();
    } else if (url.isValid() && !url.scheme().isEmpty()) {
        return {};
    }

    if (QDir::isRelativePath(path)) {
        path = baseDir.absoluteFilePath(path);
    }

    return QFileInfo(QDir::cleanPath(path)).absoluteFilePath();
}

QString playlistNameForImport(const QString& filePath, const QString& requestedName)
{
    const QString trimmed = requestedName.trimmed();
    if (!trimmed.isEmpty()) {
        return trimmed;
    }

    const QString baseName = QFileInfo(filePath).completeBaseName().trimmed();
    return baseName.isEmpty() ? QStringLiteral("Imported Playlist") : baseName;
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

} // namespace

namespace opusora {

PlaylistRepository::PlaylistRepository(Database* database)
    : m_database(database)
{
}

QList<PlaylistRow> PlaylistRepository::playlists() const
{
    QList<PlaylistRow> rows;
    if (!m_database || !m_database->isOpen()) {
        return rows;
    }

    QSqlQuery query(m_database->connection());
    if (!query.exec(QStringLiteral(
            "SELECT playlist.id, playlist.name, COUNT(playlist_track.track_id) AS track_count, "
            "IFNULL(SUM(track.duration_ms), 0) AS duration_ms "
            "FROM playlist "
            "LEFT JOIN playlist_track ON playlist_track.playlist_id = playlist.id "
            "LEFT JOIN track ON track.id = playlist_track.track_id "
            "GROUP BY playlist.id, playlist.name, playlist.sort_order "
            "ORDER BY playlist.sort_order, playlist.created_at, playlist.name COLLATE NOCASE"))) {
        qWarning() << "Failed to load playlists" << query.lastError().text();
        return rows;
    }

    while (query.next()) {
        PlaylistRow row;
        row.id = query.value(0).toInt();
        row.name = query.value(1).toString();
        row.trackCount = query.value(2).toInt();
        row.durationMs = query.value(3).toInt();
        rows.append(row);
    }

    return rows;
}

QList<PlaylistRow> PlaylistRepository::searchPlaylists(const QString& queryText) const
{
    QList<PlaylistRow> rows;
    const QString trimmed = queryText.trimmed();
    if (!m_database || !m_database->isOpen() || trimmed.isEmpty()) {
        return rows;
    }

    const QString escaped = escapeLikePattern(trimmed);
    const QString pattern = QStringLiteral("%") + escaped + QStringLiteral("%");
    const QString prefix = escaped + QStringLiteral("%");

    QSqlQuery query(m_database->connection());
    query.prepare(QStringLiteral(
        "SELECT playlist.id, playlist.name, COUNT(playlist_track.track_id) AS track_count, "
        "IFNULL(SUM(track.duration_ms), 0) AS duration_ms "
        "FROM playlist "
        "LEFT JOIN playlist_track ON playlist_track.playlist_id = playlist.id "
        "LEFT JOIN track ON track.id = playlist_track.track_id "
        "WHERE playlist.name LIKE :pattern ESCAPE '\\' "
        "GROUP BY playlist.id, playlist.name, playlist.sort_order "
        "ORDER BY CASE WHEN playlist.name LIKE :prefix ESCAPE '\\' THEN 0 ELSE 1 END, "
        "playlist.sort_order, playlist.name COLLATE NOCASE"));
    query.bindValue(QStringLiteral(":pattern"), pattern);
    query.bindValue(QStringLiteral(":prefix"), prefix);

    if (!query.exec()) {
        qWarning() << "Failed to search playlists" << trimmed << query.lastError().text();
        return rows;
    }

    while (query.next()) {
        PlaylistRow row;
        row.id = query.value(0).toInt();
        row.name = query.value(1).toString();
        row.trackCount = query.value(2).toInt();
        row.durationMs = query.value(3).toInt();
        rows.append(row);
    }

    return rows;
}

bool PlaylistRepository::createPlaylist(const QString& name)
{
    const QString trimmed = name.trimmed();
    if (!m_database || !m_database->isOpen() || trimmed.isEmpty()) {
        return false;
    }

    QSqlQuery query(m_database->connection());
    query.prepare(QStringLiteral(
        "INSERT INTO playlist(name, sort_order, created_at, updated_at) "
        "VALUES(:name, IFNULL((SELECT MAX(sort_order) + 1 FROM playlist), 0), strftime('%s','now'), strftime('%s','now'))"));
    query.bindValue(QStringLiteral(":name"), trimmed);

    if (!query.exec()) {
        qWarning() << "Failed to create playlist" << trimmed << query.lastError().text();
        return false;
    }

    return true;
}

bool PlaylistRepository::renamePlaylist(int playlistId, const QString& name)
{
    const QString trimmed = name.trimmed();
    if (!m_database || !m_database->isOpen() || playlistId <= 0 || trimmed.isEmpty()) {
        return false;
    }

    QSqlQuery query(m_database->connection());
    query.prepare(QStringLiteral(
        "UPDATE playlist SET name = :name, updated_at = strftime('%s','now') "
        "WHERE id = :playlist_id"));
    query.bindValue(QStringLiteral(":name"), trimmed);
    query.bindValue(QStringLiteral(":playlist_id"), playlistId);

    if (!query.exec()) {
        qWarning() << "Failed to rename playlist" << playlistId << trimmed << query.lastError().text();
        return false;
    }

    return query.numRowsAffected() > 0;
}

bool PlaylistRepository::deletePlaylist(int playlistId)
{
    if (!m_database || !m_database->isOpen() || playlistId <= 0) {
        return false;
    }

    QSqlQuery query(m_database->connection());
    query.prepare(QStringLiteral("DELETE FROM playlist WHERE id = :playlist_id"));
    query.bindValue(QStringLiteral(":playlist_id"), playlistId);

    if (!query.exec()) {
        qWarning() << "Failed to delete playlist" << playlistId << query.lastError().text();
        return false;
    }

    return query.numRowsAffected() > 0;
}

bool PlaylistRepository::exportM3u(int playlistId, const QString& filePath)
{
    const QString trimmedPath = filePath.trimmed();
    if (!m_database || !m_database->isOpen() || playlistId <= 0 || trimmedPath.isEmpty()) {
        return false;
    }

    QSqlQuery query(m_database->connection());
    query.prepare(QStringLiteral(
        "SELECT track.file_path, track.title, track.display_artist, track.duration_ms "
        "FROM playlist_track "
        "INNER JOIN track ON track.id = playlist_track.track_id "
        "WHERE playlist_track.playlist_id = :playlist_id "
        "ORDER BY playlist_track.position, playlist_track.added_at, playlist_track.rowid"));
    query.bindValue(QStringLiteral(":playlist_id"), playlistId);

    if (!query.exec()) {
        qWarning() << "Failed to load playlist tracks for M3U export" << playlistId << query.lastError().text();
        return false;
    }

    struct ExportTrack {
        QString filePath;
        QString title;
        QString artist;
        int durationMs = 0;
    };

    QList<ExportTrack> tracks;
    while (query.next()) {
        ExportTrack track;
        track.filePath = query.value(0).toString();
        track.title = query.value(1).toString();
        track.artist = query.value(2).toString();
        track.durationMs = query.value(3).toInt();
        tracks.append(track);
    }

    if (tracks.isEmpty()) {
        qWarning() << "Refusing to export empty playlist" << playlistId;
        return false;
    }

    const QFileInfo fileInfo(trimmedPath);
    const QDir parentDir = fileInfo.absoluteDir();
    if (!parentDir.exists()) {
        qWarning() << "M3U export directory does not exist" << parentDir.absolutePath();
        return false;
    }

    QFile file(fileInfo.absoluteFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qWarning() << "Failed to open M3U export file" << fileInfo.absoluteFilePath() << file.errorString();
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << "#EXTM3U\n";
    for (const ExportTrack& track : tracks) {
        const int durationSeconds = track.durationMs > 0 ? track.durationMs / 1000 : -1;
        QString displayTitle = track.title.trimmed();
        if (!track.artist.trimmed().isEmpty()) {
            displayTitle = track.artist.trimmed() + QStringLiteral(" - ") + displayTitle;
        }
        if (displayTitle.trimmed().isEmpty()) {
            displayTitle = QFileInfo(track.filePath).completeBaseName();
        }

        stream << "#EXTINF:" << durationSeconds << "," << displayTitle << "\n";
        stream << track.filePath << "\n";
    }

    if (stream.status() != QTextStream::Ok) {
        qWarning() << "Failed while writing M3U export file" << fileInfo.absoluteFilePath();
        return false;
    }

    qInfo() << "Exported playlist" << playlistId << "to" << fileInfo.absoluteFilePath()
            << "tracks" << tracks.size();
    return true;
}

bool PlaylistRepository::importM3u(const QString& filePath, const QString& playlistName)
{
    const QFileInfo fileInfo(filePath.trimmed());
    if (!m_database || !m_database->isOpen() || fileInfo.filePath().trimmed().isEmpty()) {
        return false;
    }

    QFile file(fileInfo.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open M3U import file" << fileInfo.absoluteFilePath() << file.errorString();
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);

    QList<QString> localPaths;
    const QDir baseDir = fileInfo.absoluteDir();
    while (!stream.atEnd()) {
        const QString localPath = localPathFromM3uLine(stream.readLine(), baseDir);
        if (!localPath.isEmpty()) {
            localPaths.append(localPath);
        }
    }

    if (localPaths.isEmpty()) {
        qWarning() << "M3U import found no local file paths" << fileInfo.absoluteFilePath();
        return false;
    }

    QList<int> trackIds;
    QSqlQuery selectTrack(m_database->connection());
    selectTrack.prepare(QStringLiteral("SELECT id FROM track WHERE file_path = :file_path"));
    for (const QString& localPath : localPaths) {
        selectTrack.bindValue(QStringLiteral(":file_path"), localPath);
        if (!selectTrack.exec()) {
            qWarning() << "Failed to match imported M3U track" << localPath << selectTrack.lastError().text();
            return false;
        }
        if (selectTrack.next()) {
            trackIds.append(selectTrack.value(0).toInt());
        }
        selectTrack.finish();
    }

    if (trackIds.isEmpty()) {
        qWarning() << "M3U import found no tracks already indexed in the library" << fileInfo.absoluteFilePath();
        return false;
    }

    QSqlDatabase database = m_database->connection();
    if (!database.transaction()) {
        qWarning() << "Failed to start M3U import transaction" << database.lastError().text();
        return false;
    }

    QSqlQuery insertPlaylist(database);
    insertPlaylist.prepare(QStringLiteral(
        "INSERT INTO playlist(name, sort_order, created_at, updated_at) "
        "VALUES(:name, IFNULL((SELECT MAX(sort_order) + 1 FROM playlist), 0), strftime('%s','now'), strftime('%s','now'))"));
    insertPlaylist.bindValue(QStringLiteral(":name"), playlistNameForImport(fileInfo.absoluteFilePath(), playlistName));
    if (!insertPlaylist.exec()) {
        qWarning() << "Failed to create imported playlist" << fileInfo.absoluteFilePath()
                   << insertPlaylist.lastError().text();
        database.rollback();
        return false;
    }

    const int playlistId = insertPlaylist.lastInsertId().toInt();
    if (playlistId <= 0) {
        qWarning() << "Failed to determine imported playlist id" << fileInfo.absoluteFilePath();
        database.rollback();
        return false;
    }

    QSqlQuery insertTrack(database);
    insertTrack.prepare(QStringLiteral(
        "INSERT INTO playlist_track(playlist_id, track_id, position, added_at) "
        "VALUES(:playlist_id, :track_id, :position, strftime('%s','now'))"));
    for (int index = 0; index < trackIds.size(); ++index) {
        insertTrack.bindValue(QStringLiteral(":playlist_id"), playlistId);
        insertTrack.bindValue(QStringLiteral(":track_id"), trackIds.at(index));
        insertTrack.bindValue(QStringLiteral(":position"), index);
        if (!insertTrack.exec()) {
            qWarning() << "Failed to add imported M3U track" << playlistId << trackIds.at(index)
                       << insertTrack.lastError().text();
            database.rollback();
            return false;
        }
    }

    if (!database.commit()) {
        qWarning() << "Failed to commit M3U import" << fileInfo.absoluteFilePath() << database.lastError().text();
        return false;
    }

    qInfo() << "Imported M3U playlist" << fileInfo.absoluteFilePath()
            << "matched" << trackIds.size() << "of" << localPaths.size() << "paths";
    return true;
}

bool PlaylistRepository::addTrack(int playlistId, int trackId)
{
    return addTracks(playlistId, QList<int> { trackId }) == 1;
}

int PlaylistRepository::addTracks(int playlistId, const QList<int>& trackIds)
{
    if (!m_database || !m_database->isOpen() || playlistId <= 0 || trackIds.isEmpty()) {
        return 0;
    }

    QList<int> validTrackIds;
    validTrackIds.reserve(trackIds.size());
    for (const int trackId : trackIds) {
        if (trackId > 0) {
            validTrackIds.append(trackId);
        }
    }
    if (validTrackIds.isEmpty()) {
        return 0;
    }

    QSqlDatabase database = m_database->connection();
    if (!database.transaction()) {
        qWarning() << "Failed to start playlist bulk add transaction" << playlistId << database.lastError().text();
        return 0;
    }

    int startPosition = 0;
    QSqlQuery positionQuery(database);
    positionQuery.prepare(QStringLiteral(
        "SELECT IFNULL(MAX(position) + 1, 0) FROM playlist_track WHERE playlist_id = :playlist_id"));
    positionQuery.bindValue(QStringLiteral(":playlist_id"), playlistId);
    if (!positionQuery.exec() || !positionQuery.next()) {
        qWarning() << "Failed to load playlist append position" << playlistId << positionQuery.lastError().text();
        database.rollback();
        return 0;
    }
    startPosition = positionQuery.value(0).toInt();

    QSqlQuery insert(database);
    insert.prepare(QStringLiteral(
        "INSERT INTO playlist_track(playlist_id, track_id, position, added_at) "
        "VALUES(:playlist_id, :track_id, :position, strftime('%s','now'))"));

    int added = 0;
    for (const int trackId : validTrackIds) {
        insert.bindValue(QStringLiteral(":playlist_id"), playlistId);
        insert.bindValue(QStringLiteral(":track_id"), trackId);
        insert.bindValue(QStringLiteral(":position"), startPosition + added);
        if (!insert.exec()) {
            qWarning() << "Failed to bulk add track to playlist" << playlistId << trackId
                       << insert.lastError().text();
            database.rollback();
            return 0;
        }
        ++added;
    }

    if (!updatePlaylistTimestamp(database, playlistId)) {
        database.rollback();
        return 0;
    }

    if (!database.commit()) {
        qWarning() << "Failed to commit playlist bulk add" << playlistId << database.lastError().text();
        return 0;
    }

    return added;
}

bool PlaylistRepository::removeTrack(int playlistId, int trackId)
{
    if (!m_database || !m_database->isOpen() || playlistId <= 0 || trackId <= 0) {
        return false;
    }

    QSqlDatabase database = m_database->connection();
    if (!database.transaction()) {
        qWarning() << "Failed to start playlist track removal transaction" << database.lastError().text();
        return false;
    }

    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "DELETE FROM playlist_track WHERE playlist_id = :playlist_id AND track_id = :track_id"));
    query.bindValue(QStringLiteral(":playlist_id"), playlistId);
    query.bindValue(QStringLiteral(":track_id"), trackId);

    if (!query.exec()) {
        qWarning() << "Failed to remove track from playlist" << playlistId << trackId << query.lastError().text();
        database.rollback();
        return false;
    }

    const bool removed = query.numRowsAffected() > 0;
    if (removed && (!renumberPlaylistPositions(database, playlistId) || !updatePlaylistTimestamp(database, playlistId))) {
        database.rollback();
        return false;
    }

    if (!database.commit()) {
        qWarning() << "Failed to commit playlist track removal" << playlistId << database.lastError().text();
        return false;
    }

    return removed;
}

bool PlaylistRepository::removeEntry(int playlistId, qint64 playlistEntryId)
{
    if (!m_database || !m_database->isOpen() || playlistId <= 0 || playlistEntryId <= 0) {
        return false;
    }

    QSqlDatabase database = m_database->connection();
    if (!database.transaction()) {
        qWarning() << "Failed to start playlist entry removal transaction" << database.lastError().text();
        return false;
    }

    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "DELETE FROM playlist_track WHERE playlist_id = :playlist_id AND rowid = :entry_id"));
    query.bindValue(QStringLiteral(":playlist_id"), playlistId);
    query.bindValue(QStringLiteral(":entry_id"), playlistEntryId);
    if (!query.exec()) {
        qWarning() << "Failed to remove playlist entry" << playlistId << playlistEntryId << query.lastError().text();
        database.rollback();
        return false;
    }

    if (query.numRowsAffected() <= 0) {
        database.rollback();
        return false;
    }

    if (!renumberPlaylistPositions(database, playlistId) || !updatePlaylistTimestamp(database, playlistId)) {
        database.rollback();
        return false;
    }

    if (!database.commit()) {
        qWarning() << "Failed to commit playlist entry removal" << playlistId << database.lastError().text();
        return false;
    }

    return true;
}

bool PlaylistRepository::moveEntry(int playlistId, qint64 playlistEntryId, int direction)
{
    if (!m_database || !m_database->isOpen() || playlistId <= 0 || playlistEntryId <= 0 || direction == 0) {
        return false;
    }

    QSqlDatabase database = m_database->connection();
    if (!database.transaction()) {
        qWarning() << "Failed to start playlist entry move transaction" << database.lastError().text();
        return false;
    }

    if (!renumberPlaylistPositions(database, playlistId)) {
        database.rollback();
        return false;
    }

    QSqlQuery current(database);
    current.prepare(QStringLiteral(
        "SELECT position FROM playlist_track WHERE playlist_id = :playlist_id AND rowid = :entry_id"));
    current.bindValue(QStringLiteral(":playlist_id"), playlistId);
    current.bindValue(QStringLiteral(":entry_id"), playlistEntryId);
    if (!current.exec()) {
        qWarning() << "Failed to load playlist entry for moving" << playlistId << playlistEntryId
                   << current.lastError().text();
        database.rollback();
        return false;
    }
    if (!current.next()) {
        database.rollback();
        return false;
    }

    const int currentPosition = current.value(0).toInt();
    QSqlQuery neighbor(database);
    if (direction < 0) {
        neighbor.prepare(QStringLiteral(
            "SELECT rowid, position FROM playlist_track "
            "WHERE playlist_id = :playlist_id AND position < :position "
            "ORDER BY position DESC LIMIT 1"));
    } else {
        neighbor.prepare(QStringLiteral(
            "SELECT rowid, position FROM playlist_track "
            "WHERE playlist_id = :playlist_id AND position > :position "
            "ORDER BY position ASC LIMIT 1"));
    }
    neighbor.bindValue(QStringLiteral(":playlist_id"), playlistId);
    neighbor.bindValue(QStringLiteral(":position"), currentPosition);
    if (!neighbor.exec()) {
        qWarning() << "Failed to load neighboring playlist entry" << playlistId << neighbor.lastError().text();
        database.rollback();
        return false;
    }
    if (!neighbor.next()) {
        database.rollback();
        return false;
    }

    const qint64 neighborEntryId = neighbor.value(0).toLongLong();
    const int neighborPosition = neighbor.value(1).toInt();

    QSqlQuery stageCurrent(database);
    stageCurrent.prepare(QStringLiteral("UPDATE playlist_track SET position = -1 WHERE rowid = :entry_id"));
    stageCurrent.bindValue(QStringLiteral(":entry_id"), playlistEntryId);
    if (!stageCurrent.exec()) {
        qWarning() << "Failed to stage playlist entry move" << playlistId << stageCurrent.lastError().text();
        database.rollback();
        return false;
    }

    QSqlQuery moveNeighbor(database);
    moveNeighbor.prepare(QStringLiteral("UPDATE playlist_track SET position = :position WHERE rowid = :entry_id"));
    moveNeighbor.bindValue(QStringLiteral(":position"), currentPosition);
    moveNeighbor.bindValue(QStringLiteral(":entry_id"), neighborEntryId);
    if (!moveNeighbor.exec()) {
        qWarning() << "Failed to move neighboring playlist entry" << playlistId << moveNeighbor.lastError().text();
        database.rollback();
        return false;
    }

    QSqlQuery moveCurrent(database);
    moveCurrent.prepare(QStringLiteral("UPDATE playlist_track SET position = :position WHERE rowid = :entry_id"));
    moveCurrent.bindValue(QStringLiteral(":position"), neighborPosition);
    moveCurrent.bindValue(QStringLiteral(":entry_id"), playlistEntryId);
    if (!moveCurrent.exec()) {
        qWarning() << "Failed to move playlist entry" << playlistId << moveCurrent.lastError().text();
        database.rollback();
        return false;
    }

    if (!updatePlaylistTimestamp(database, playlistId)) {
        database.rollback();
        return false;
    }

    if (!database.commit()) {
        qWarning() << "Failed to commit playlist entry move" << playlistId << database.lastError().text();
        return false;
    }

    return true;
}

} // namespace opusora
