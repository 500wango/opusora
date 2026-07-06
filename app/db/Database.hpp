#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QString>

namespace opusora {

class Database : public QObject {
    Q_OBJECT

public:
    explicit Database(QString databasePath, QObject* parent = nullptr);
    ~Database() override;

    bool open();
    bool migrate();
    bool isOpen() const;

    QSqlDatabase connection() const;
    QString lastError() const;

private:
    bool executeScript(const QString& script);
    bool ensureColumn(const QString& tableName, const QString& columnName, const QString& definition);
    QString readResourceText(const QString& resourcePath);

    QString m_connectionName;
    QString m_databasePath;
    QString m_lastError;
};

} // namespace opusora
