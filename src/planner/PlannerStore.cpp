/**
 * @file PlannerStore.cpp
 * @brief Local copy of the planner's Fertigstände (SQLite)
 */
#include "multipack/planner/PlannerStore.h"

#include "multipack/database/DatabaseManager.h"
#include "multipack/system/RobFileParser.h"
#include "multipack/system/RobPaletteConversion.h"

#include <QDateTime>
#include <QDebug>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimeZone>
#include <QVariant>

namespace multipack {
namespace planner {

namespace {

StoredFinal readRow(const QSqlQuery& query)
{
    StoredFinal stored;
    stored.entry.id = query.value(0).toString();
    stored.entry.recordId = query.value(1).toString();
    stored.entry.name = query.value(2).toString();
    stored.entry.fileName = query.value(3).toString();
    stored.entry.sha256 = query.value(4).toString();
    const QVariant markedAt = query.value(5);
    if (!markedAt.isNull()) {
        stored.entry.markedAt = QDateTime::fromMSecsSinceEpoch(markedAt.toLongLong(), QTimeZone::utc());
    }
    stored.inList = query.value(6).toInt() != 0;
    stored.downloaded = query.value(7).toInt() != 0;
    stored.loadError = query.value(8).toString();
    return stored;
}

QVariant markedAtValue(const FinalEntry& entry)
{
    return entry.markedAt.isValid() ? QVariant(entry.markedAt.toMSecsSinceEpoch()) : QVariant();
}

constexpr const char* SELECT_COLUMNS =
    "SELECT final_id, record_id, name, file_name, sha256, marked_at, in_list, "
    "(rob IS NOT NULL), load_error FROM planner_finals";

} // namespace

QString plannerPaletteFileName(const QString& finalId)
{
    return QString::fromLatin1(PLANNER_FILE_PREFIX) + finalId + QStringLiteral(".rob");
}

QString finalIdFromPaletteFileName(const QString& fileName)
{
    const QString prefix = QString::fromLatin1(PLANNER_FILE_PREFIX);
    if (!fileName.startsWith(prefix) || !fileName.endsWith(QStringLiteral(".rob"))) {
        return QString();
    }
    return fileName.mid(prefix.size(), fileName.size() - prefix.size() - 4);
}

PlannerStore::PlannerStore() = default;

PlannerStore::~PlannerStore()
{
    close();
}

bool PlannerStore::open(const QString& databasePath)
{
    close();
    m_database = std::make_unique<database::DatabaseManager>();
    if (!m_database->open(databasePath)) {
        m_database.reset();
        return false;
    }
    if (!createTables()) {
        close();
        return false;
    }
    return true;
}

bool PlannerStore::isOpen() const
{
    return m_database && m_database->isOpen();
}

void PlannerStore::close()
{
    if (m_database) {
        m_database->close();
        m_database.reset();
    }
}

bool PlannerStore::createTables()
{
    QSqlQuery query(m_database->connection());
    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS planner_finals (
            final_id TEXT PRIMARY KEY,
            record_id TEXT NOT NULL,
            name TEXT NOT NULL,
            file_name TEXT,
            sha256 TEXT NOT NULL,
            marked_at INTEGER,
            in_list INTEGER NOT NULL DEFAULT 1,
            rob BLOB,
            stored_at INTEGER,
            load_error TEXT
        )
    )")) {
        qCritical() << "Failed to create planner_finals:" << query.lastError().text();
        return false;
    }
    // Tables created before load_error existed: add it (fails harmlessly if present).
    query.exec("ALTER TABLE planner_finals ADD COLUMN load_error TEXT");
    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS planner_state (
            key TEXT PRIMARY KEY,
            value TEXT
        )
    )")) {
        qCritical() << "Failed to create planner_state:" << query.lastError().text();
        return false;
    }
    return true;
}

