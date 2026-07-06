#include "playlist/PlaylistListModel.hpp"

#include "db/Database.hpp"

#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>

namespace {

QString suggestedM3uFileName(const QString& playlistName)
{
    QString name = playlistName.trimmed();
    if (name.isEmpty()) {
        name = QStringLiteral("playlist");
    }

    const QString invalidCharacters = QStringLiteral("/\\:*?\"<>|");
    for (const QChar character : invalidCharacters) {
        name.replace(character, QLatin1Char('_'));
    }

    return name + QStringLiteral(".m3u8");
}

QString normalizedM3uPath(const QString& filePath)
{
    QString path = filePath.trimmed();
    if (path.isEmpty()) {
        return {};
    }

    const QString lower = path.toLower();
    if (!lower.endsWith(QStringLiteral(".m3u")) && !lower.endsWith(QStringLiteral(".m3u8"))) {
        path += QStringLiteral(".m3u8");
    }

    return path;
}

} // namespace

namespace opusora {

PlaylistListModel::PlaylistListModel(Database* database, QObject* parent)
    : QAbstractListModel(parent)
    , m_database(database)
{
    reload();
}

int PlaylistListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_playlists.size();
}

int PlaylistListModel::count() const
{
    return m_playlists.size();
}

QVariant PlaylistListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_playlists.size()) {
        return {};
    }

    const PlaylistRow& playlist = m_playlists.at(index.row());
    switch (role) {
    case IdRole:
        return playlist.id;
    case NameRole:
        return playlist.name;
    case TrackCountRole:
        return playlist.trackCount;
    case DurationMsRole:
        return playlist.durationMs;
    case Qt::DisplayRole:
        return playlist.name;
    default:
        return {};
    }
}

QHash<int, QByteArray> PlaylistListModel::roleNames() const
{
    return {
        { IdRole, "playlistId" },
        { NameRole, "name" },
        { TrackCountRole, "trackCount" },
        { DurationMsRole, "durationMs" },
    };
}

void PlaylistListModel::reload()
{
    const int previousCount = m_playlists.size();
    beginResetModel();
    m_playlists = PlaylistRepository(m_database).playlists();
    endResetModel();
    qInfo() << "Loaded playlist model rows" << m_playlists.size();
    if (previousCount != m_playlists.size()) {
        emit countChanged();
    }
}

bool PlaylistListModel::createPlaylist(const QString& name)
{
    if (!PlaylistRepository(m_database).createPlaylist(name)) {
        return false;
    }

    reload();
    return true;
}

bool PlaylistListModel::renamePlaylist(int playlistId, const QString& name)
{
    if (!PlaylistRepository(m_database).renamePlaylist(playlistId, name)) {
        return false;
    }

    reload();
    emit playlistChanged(playlistId);
    return true;
}

bool PlaylistListModel::deletePlaylist(int playlistId)
{
    if (!PlaylistRepository(m_database).deletePlaylist(playlistId)) {
        return false;
    }

    reload();
    emit playlistChanged(playlistId);
    return true;
}

bool PlaylistListModel::exportM3u(int playlistId, const QString& filePath)
{
    return PlaylistRepository(m_database).exportM3u(playlistId, normalizedM3uPath(filePath));
}

bool PlaylistListModel::requestExportM3u(int playlistId, const QString& playlistName)
{
    if (playlistId <= 0) {
        return false;
    }

    const QString defaultPath = QDir::home().absoluteFilePath(suggestedM3uFileName(playlistName));
    const QString selectedPath = QFileDialog::getSaveFileName(
        nullptr,
        tr("Export Playlist"),
        defaultPath,
        tr("M3U Playlist (*.m3u8 *.m3u)"));
    if (selectedPath.isEmpty()) {
        return false;
    }

    return exportM3u(playlistId, selectedPath);
}

bool PlaylistListModel::importM3u(const QString& filePath, const QString& playlistName)
{
    if (!PlaylistRepository(m_database).importM3u(filePath, playlistName)) {
        return false;
    }

    reload();
    return true;
}

bool PlaylistListModel::requestImportM3u()
{
    const QString selectedPath = QFileDialog::getOpenFileName(
        nullptr,
        tr("Import Playlist"),
        QDir::homePath(),
        tr("M3U Playlist (*.m3u8 *.m3u)"));
    if (selectedPath.isEmpty()) {
        return false;
    }

    return importM3u(selectedPath);
}

bool PlaylistListModel::addTrack(int playlistId, int trackId)
{
    if (!PlaylistRepository(m_database).addTrack(playlistId, trackId)) {
        return false;
    }

    reload();
    emit playlistChanged(playlistId);
    return true;
}

int PlaylistListModel::addTracks(int playlistId, const QVariantList& trackIds)
{
    QList<int> normalizedTrackIds;
    normalizedTrackIds.reserve(trackIds.size());
    for (const QVariant& trackIdVariant : trackIds) {
        bool ok = false;
        const int trackId = trackIdVariant.toInt(&ok);
        if (ok && trackId > 0) {
            normalizedTrackIds.append(trackId);
        }
    }

    const int added = PlaylistRepository(m_database).addTracks(playlistId, normalizedTrackIds);
    if (added <= 0) {
        return 0;
    }

    reload();
    emit playlistChanged(playlistId);
    return added;
}

bool PlaylistListModel::removeTrack(int playlistId, int trackId)
{
    if (!PlaylistRepository(m_database).removeTrack(playlistId, trackId)) {
        return false;
    }

    reload();
    emit playlistChanged(playlistId);
    return true;
}

bool PlaylistListModel::removeEntry(int playlistId, qint64 playlistEntryId)
{
    if (!PlaylistRepository(m_database).removeEntry(playlistId, playlistEntryId)) {
        return false;
    }

    reload();
    emit playlistChanged(playlistId);
    return true;
}

bool PlaylistListModel::moveEntry(int playlistId, qint64 playlistEntryId, int direction)
{
    if (!PlaylistRepository(m_database).moveEntry(playlistId, playlistEntryId, direction)) {
        return false;
    }

    reload();
    emit playlistChanged(playlistId);
    return true;
}

} // namespace opusora
