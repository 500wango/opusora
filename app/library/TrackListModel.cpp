#include "library/TrackListModel.hpp"

#include "db/Database.hpp"

#include <QDebug>
#include <QVariantMap>

#include <algorithm>

namespace opusora {

namespace {

bool isSupportedSortOrder(const QString& sortOrder)
{
    return sortOrder == QStringLiteral("title")
        || sortOrder == QStringLiteral("artist")
        || sortOrder == QStringLiteral("album")
        || sortOrder == QStringLiteral("recent")
        || sortOrder == QStringLiteral("duration");
}

} // namespace

TrackListModel::TrackListModel(Database* database, QObject* parent)
    : QAbstractListModel(parent)
    , m_database(database)
{
    reload();
}

int TrackListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_tracks.size();
}

int TrackListModel::count() const
{
    return m_tracks.size();
}

int TrackListModel::totalDurationMs() const
{
    int total = 0;
    for (const TrackRow& track : m_tracks) {
        total += track.durationMs;
    }
    return total;
}

QString TrackListModel::sortOrder() const
{
    return m_sortOrder;
}

QVariant TrackListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_tracks.size()) {
        return {};
    }

    const TrackRow& track = m_tracks.at(index.row());
    switch (role) {
    case IdRole:
        return track.id;
    case TitleRole:
        return track.title;
    case ArtistRole:
        return track.artist;
    case AlbumRole:
        return track.album;
    case FilePathRole:
        return track.filePath;
    case CoverUrlRole:
        return track.coverUrl;
    case FormatRole:
        return track.format;
    case LyricsRole:
        return track.lyricsText;
    case DurationMsRole:
        return track.durationMs;
    case TrackNumberRole:
        return track.trackNumber;
    case DiscNumberRole:
        return track.discNumber;
    case ReplayGainTrackGainRole:
        return track.replayGainTrackGainDb;
    case ReplayGainAlbumGainRole:
        return track.replayGainAlbumGainDb;
    case HasReplayGainTrackGainRole:
        return track.hasReplayGainTrackGain;
    case HasReplayGainAlbumGainRole:
        return track.hasReplayGainAlbumGain;
    case PlaylistEntryIdRole:
        return QVariant::fromValue(track.playlistEntryId);
    case PlaylistPositionRole:
        return track.playlistPosition;
    case Qt::DisplayRole:
        return track.title;
    default:
        return {};
    }
}

QHash<int, QByteArray> TrackListModel::roleNames() const
{
    return {
        { IdRole, "trackId" },
        { TitleRole, "title" },
        { ArtistRole, "artist" },
        { AlbumRole, "album" },
        { FilePathRole, "filePath" },
        { CoverUrlRole, "coverUrl" },
        { FormatRole, "format" },
        { LyricsRole, "lyricsText" },
        { DurationMsRole, "durationMs" },
        { TrackNumberRole, "trackNumber" },
        { DiscNumberRole, "discNumber" },
        { ReplayGainTrackGainRole, "replayGainTrackGainDb" },
        { ReplayGainAlbumGainRole, "replayGainAlbumGainDb" },
        { HasReplayGainTrackGainRole, "hasReplayGainTrackGain" },
        { HasReplayGainAlbumGainRole, "hasReplayGainAlbumGain" },
        { PlaylistEntryIdRole, "playlistEntryId" },
        { PlaylistPositionRole, "playlistPosition" },
    };
}

void TrackListModel::reload()
{
    const int previousCount = m_tracks.size();
    const int previousDuration = totalDurationMs();
    beginResetModel();
    LibraryRepository repository(m_database);
    switch (m_filterMode) {
    case FilterMode::All:
        m_tracks = repository.tracks(m_sortOrder);
        break;
    case FilterMode::Album:
        m_tracks = repository.tracksForAlbum(m_filterId);
        break;
    case FilterMode::Artist:
        m_tracks = repository.tracksForArtist(m_filterId);
        break;
    case FilterMode::Genre:
        m_tracks = repository.tracksForGenre(m_filterText);
        break;
    case FilterMode::Playlist:
        m_tracks = repository.tracksForPlaylist(m_filterId);
        break;
    case FilterMode::RecentAdded:
        m_tracks = repository.recentlyAddedTracks(m_limit);
        break;
    case FilterMode::RecentPlayed:
        m_tracks = repository.recentlyPlayedTracks(m_limit);
        break;
    case FilterMode::Duplicates:
        m_tracks = repository.duplicateTracks(m_limit);
        break;
    }
    endResetModel();
    qInfo() << "Loaded track model rows" << m_tracks.size();
    if (previousCount != m_tracks.size()) {
        emit countChanged();
    }
    if (previousDuration != totalDurationMs()) {
        emit totalDurationChanged();
    }
}