bool PlannerStore::applyList(const QVector<FinalEntry>& entries)
{
    if (!isOpen()) {
        return false;
    }
    QSqlDatabase db = m_database->connection();
    if (!db.transaction()) {
        qWarning() << "Planner store: cannot start transaction:" << db.lastError().text();
        return false;
    }

    QSqlQuery query(db);
    if (!query.exec("UPDATE planner_finals SET in_list = 0")) {
        db.rollback();
        return false;
    }

    for (const FinalEntry& entry : entries) {
        // A Fertigstand is immutable; if the checksum for a known id ever
        // differs, drop the stored bytes so they are fetched and checked again.
        query.prepare(R"(
            INSERT INTO planner_finals (final_id, record_id, name, file_name, sha256, marked_at, in_list)
            VALUES (?, ?, ?, ?, ?, ?, 1)
            ON CONFLICT(final_id) DO UPDATE SET
                record_id = excluded.record_id,
                name = excluded.name,
                file_name = excluded.file_name,
                marked_at = excluded.marked_at,
                in_list = 1,
                rob = CASE WHEN planner_finals.sha256 = excluded.sha256 THEN planner_finals.rob ELSE NULL END,
                load_error = CASE WHEN planner_finals.sha256 = excluded.sha256
                                  THEN planner_finals.load_error ELSE NULL END,
                sha256 = excluded.sha256
        )");
        query.addBindValue(entry.id);
        query.addBindValue(entry.recordId);
        query.addBindValue(entry.name);
        query.addBindValue(entry.fileName);
        query.addBindValue(entry.sha256);
        query.addBindValue(markedAtValue(entry));
        if (!query.exec()) {
            qWarning() << "Planner store: cannot upsert final:" << query.lastError().text();
            db.rollback();
            return false;
        }
    }

    if (!db.commit()) {
        qWarning() << "Planner store: commit failed:" << db.lastError().text();
        db.rollback();
        return false;
    }
    return true;
}

QVector<FinalEntry> PlannerStore::pendingDownloads() const
{
    QVector<FinalEntry> result;
    if (!isOpen()) {
        return result;
    }
    QSqlQuery query(m_database->connection());
    if (!query.exec(QString::fromLatin1(SELECT_COLUMNS) + " WHERE in_list = 1 AND rob IS NULL ORDER BY name")) {
        return result;
    }
    while (query.next()) {
        result.append(readRow(query).entry);
    }
    return result;
}

StoreOutcome PlannerStore::storeRob(const FinalEntry& entry, const QByteArray& bytes, QString* error)
{
    auto fail = [error](StoreOutcome outcome, const QString& message) {
        if (error) {
            *error = message;
        }
        return outcome;
    };

    if (!isOpen()) {
        return fail(StoreOutcome::DatabaseError, QStringLiteral("Datenbank nicht geöffnet"));
    }

    // 1. Checksum over the raw bytes.
    if (!sha256Matches(entry.sha256, bytes)) {
        recordLoadError(entry, QString::fromLatin1(LOAD_ERROR_CHECKSUM));
        return fail(StoreOutcome::ShaMismatch, QStringLiteral("Prüfsumme stimmt nicht"));
    }

    // 2. The HMI's own .rob parser must accept it before anything is stored.
    RobFileParser parser;
    const RobFileData parsed = parser.parseBytes(bytes, entry.fileName.isEmpty() ? entry.id : entry.fileName);
    if (!parsed.isValid) {
        recordLoadError(entry, QString::fromLatin1(LOAD_ERROR_INVALID_ROB));
        return fail(StoreOutcome::InvalidRob, parsed.errorMessage);
    }

    // 3. Palette rows (what the HMI loads and the robot reads) ...
    database::PaletteData paletteData = system::robFileToPaletteData(parsed);
    paletteData.metadata.fileName = plannerPaletteFileName(entry.id);
    paletteData.metadata.fileTimestamp = entry.markedAt.isValid()
        ? entry.markedAt.toMSecsSinceEpoch()
        : QDateTime::currentMSecsSinceEpoch();
    // Replace any earlier rows for this id (savePaletteData keeps a row whose
    // timestamp is not older, which would hide re-fetched bytes).
    (void)m_database->deletePalette(paletteData.metadata.fileName);
    if (m_database->savePaletteData(paletteData) == database::SaveResult::Error) {
        return fail(StoreOutcome::DatabaseError, QStringLiteral("Speichern fehlgeschlagen"));
    }

    // 4. ... and the unchanged bytes plus metadata.
    QSqlQuery query(m_database->connection());
    query.prepare(R"(
        INSERT INTO planner_finals (final_id, record_id, name, file_name, sha256, marked_at, in_list, rob, stored_at)
        VALUES (?, ?, ?, ?, ?, ?, 0, ?, ?)
        ON CONFLICT(final_id) DO UPDATE SET
            rob = excluded.rob,
            sha256 = excluded.sha256,
            stored_at = excluded.stored_at,
            load_error = NULL
    )");
    // NOT NULL columns: a proposal-only entry has no recordId until the next list.
    query.addBindValue(entry.id);
    query.addBindValue(entry.recordId.isNull() ? QStringLiteral("") : entry.recordId);
    query.addBindValue(entry.name.isNull() ? QStringLiteral("") : entry.name);
    query.addBindValue(entry.fileName);
    query.addBindValue(entry.sha256.toLower());
    query.addBindValue(markedAtValue(entry));
    query.addBindValue(bytes);
    query.addBindValue(QDateTime::currentMSecsSinceEpoch());
    if (!query.exec()) {
        qWarning() << "Planner store: cannot store rob:" << query.lastError().text();
        return fail(StoreOutcome::DatabaseError, QStringLiteral("Speichern fehlgeschlagen"));
    }
    return StoreOutcome::Stored;
}

