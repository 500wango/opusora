#pragma once

#include <QList>
#include <QString>
#include <QtGlobal>

namespace opusora {

class Database;

struct PlaylistRow {
    int id = 0;
    QString name;
    int trackCount = 0;
    int durationMs = 0;
};

class PlaylistRepository {
public:
    explicit PlaylistRepository(Database* database);

    QList<PlaylistRow> playlists() const;
    QList<PlaylistRow> searchPlaylists(const QString& queryText) const;
    bool createPlaylist(const QString& name);
    bool renamePlaylist(int playlistId, const QString& name);
    bool deletePlaylist(int playlistId);
    bool exportM3u(int playlistId, const QString& filePath);
    bool importM3u(const QString& filePath, const QString& playlistName = QString());
    bool addTrack(int playlistId, int trackId);
    int addTracks(int playlistId, const QList<int>& trackIds);
    bool removeTrack(int playlistId, int trackId);
    bool removeEntry(int playlistId, qint64 playlistEntryId);
    bool moveEntry(int playlistId, qint64 playlistEntryId, int direction);

private:
    Database* m_database = nullptr;
};

} // namespace opusora
