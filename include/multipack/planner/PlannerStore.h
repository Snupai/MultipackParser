/**
 * @file PlannerStore.h
 * @brief Local copy of the planner's Fertigstände (SQLite)
 *
 * Keeps every Fertigstand the HMI has seen in the table `planner_finals` of
 * the application database: list metadata, whether it is still in the
 * planner's list, and the downloaded .rob bytes (unchanged). Downloaded
 * Fertigstände are also stored as regular palette data under the name
 * `planner:<finalId>.rob`, so the existing load path and the robot's XML-RPC
 * calls work unchanged; USB plan lists never show those rows.
 *
 * Nothing is deleted: withdrawn entries are only hidden, so the running plan
 * stays available while the planner is unreachable.
 */
#ifndef MULTIPACK_PLANNER_PLANNERSTORE_H
#define MULTIPACK_PLANNER_PLANNERSTORE_H

#include "multipack/planner/PlannerProtocol.h"

#include <QString>
#include <QVector>

#include <memory>
#include <optional>

namespace multipack {
namespace database { class DatabaseManager; }

namespace planner {

/// Prefix of palette rows that come from the planner.
constexpr const char* PLANNER_FILE_PREFIX = "planner:";

/// Palette file name used for a Fertigstand in `paletten_metadata`.
[[nodiscard]] QString plannerPaletteFileName(const QString& finalId);

/// Fertigstand id for a palette file name, or empty when it is not a planner plan.
[[nodiscard]] QString finalIdFromPaletteFileName(const QString& fileName);

/**
 * @struct StoredFinal
 * @brief A Fertigstand as known locally
 */
struct StoredFinal {
    FinalEntry entry;
    bool inList = false;     ///< Present in the planner's latest list (false = withdrawn)
    bool downloaded = false; ///< .rob validated and stored
};

/**
 * @enum StoreOutcome
 */
enum class StoreOutcome {
    Stored,        ///< Validated and stored (or already stored)
    ShaMismatch,   ///< Bytes do not match the expected SHA-256
    InvalidRob,    ///< The .rob parser rejected the content
    DatabaseError
};

class PlannerStore
{
public:
    PlannerStore();
    ~PlannerStore();

    PlannerStore(const PlannerStore&) = delete;
    PlannerStore& operator=(const PlannerStore&) = delete;

    /// Open (and create tables in) the application database file.
    [[nodiscard]] bool open(const QString& databasePath);
    [[nodiscard]] bool isOpen() const;
    void close();

    /**
     * @brief Apply a finals list from the planner
     *
     * Inserts/updates every entry (marked as in the list) and marks every
     * other known Fertigstand as withdrawn. Downloaded bytes are kept.
     */
    [[nodiscard]] bool applyList(const QVector<FinalEntry>& entries);

    /// Entries that are in the list but not downloaded yet.
    [[nodiscard]] QVector<FinalEntry> pendingDownloads() const;

    /**
     * @brief Verify, validate and store downloaded .rob bytes
     *
     * Order: SHA-256 check, .rob parser, then storage. Nothing is written
     * unless both checks pass. An entry that is not known yet (e.g. from a
     * Planvorschlag that is newer than the last list) is added as not listed;
     * the next list marks it.
     */
    [[nodiscard]] StoreOutcome storeRob(const FinalEntry& entry, const QByteArray& bytes, QString* error = nullptr);

    /// All known Fertigstände, listed ones first by name.
    [[nodiscard]] QVector<StoredFinal> finals() const;

    [[nodiscard]] std::optional<StoredFinal> find(const QString& finalId) const;

    /// Raw .rob bytes as downloaded (empty when not downloaded).
    [[nodiscard]] QByteArray robBytes(const QString& finalId) const;

    [[nodiscard]] QString finalsEtag() const;
    void setFinalsEtag(const QString& etag);

private:
    [[nodiscard]] bool createTables();

    std::unique_ptr<database::DatabaseManager> m_database;
};

} // namespace planner
} // namespace multipack

#endif // MULTIPACK_PLANNER_PLANNERSTORE_H
