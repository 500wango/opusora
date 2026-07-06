#include "search/SearchController.hpp"

#include "db/Database.hpp"

#include <QDebug>
#include <QVariantMap>

namespace opusora {

namespace {

QVariantMap queueItemFromTrack(const TrackRow& track)
{
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
    return item;
}

QVariantList queueItemsFromTracks(const QList<TrackRow>& tracks)
{
    QVariantList items;
    items.reserve(tracks.size());
    for (const TrackRow& track : tracks) {
        items.append(queueItemFromTrack(track));
    }
    return items;
}

QVariantMap sectionItem(const QString& labelKey, int count)
{
    QVariantMap item;
    item.insert(QStringLiteral("type"), QStringLiteral("section"));
    item.insert(QStringLiteral("labelKey"), labelKey);
    item.insert(QStringLiteral("count"), count);
    return item;
}

} // namespace

SearchController::SearchController(Database* database, QObject* parent)
    : QAbstractListModel(parent)
    , m_database(database)
{
}

QString SearchController::query() const
{
    return m_query;
}

int SearchController::count() const
{
    return m_results.size();
}

int SearchController::totalCount() const
{
    return m_results.size() + m_albumResults.size() + m_artistResults.size() + m_playlistResults.size();
}

QVariantList SearchController::groupedItems() const
{
    QVariantList items;

    if (!m_results.isEmpty()) {
        items.append(sectionItem(QStringLiteral("detail.tracks"), m_results.size()));
        for (const TrackRow& track : m_results) {
            QVariantMap item;
            item.insert(QStringLiteral("type"), QStringLiteral("track"));
            item.insert(QStringLiteral("id"), track.id);
            item.insert(QStringLiteral("title"), track.title);
            item.insert(QStringLiteral("artist"), track.artist);
            item.insert(QStringLiteral("album"), track.album);
            item.insert(QStringLiteral("filePath"), track.filePath);
            item.insert(QStringLiteral("coverUrl"), track.coverUrl);
            item.insert(QStringLiteral("durationMs"), track.durationMs);
            items.append(item);
        }
    }

    if (!m_albumResults.isEmpty()) {
        items.append(sectionItem(QStringLiteral("detail.albums"), m_albumResults.size()));
        for (const AlbumRow& album : m_albumResults) {
            QVariantMap item;
            item.insert(QStringLiteral("type"), QStringLiteral("album"));
            item.insert(QStringLiteral("id"), album.id);
            item.insert(QStringLiteral("title"), album.title);
            item.insert(QStringLiteral("artist"), album.artist);
            item.insert(QStringLiteral("coverUrl"), album.coverUrl);
            item.insert(QStringLiteral("trackCount"), album.trackCount);
            item.insert(QStringLiteral("durationMs"), album.durationMs);
            items.append(item);
        }
    }

    if (!m_artistResults.isEmpty()) {
        items.append(sectionItem(QStringLiteral("detail.artists"), m_artistResults.size()));
        for (const ArtistRow& artist : m_artistResults) {
            QVariantMap item;
            item.insert(QStringLiteral("type"), QStringLiteral("artist"));
            item.insert(QStringLiteral("id"), artist.id);
            item.insert(QStringLiteral("title"), artist.name);
            item.insert(QStringLiteral("albumCount"), artist.albumCount);
            item.insert(QStringLiteral("trackCount"), artist.trackCount);
            items.append(item);
        }
    }

    if (!m_playlistResults.isEmpty()) {
        items.append(sectionItem(QStringLiteral("detail.playlists"), m_playlistResults.size()));
        for (const PlaylistRow& playlist : m_playlistResults) {
            QVariantMap item;
            item.insert(QStringLiteral("type"), QStringLiteral("playlist"));
            item.insert(QStringLiteral("id"), playlist.id);
            item.insert(QStringLiteral("title"), playlist.name);
            item.insert(QStringLiteral("trackCount"), playlist.trackCount);
            item.insert(QStringLiteral("durationMs"), playlist.durationMs);
            items.append(item);
        }
    }

    return items;
}

int SearchController::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_results.size();
}

QVariant SearchController::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_results.size()) {
        return {};
    }

    const TrackRow& track = m_results.at(index.row());
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
    case DurationMsRole:
        return track.durationMs;
    case TrackNumberRole:
        return track.trackNumber;
    case DiscNumberRole:
        return track.discNumber;
    case Qt::DisplayRole:
        return track.title;
    default:
        return {};
    }
}

QHash<int, QByteArray> SearchController::roleNames() const
{
    return {
        { IdRole, "trackId" },
        { TitleRole, "title" },
        { ArtistRole, "artist" },
        { AlbumRole, "album" },
        { FilePathRole, "filePath" },
        { CoverUrlRole, "coverUrl" },
        { FormatRole, "format" },
        { DurationMsRole, "durationMs" },
        { TrackNumberRole, "trackNumber" },
        { DiscNumberRole, "discNumber" },
    };
}

void SearchController::search(const QString& query)
{
    const QString trimmed = query.trimmed();
    if (m_query == trimmed) {
        reload();
        return;
    }

    m_query = trimmed;
    emit queryChanged();
    reload();
}

void SearchController::reload()
{
    const int previousCount = totalCount();
    beginResetModel();
    LibraryRepository libraryRepository(m_database);
    m_results = libraryRepository.searchTracks(m_query);
    m_albumResults = libraryRepository.searchAlbums(m_query);
    m_artistResults = libraryRepository.searchArtists(m_query);
    m_playlistResults = PlaylistRepository(m_database).searchPlaylists(m_query);
    endResetModel();

    qInfo() << "Loaded search result rows" << totalCount() << "for query" << m_query;
    emit resultsChanged();
    if (previousCount != totalCount()) {
        emit countChanged();
    }
}

QVariantList SearchController::queueItems() const
{
    QVariantList items;
    items.reserve(m_results.size());

    for (const TrackRow& track : m_results) {
        items.append(queueItemFromTrack(track));
    }

    return items;
}

QVariantList SearchController::queueItemsForResult(const QString& resultType, int id) const
{
    if (id <= 0) {
        return {};
    }

    if (resultType == QStringLiteral("track")) {
        for (const TrackRow& track : m_results) {
            if (track.id == id) {
                return QVariantList { queueItemFromTrack(track) };
            }
        }
        return {};
    }

    LibraryRepository repository(m_database);
    if (resultType == QStringLiteral("album")) {
        return queueItemsFromTracks(repository.tracksForAlbum(id));
    }
    if (resultType == QStringLiteral("artist")) {
        return queueItemsFromTracks(repository.tracksForArtist(id));
    }
    if (resultType == QStringLiteral("playlist")) {
        return queueItemsFromTracks(repository.tracksForPlaylist(id));
    }

    return {};
}

} // namespace opusora