void TrackListModel::setSortOrder(const QString& sortOrder)
{
    const QString normalized = isSupportedSortOrder(sortOrder) ? sortOrder : QStringLiteral("title");
    if (m_sortOrder == normalized) {
        return;
    }

    m_sortOrder = normalized;
    emit sortOrderChanged();
    if (m_filterMode == FilterMode::All) {
        reload();
    }
}

void TrackListModel::clearFilter()
{
    m_filterMode = FilterMode::All;
    m_filterId = 0;
    m_filterText.clear();
    m_limit = 8;
    reload();
}

void TrackListModel::loadRecentAdded(int limit)
{
    m_filterMode = FilterMode::RecentAdded;
    m_filterId = 0;
    m_filterText.clear();
    m_limit = std::max(1, std::min(limit, 50));
    reload();
}

void TrackListModel::loadRecentPlayed(int limit)
{
    m_filterMode = FilterMode::RecentPlayed;
    m_filterId = 0;
    m_filterText.clear();
    m_limit = std::max(1, std::min(limit, 50));
    reload();
}

void TrackListModel::loadDuplicates(int limit)
{
    m_filterMode = FilterMode::Duplicates;
    m_filterId = 0;
    m_filterText.clear();
    m_limit = std::max(1, std::min(limit, 500));
    reload();
}

void TrackListModel::loadAlbum(int albumId)
{
    m_filterMode = FilterMode::Album;
    m_filterId = albumId;
    m_filterText.clear();
    m_limit = 8;
    reload();
}

void TrackListModel::loadArtist(int artistId)
{
    m_filterMode = FilterMode::Artist;
    m_filterId = artistId;
    m_filterText.clear();
    m_limit = 8;
    reload();
}

void TrackListModel::loadGenre(const QString& genre)
{
    m_filterMode = FilterMode::Genre;
    m_filterId = 0;
    m_filterText = genre.trimmed();
    m_limit = 8;
    reload();
}

void TrackListModel::loadPlaylist(int playlistId)
{
    m_filterMode = FilterMode::Playlist;
    m_filterId = playlistId;
    m_filterText.clear();
    m_limit = 8;
    reload();
}

QString TrackListModel::filePathAt(int row) const
{
    if (row < 0 || row >= m_tracks.size()) {
        return {};
    }

    return m_tracks.at(row).filePath;
}

QString TrackListModel::lyricsForTrack(int trackId) const
{
    return LibraryRepository(m_database).lyricsForTrack(trackId);
}

bool TrackListModel::saveTrackDetails(int trackId, const QString& title, const QString& artist, const QString& album, const QString& lyricsText)
{
    if (!LibraryRepository(m_database).updateTrackDetails(trackId, title, artist, album, lyricsText)) {
        return false;
    }

    reload();
    return true;
}

QVariantList TrackListModel::queueItems() const
{
    QVariantList items;
    items.reserve(m_tracks.size());

    for (const TrackRow& track : m_tracks) {
        QVariantMap item;
        item.insert(QStringLiteral("trackId"), track.id);
        item.insert(QStringLiteral("filePath"), track.filePath);
        item.insert(QStringLiteral("title"), track.title);
        item.insert(QStringLiteral("artist"), track.artist);
        item.insert(QStringLiteral("album"), track.album);
        item.insert(QStringLiteral("coverUrl"), track.coverUrl);
        item.insert(QStringLiteral("hasReplayGainTrackGain"), track.hasReplayGainTrackGain);
        item.insert(QStringLiteral("hasReplayGainAlbumGain"), track.hasReplayGainAlbumGain);
        if (track.hasReplayGainTrackGain) {
            item.insert(QStringLiteral("replayGainTrackGainDb"), track.replayGainTrackGainDb);
        }
        if (track.hasReplayGainAlbumGain) {
            item.insert(QStringLiteral("replayGainAlbumGainDb"), track.replayGainAlbumGainDb);
        }
        items.append(item);
    }

    return items;
}

} // namespace opusora
