#include "library/GenreListModel.hpp"

#include "db/Database.hpp"

#include <QDebug>

namespace opusora {

GenreListModel::GenreListModel(Database* database, QObject* parent)
    : QAbstractListModel(parent)
    , m_database(database)
{
    reload();
}

int GenreListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_genres.size();
}

int GenreListModel::count() const
{
    return m_genres.size();
}

QVariant GenreListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_genres.size()) {
        return {};
    }

    const GenreRow& genre = m_genres.at(index.row());
    switch (role) {
    case NameRole:
        return genre.name;
    case TrackCountRole:
        return genre.trackCount;
    case DurationMsRole:
        return genre.durationMs;
    case Qt::DisplayRole:
        return genre.name;
    default:
        return {};
    }
}

QHash<int, QByteArray> GenreListModel::roleNames() const
{
    return {
        { NameRole, "name" },
        { TrackCountRole, "trackCount" },
        { DurationMsRole, "durationMs" },
    };
}

void GenreListModel::reload()
{
    const int previousCount = m_genres.size();
    beginResetModel();
    m_genres = LibraryRepository(m_database).genres();
    endResetModel();
    qInfo() << "Loaded genre model rows" << m_genres.size();
    if (previousCount != m_genres.size()) {
        emit countChanged();
    }
}

} // namespace opusora
