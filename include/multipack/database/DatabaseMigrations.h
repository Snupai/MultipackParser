/**
 * @file DatabaseMigrations.h
 */
#ifndef MULTIPACK_DATABASE_DATABASEMIGRATIONS_H
#define MULTIPACK_DATABASE_DATABASEMIGRATIONS_H
class QSqlDatabase;
namespace multipack { namespace database { namespace DatabaseMigrations {
    bool runMigrations(QSqlDatabase& db);
    int getCurrentVersion(QSqlDatabase& db);
}}} // namespace
#endif
