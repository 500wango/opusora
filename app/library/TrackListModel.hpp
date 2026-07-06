#pragma once

#include "library/LibraryRepository.hpp"

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QVariantList>

namespace opusora {

class Database;

class TrackListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int totalDurationMs READ totalDurationMs NOTIFY totalDurationChanged)
    Q_PROPERTY(QString sortOrder READ sortOrder WRITE setSortOrder NOTIFY sortOrderChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        ArtistRole,
        AlbumRole,
        FilePathRole,
        CoverUrlRole,
        FormatRole,
        LyricsRole,
        DurationMsRole,
        TrackNumberRole,
        DiscNumberRole,
        ReplayGainTrackGainRole,
        ReplayGainAlbumGainRole,
        HasReplayGainTrackGainRole,
        HasReplayGainAlbumGainRole,
        PlaylistEntryIdRole,
        PlaylistPositionRole,
    };

    explicit TrackListModel(Database* database, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int count() const;
    int totalDurationMs() const;
    QString sortOrder() const;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void reload();
    Q_INVOKABLE void setSortOrder(const QString& sortOrder);
    Q_INVOKABLE void clearFilter();
    Q_INVOKABLE void loadRecentAdded(int limit = 8);
    Q_INVOKABLE void loadRecentPlayed(int limit = 8);
    Q_INVOKABLE void loadDuplicates(int limit = 100);
    Q_INVOKABLE void loadAlbum(int albumId);
    Q_INVOKABLE void loadArtist(int artistId);
    Q_INVOKABLE void loadGenre(const QString& genre);
    Q_INVOKABLE void loadPlaylist(int playlistId);
    Q_INVOKABLE QString filePathAt(int row) const;
    Q_INVOKABLE QString lyricsForTrack(int trackId) const;
    Q_INVOKABLE bool saveTrackDetails(int trackId, const QString& title, const QString& artist, const QString& album, const QString& lyricsText);
    Q_INVOKABLE QVariantList queueItems() const;

signals:
    void countChanged();
    void totalDurationChanged();
    void sortOrderChanged();

private:
    enum class FilterMode {
        All,
        Album,
        Artist,
        Genre,
        Playlist,
        RecentAdded,
        RecentPlayed,
        Duplicates,
    };

    Database* m_database = nullptr;
    QList<TrackRow> m_tracks;
    FilterMode m_filterMode = FilterMode::All;
    int m_filterId = 0;
    QString m_filterText;
    int m_limit = 8;
    QString m_sortOrder = QStringLiteral("title");
};

} // namespace opusora
