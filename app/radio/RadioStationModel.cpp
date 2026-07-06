#include "radio/RadioStationModel.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QUrl>

namespace opusora {

RadioStationModel::RadioStationModel(QObject* parent)
    : QAbstractListModel(parent)
{
    load();
}

int RadioStationModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_stations.size();
}

int RadioStationModel::count() const
{
    return m_stations.size();
}

QVariant RadioStationModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_stations.size()) {
        return {};
    }

    const RadioStation& station = m_stations.at(index.row());
    switch (role) {
    case NameRole:
        return station.name;
    case UrlRole:
        return station.url;
    case Qt::DisplayRole:
        return station.name;
    default:
        return {};
    }
}

QHash<int, QByteArray> RadioStationModel::roleNames() const
{
    return {
        { NameRole, "name" },
        { UrlRole, "url" },
    };
}

bool RadioStationModel::addStation(const QString& name, const QString& url)
{
    const QString trimmedUrl = url.trimmed();
    if (!isValidStreamUrl(trimmedUrl)) {
        return false;
    }

    QString trimmedName = name.trimmed();
    if (trimmedName.isEmpty()) {
        trimmedName = QUrl(trimmedUrl).host();
    }
    if (trimmedName.isEmpty()) {
        return false;
    }

    beginInsertRows(QModelIndex(), m_stations.size(), m_stations.size());
    m_stations.append(RadioStation { trimmedName, trimmedUrl });
    endInsertRows();
    save();
    emit countChanged();
    return true;
}

bool RadioStationModel::removeStation(int row)
{
    if (row < 0 || row >= m_stations.size()) {
        return false;
    }

    beginRemoveRows(QModelIndex(), row, row);
    m_stations.removeAt(row);
    endRemoveRows();
    save();
    emit countChanged();
    return true;
}

QString RadioStationModel::stationName(int row) const
{
    return row >= 0 && row < m_stations.size() ? m_stations.at(row).name : QString();
}

QString RadioStationModel::stationUrl(int row) const
{
    return row >= 0 && row < m_stations.size() ? m_stations.at(row).url : QString();
}

void RadioStationModel::load()
{
    const QByteArray data = QSettings().value(QStringLiteral("radio/stations")).toString().toUtf8();
    if (data.isEmpty()) {
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(data);
    if (!document.isArray()) {
        return;
    }

    QList<RadioStation> stations;
    const QJsonArray array = document.array();
    for (const QJsonValue& value : array) {
        const QJsonObject object = value.toObject();
        RadioStation station {
            object.value(QStringLiteral("name")).toString().trimmed(),
            object.value(QStringLiteral("url")).toString().trimmed(),
        };
        if (!station.name.isEmpty() && isValidStreamUrl(station.url)) {
            stations.append(station);
        }
    }

    if (stations.isEmpty()) {
        return;
    }

    beginResetModel();
    m_stations = std::move(stations);
    endResetModel();
    emit countChanged();
}

void RadioStationModel::save() const
{
    QJsonArray array;
    for (const RadioStation& station : m_stations) {
        QJsonObject object;
        object.insert(QStringLiteral("name"), station.name);
        object.insert(QStringLiteral("url"), station.url);
        array.append(object);
    }

    QSettings().setValue(
        QStringLiteral("radio/stations"),
        QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact)));
}

bool RadioStationModel::isValidStreamUrl(const QString& url) const
{
    const QUrl parsed(url);
    return parsed.isValid()
        && (parsed.scheme() == QStringLiteral("http") || parsed.scheme() == QStringLiteral("https"))
        && !parsed.host().isEmpty();
}

} // namespace opusora
