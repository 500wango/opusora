#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QString>

namespace opusora {

struct RadioStation {
    QString name;
    QString url;
};

class RadioStationModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        UrlRole,
    };

    explicit RadioStationModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int count() const;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE bool addStation(const QString& name, const QString& url);
    Q_INVOKABLE bool removeStation(int row);
    Q_INVOKABLE QString stationName(int row) const;
    Q_INVOKABLE QString stationUrl(int row) const;

signals:
    void countChanged();

private:
    void load();
    void save() const;
    bool isValidStreamUrl(const QString& url) const;

    QList<RadioStation> m_stations;
};

} // namespace opusora
