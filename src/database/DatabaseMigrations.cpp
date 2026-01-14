/**
 * @file DatabaseMigrations.cpp
 */
#include "multipack/database/DatabaseMigrations.h"
#include <QDebug>
namespace multipack { namespace database { namespace DatabaseMigrations {
bool runMigrations(QSqlDatabase& db) {
    Q_UNUSED(db);
    qDebug() << "DatabaseMigrations::runMigrations - TODO";
    return true;
}
int getCurrentVersion(QSqlDatabase& db) {
    Q_UNUSED(db);
    return 0;
}
}}} // namespace
