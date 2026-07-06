#include "library/ArtistListModel.hpp"

#include "db/Database.hpp"

#include <QDebug>

namespace opusora {

ArtistListModel::ArtistListModel(Database* database, QObject* parent)
    : QAbstractListModel(parent)
    , m_database(database)
{
    reload();
}

int ArtistListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_artists.size();
}

int ArtistListModel::count() const
{
    return m_artists.size();
}

QVariant ArtistListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_artists.size()) {
        return {};
    }

    const ArtistRow& artist = m_artists.at(index.row());
    switch (role) {
    case IdRole:
        return artist.id;
    case NameRole:
        return artist.name;
    case AlbumCountRole:
        return artist.albumCount;
    case TrackCountRole:
        return artist.trackCount;
    case Qt::DisplayRole:
        return artist.name;
    default:
        return {};
    }
}

QHash<int, QByteArray> ArtistListModel::roleNames() const
{
    return {
        { IdRole, "artistId" },
        { NameRole, "name" },
        { AlbumCountRole, "albumCount" },
        { TrackCountRole, "trackCount" },
    };
}

void ArtistListModel::reload()
{
    const int previousCount = m_artists.size();
    beginResetModel();
    m_artists = LibraryRepository(m_database).artists();
    endResetModel();
    qInfo() << "Loaded artist model rows" << m_artists.size();
    if (previousCount != m_artists.size()) {
        emit countChanged();
    }
}

} // namespace opusora
