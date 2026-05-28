/**
 * @file DatabaseMigrations.h
 * @brief Database schema migration system
 */
#ifndef MULTIPACK_DATABASE_DATABASEMIGRATIONS_H
#define MULTIPACK_DATABASE_DATABASEMIGRATIONS_H

#include <QSqlDatabase>
#include <functional>
#include <vector>

namespace multipack {
namespace database {

/**
 * @namespace DatabaseMigrations
 * @brief Handles database schema migrations
 *
 * Provides a versioned migration system that tracks schema changes
 * and applies them automatically when opening older databases.
 */
namespace DatabaseMigrations {

/// Current schema version
constexpr int CURRENT_VERSION = 2;

/**
 * @brief Run all pending migrations
 * @param db Database connection
 * @return true if all migrations succeeded
 */
bool runMigrations(QSqlDatabase& db);

/**
 * @brief Get current schema version from database
 * @param db Database connection
 * @return Version number, 0 if no version table exists
 */
int getCurrentVersion(QSqlDatabase& db);

/**
 * @brief Set the schema version in the database
 * @param db Database connection
 * @param version New version number
 * @return true if successful
 */
bool setVersion(QSqlDatabase& db, int version);

/**
 * @brief Check if a column exists in a table
 * @param db Database connection
 * @param table Table name
 * @param column Column name
 * @return true if column exists
 */
bool columnExists(QSqlDatabase& db, const QString& table, const QString& column);

/**
 * @brief Add a column to a table if it doesn't exist
 * @param db Database connection
 * @param table Table name
 * @param column Column name
 * @param type Column type (e.g., "INTEGER", "REAL", "TEXT")
 * @return true if column was added or already exists
 */
bool addColumnIfNotExists(QSqlDatabase& db, const QString& table,
                          const QString& column, const QString& type);

} // namespace DatabaseMigrations
} // namespace database
} // namespace multipack

#endif // MULTIPACK_DATABASE_DATABASEMIGRATIONS_H
