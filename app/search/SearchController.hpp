#pragma once

#include "library/LibraryRepository.hpp"
#include "playlist/PlaylistRepository.hpp"

#include <QAbstractListModel>
#include <QList>
#include <QVariantList>

namespace opusora {

class Database;

class SearchController : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString query READ query NOTIFY queryChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY resultsChanged)
    Q_PROPERTY(QVariantList groupedItems READ groupedItems NOTIFY resultsChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        ArtistRole,
        AlbumRole,
        FilePathRole,
        CoverUrlRole,
        FormatRole,
        DurationMsRole,
        TrackNumberRole,
        DiscNumberRole,
    };

    explicit SearchController(Database* database, QObject* parent = nullptr);

    QString query() const;
    int count() const;
    int totalCount() const;
    QVariantList groupedItems() const;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void search(const QString& query);
    Q_INVOKABLE void reload();
    Q_INVOKABLE QVariantList queueItems() const;
    Q_INVOKABLE QVariantList queueItemsForResult(const QString& resultType, int id) const;

signals:
    void queryChanged();
    void countChanged();
    void resultsChanged();

private:
    Database* m_database = nullptr;
    QString m_query;
    QList<TrackRow> m_results;
    QList<AlbumRow> m_albumResults;
    QList<ArtistRow> m_artistResults;
    QList<PlaylistRow> m_playlistResults;
};

} // namespace opusora