void PlannerStore::recordLoadError(const FinalEntry& entry, const QString& code)
{
    // Kept with the entry so the list shows the error (also after a restart
    // while offline) instead of "wird geladen" forever. A new list entry with
    // a different checksum clears it (applyList), a successful store too.
    QSqlQuery query(m_database->connection());
    query.prepare(R"(
        INSERT INTO planner_finals (final_id, record_id, name, file_name, sha256, marked_at, in_list, load_error)
        VALUES (?, ?, ?, ?, ?, ?, 0, ?)
        ON CONFLICT(final_id) DO UPDATE SET load_error = excluded.load_error
    )");
    query.addBindValue(entry.id);
    query.addBindValue(entry.recordId.isNull() ? QStringLiteral("") : entry.recordId);
    query.addBindValue(entry.name.isNull() ? QStringLiteral("") : entry.name);
    query.addBindValue(entry.fileName);
    query.addBindValue(entry.sha256.toLower());
    query.addBindValue(markedAtValue(entry));
    query.addBindValue(code);
    if (!query.exec()) {
        qWarning() << "Planner store: cannot record load error:" << query.lastError().text();
    }
}

QVector<StoredFinal> PlannerStore::finals() const
{
    QVector<StoredFinal> result;
    if (!isOpen()) {
        return result;
    }
    QSqlQuery query(m_database->connection());
    if (!query.exec(QString::fromLatin1(SELECT_COLUMNS)
                    + " ORDER BY in_list DESC, name COLLATE NOCASE, final_id")) {
        return result;
    }
    while (query.next()) {
        result.append(readRow(query));
    }
    return result;
}

std::optional<StoredFinal> PlannerStore::find(const QString& finalId) const
{
    if (!isOpen() || finalId.isEmpty()) {
        return std::nullopt;
    }
    QSqlQuery query(m_database->connection());
    query.prepare(QString::fromLatin1(SELECT_COLUMNS) + " WHERE final_id = ?");
    query.addBindValue(finalId);
    if (!query.exec() || !query.next()) {
        return std::nullopt;
    }
    return readRow(query);
}

QByteArray PlannerStore::robBytes(const QString& finalId) const
{
    if (!isOpen()) {
        return QByteArray();
    }
    QSqlQuery query(m_database->connection());
    query.prepare("SELECT rob FROM planner_finals WHERE final_id = ?");
    query.addBindValue(finalId);
    if (!query.exec() || !query.next()) {
        return QByteArray();
    }
    return query.value(0).toByteArray();
}

QString PlannerStore::finalsEtag() const
{
    if (!isOpen()) {
        return QString();
    }
    QSqlQuery query(m_database->connection());
    if (!query.exec("SELECT value FROM planner_state WHERE key = 'finals_etag'") || !query.next()) {
        return QString();
    }
    return query.value(0).toString();
}

void PlannerStore::setFinalsEtag(const QString& etag)
{
    if (!isOpen()) {
        return;
    }
    QSqlQuery query(m_database->connection());
    query.prepare("INSERT INTO planner_state (key, value) VALUES ('finals_etag', ?) "
                  "ON CONFLICT(key) DO UPDATE SET value = excluded.value");
    query.addBindValue(etag);
    if (!query.exec()) {
        qWarning() << "Planner store: cannot save ETag:" << query.lastError().text();
    }
}

} // namespace planner
} // namespace multipack
