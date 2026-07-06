#pragma once

#include "library/LibraryRepository.hpp"

#include <QAbstractListModel>
#include <QList>

namespace opusora {

class Database;

class GenreListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        TrackCountRole,
        DurationMsRole,
    };

    explicit GenreListModel(Database* database, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int count() const;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void reload();

signals:
    void countChanged();

private:
    Database* m_database = nullptr;
    QList<GenreRow> m_genres;
};

} // namespace opusora
