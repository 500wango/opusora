#pragma once

#include "playlist/PlaylistRepository.hpp"

#include <QAbstractListModel>
#include <QList>
#include <QVariantList>

namespace opusora {

class Database;

class PlaylistListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        NameRole,
        TrackCountRole,
        DurationMsRole,
    };

    explicit PlaylistListModel(Database* database, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int count() const;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void reload();
    Q_INVOKABLE bool createPlaylist(const QString& name);
    Q_INVOKABLE bool renamePlaylist(int playlistId, const QString& name);
    Q_INVOKABLE bool deletePlaylist(int playlistId);
    Q_INVOKABLE bool exportM3u(int playlistId, const QString& filePath);
    Q_INVOKABLE bool requestExportM3u(int playlistId, const QString& playlistName);
    Q_INVOKABLE bool importM3u(const QString& filePath, const QString& playlistName = QString());
    Q_INVOKABLE bool requestImportM3u();
    Q_INVOKABLE bool addTrack(int playlistId, int trackId);
    Q_INVOKABLE int addTracks(int playlistId, const QVariantList& trackIds);
    Q_INVOKABLE bool removeTrack(int playlistId, int trackId);
    Q_INVOKABLE bool removeEntry(int playlistId, qint64 playlistEntryId);
    Q_INVOKABLE bool moveEntry(int playlistId, qint64 playlistEntryId, int direction);

signals:
    void countChanged();
    void playlistChanged(int playlistId);

private:
    Database* m_database = nullptr;
    QList<PlaylistRow> m_playlists;
};

} // namespace opusora
