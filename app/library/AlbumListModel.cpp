#include "library/AlbumListModel.hpp"

#include "db/Database.hpp"

#include <QDebug>

namespace opusora {

AlbumListModel::AlbumListModel(Database* database, QObject* parent)
    : QAbstractListModel(parent)
    , m_database(database)
{
    reload();
}

int AlbumListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_albums.size();
}

int AlbumListModel::count() const
{
    return m_albums.size();
}

QVariant AlbumListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_albums.size()) {
        return {};
    }

    const AlbumRow& album = m_albums.at(index.row());
    switch (role) {
    case IdRole:
        return album.id;
    case TitleRole:
        return album.title;
    case ArtistRole:
        return album.artist;
    case GenreRole:
        return album.genre;
    case CoverUrlRole:
        return album.coverUrl;
    case YearRole:
        return album.year;
    case TrackCountRole:
        return album.trackCount;
    case DurationMsRole:
        return album.durationMs;
    case Qt::DisplayRole:
        return album.title;
    default:
        return {};
    }
}

QHash<int, QByteArray> AlbumListModel::roleNames() const
{
    return {
        { IdRole, "albumId" },
        { TitleRole, "title" },
        { ArtistRole, "artist" },
        { GenreRole, "genre" },
        { CoverUrlRole, "coverUrl" },
        { YearRole, "year" },
        { TrackCountRole, "trackCount" },
        { DurationMsRole, "durationMs" },
    };
}

void AlbumListModel::reload()
{
    const int previousCount = m_albums.size();
    beginResetModel();
    m_albums = LibraryRepository(m_database).albums();
    endResetModel();
    qInfo() << "Loaded album model rows" << m_albums.size();
    if (previousCount != m_albums.size()) {
        emit countChanged();
    }
}

} // namespace opusora
