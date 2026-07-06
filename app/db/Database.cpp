#include "db/Database.hpp"

#include <QDebug>
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

#include <utility>

namespace {

QStringList splitSqlStatements(const QString& script)
{
    QStringList statements;
    QString current;

    const auto lines = script.split(QLatin1Char('\n'));
    for (QString line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QStringLiteral("--")) || trimmed.isEmpty()) {
            continue;
        }

        current += line;
        current += QLatin1Char('\n');
        if (trimmed.endsWith(QLatin1Char(';'))) {
            statements.append(current.trimmed());
            current.clear();
        }
    }

    if (!current.trimmed().isEmpty()) {
        statements.append(current.trimmed());
    }

    return statements;
}

} // namespace

namespace opusora {

Database::Database(QString databasePath, QObject* parent)
    : QObject(parent)
    , m_connectionName(QStringLiteral("opusora-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
    , m_databasePath(std::move(databasePath))
{
}

Database::~Database()
{
    if (QSqlDatabase::contains(m_connectionName)) {
        QSqlDatabase database = QSqlDatabase::database(m_connectionName);
        database.close();
        database = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

bool Database::open()
{
    if (isOpen()) {
        return true;
    }

    QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    database.setDatabaseName(m_databasePath);

    if (!database.open()) {
        m_lastError = database.lastError().text();
        qWarning() << "Failed to open database" << m_databasePath << m_lastError;
        return false;
    }

    QSqlQuery pragma(database);
    if (!pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
        m_lastError = pragma.lastError().text();
        qWarning() << "Failed to enable SQLite foreign keys" << m_lastError;
        return false;
    }

    qInfo() << "Database opened at" << m_databasePath;
    return true;
}

bool Database::migrate()
{
    if (!open()) {
        return false;
    }

    const QString migration = readResourceText(QStringLiteral(":/app/db/migrations/001_initial.sql"));
    if (migration.isEmpty()) {
        return false;
    }

    if (!executeScript(migration)) {
        return false;
    }

    if (!ensureColumn(QStringLiteral("track"), QStringLiteral("replaygain_track_gain"), QStringLiteral("REAL"))
        || !ensureColumn(QStringLiteral("track"), QStringLiteral("replaygain_album_gain"), QStringLiteral("REAL"))) {
        return false;
    }

    QSqlQuery query(connection());
    if (!query.exec(QStringLiteral("PRAGMA user_version = 2"))) {
        m_lastError = query.lastError().text();
        qWarning() << "Failed to set database user_version" << m_lastError;
        return false;
    }

    qInfo() << "Database migration complete";
    return true;
}

bool Database::isOpen() const
{
    return QSqlDatabase::contains(m_connectionName)
        && QSqlDatabase::database(m_connectionName).isOpen();
}

QSqlDatabase Database::connection() const
{
    return QSqlDatabase::database(m_connectionName);
}

QString Database::lastError() const
{
    return m_lastError;
}

bool Database::executeScript(const QString& script)
{
    QSqlDatabase database = connection();
    if (!database.transaction()) {
        m_lastError = database.lastError().text();
        qWarning() << "Failed to start migration transaction" << m_lastError;
        return false;
    }

    for (const QString& statement : splitSqlStatements(script)) {
        QSqlQuery query(database);
        if (!query.exec(statement)) {
            m_lastError = query.lastError().text();
            qWarning() << "Migration statement failed" << m_lastError << statement;
            database.rollback();
            return false;
        }
    }

    if (!database.commit()) {
        m_lastError = database.lastError().text();
        qWarning() << "Failed to commit migration transaction" << m_lastError;
        return false;
    }

    return true;
}

bool Database::ensureColumn(const QString& tableName, const QString& columnName, const QString& definition)
{
    QSqlQuery infoQuery(connection());
    if (!infoQuery.exec(QStringLiteral("PRAGMA table_info(%1)").arg(tableName))) {
        m_lastError = infoQuery.lastError().text();
        qWarning() << "Failed to inspect table columns" << tableName << m_lastError;
        return false;
    }

    while (infoQuery.next()) {
        if (infoQuery.value(1).toString() == columnName) {
            return true;
        }
    }

    QSqlQuery alterQuery(connection());
    const QString statement = QStringLiteral("ALTER TABLE %1 ADD COLUMN %2 %3")
                                  .arg(tableName, columnName, definition);
    if (!alterQuery.exec(statement)) {
        m_lastError = alterQuery.lastError().text();
        qWarning() << "Failed to add database column" << tableName << columnName << m_lastError;
        return false;
    }

    return true;
}

QString Database::readResourceText(const QString& resourcePath)
{
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_lastError = file.errorString();
        qWarning() << "Failed to read resource" << resourcePath << file.errorString();
        return {};
    }

    return QString::fromUtf8(file.readAll());
}

} // namespace opusora
