/**
 * @file DatabaseMigrations.cpp
 * @brief Implementation of database schema migration system
 */
#include "multipack/database/DatabaseMigrations.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QDebug>

namespace multipack {
namespace database {
namespace DatabaseMigrations {

namespace {

/**
 * @brief Create the schema version table if it doesn't exist
 */
bool createVersionTable(QSqlDatabase& db)
{
    QSqlQuery query(db);
    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS schema_version (
            version INTEGER PRIMARY KEY
        )
    )")) {
        qCritical() << "Failed to create schema_version table:"
                    << query.lastError().text();
        return false;
    }
    return true;
}

/**
 * @brief Migration from version 0 to 1
 * Initial schema - adds weight column to paket_dim if missing
 */
bool migrateV0toV1(QSqlDatabase& db)
{
    qDebug() << "Running migration v0 -> v1";

    // Add weight column to paket_dim if it doesn't exist
    if (!addColumnIfNotExists(db, "paket_dim", "weight", "REAL")) {
        return false;
    }

    return true;
}

/**
 * @brief Migration from version 1 to 2
 * Adds einzelpaket_laengs column to paket_dim
 */
bool migrateV1toV2(QSqlDatabase& db)
{
    qDebug() << "Running migration v1 -> v2";

    // Add einzelpaket_laengs column to paket_dim if it doesn't exist
    if (!addColumnIfNotExists(db, "paket_dim", "einzelpaket_laengs", "INTEGER")) {
        return false;
    }

    return true;
}

} // anonymous namespace

bool runMigrations(QSqlDatabase& db)
{
    if (!db.isOpen()) {
        qCritical() << "Cannot run migrations - database not open";
        return false;
    }

    // Ensure version table exists
    if (!createVersionTable(db)) {
        return false;
    }

    int currentVersion = getCurrentVersion(db);
    qDebug() << "Database schema version:" << currentVersion
             << "Target version:" << CURRENT_VERSION;

    if (currentVersion >= CURRENT_VERSION) {
        qDebug() << "Database is up to date";
        return true;
    }

    // Run migrations in sequence
    bool success = true;

    if (currentVersion < 1 && success) {
        success = migrateV0toV1(db);
        if (success) setVersion(db, 1);
    }

    if (currentVersion < 2 && success) {
        success = migrateV1toV2(db);
        if (success) setVersion(db, 2);
    }

    // Add future migrations here:
    // if (currentVersion < 3 && success) {
    //     success = migrateV2toV3(db);
    //     if (success) setVersion(db, 3);
    // }

    if (success) {
        qDebug() << "Database migrations completed successfully";
    } else {
        qCritical() << "Database migration failed";
    }

    return success;
}

int getCurrentVersion(QSqlDatabase& db)
{
    QSqlQuery query(db);

    // Check if version table exists
    if (!query.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='schema_version'")) {
        return 0;
    }

    if (!query.next()) {
        // Version table doesn't exist - legacy database
        return 0;
    }

    // Get current version
    query.clear();
    if (!query.exec("SELECT MAX(version) FROM schema_version")) {
        return 0;
    }

    if (query.next() && !query.value(0).isNull()) {
        return query.value(0).toInt();
    }

    return 0;
}

bool setVersion(QSqlDatabase& db, int version)
{
    QSqlQuery query(db);

    // Delete existing version entries
    query.exec("DELETE FROM schema_version");

    // Insert new version
    query.prepare("INSERT INTO schema_version (version) VALUES (:version)");
    query.bindValue(":version", version);

    if (!query.exec()) {
        qCritical() << "Failed to set schema version:"
                    << query.lastError().text();
        return false;
    }

    qDebug() << "Schema version set to:" << version;
    return true;
}

bool columnExists(QSqlDatabase& db, const QString& table, const QString& column)
{
    QSqlQuery query(db);

    // Use PRAGMA to get table info
    if (!query.exec(QString("PRAGMA table_info(%1)").arg(table))) {
        qWarning() << "Failed to get table info for" << table;
        return false;
    }

    while (query.next()) {
        QString colName = query.value(1).toString();
        if (colName.compare(column, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }

    return false;
}

bool addColumnIfNotExists(QSqlDatabase& db, const QString& table,
                          const QString& column, const QString& type)
{
    if (columnExists(db, table, column)) {
        qDebug() << "Column" << column << "already exists in" << table;
        return true;
    }

    QSqlQuery query(db);
    QString sql = QString("ALTER TABLE %1 ADD COLUMN %2 %3")
                      .arg(table, column, type);

    if (!query.exec(sql)) {
        qCritical() << "Failed to add column" << column << "to" << table
                    << ":" << query.lastError().text();
        return false;
    }

    qDebug() << "Added column" << column << "to table" << table;
    return true;
}

} // namespace DatabaseMigrations
} // namespace database
} // namespace multipack
