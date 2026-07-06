#pragma once

#include "metadata/MetadataTypes.hpp"

#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QtGlobal>

namespace opusora {

class Database;

struct TrackRow {
    int id = 0;
    QString title;
    QString artist;
    QString album;
    QString filePath;
    QString coverUrl;
    QString format;
    QString lyricsText;
    int durationMs = 0;
    int trackNumber = 0;
    int discNumber = 0;
    double replayGainTrackGainDb = 0.0;
    double replayGainAlbumGainDb = 0.0;
    bool hasReplayGainTrackGain = false;
    bool hasReplayGainAlbumGain = false;
    qint64 playlistEntryId = 0;
    int playlistPosition = -1;
};

struct AlbumRow {
    int id = 0;
    QString title;
    QString artist;
    QString genre;
    QString coverUrl;
    int year = 0;
    int trackCount = 0;
    int durationMs = 0;
};

struct ArtistRow {
    int id = 0;
    QString name;
    int albumCount = 0;
    int trackCount = 0;
};

struct GenreRow {
    QString name;
    int trackCount = 0;
    int durationMs = 0;
};

struct TrackFileStamp {
    qint64 fileSize = -1;
    qint64 modifiedAt = -1;
};

class LibraryRepository {
public:
    explicit LibraryRepository(Database* database);

    bool upsertTrack(const QString& filePath, const TrackMetadata& metadata);
    QHash<QString, TrackFileStamp> trackFileStampsUnderRoot(const QString& rootPath) const;
    bool removeTracksUnderRoot(const QString& rootPath);
    bool removeMissingTracksUnderRoot(const QString& rootPath, const QSet<QString>& existingFilePaths);
    bool clearLibraryIndex();
    int repairStoredMetadataText();
    bool updateTrackDetails(int trackId, const QString& title, const QString& artist, const QString& album, const QString& lyricsText);
    bool markTrackPlayed(int trackId);
    QString lyricsForTrack(int trackId) const;
    QList<TrackRow> tracks(const QString& sortOrder = QStringLiteral("title")) const;
    QList<TrackRow> recentlyAddedTracks(int limit = 8) const;
    QList<TrackRow> recentlyPlayedTracks(int limit = 8) const;
    QList<TrackRow> duplicateTracks(int limit = 100) const;
    QList<TrackRow> searchTracks(const QString& queryText) const;
    QList<AlbumRow> searchAlbums(const QString& queryText) const;
    QList<ArtistRow> searchArtists(const QString& queryText) const;
    QList<TrackRow> tracksForAlbum(int albumId) const;
    QList<TrackRow> tracksForArtist(int artistId) const;
    QList<TrackRow> tracksForGenre(const QString& genre) const;
    QList<TrackRow> tracksForPlaylist(int playlistId) const;
    QList<AlbumRow> albums() const;
    QList<ArtistRow> artists() const;
    QList<GenreRow> genres() const;

private:
    int upsertArtist(const QString& name);
    int upsertAlbum(const QString& title, int albumArtistId, const QString& albumArtistName);
    int upsertCover(int sourceTrackId, const EmbeddedCover& cover);
    void upsertTrackArtist(int trackId, int artistId);

    Database* m_database = nullptr;
};

} // namespace opusora
