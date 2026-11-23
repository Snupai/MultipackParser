/**
 * @file PlannerPlanList.h
 * @brief What the HMI shows of the planner's Fertigstände (pure logic)
 *
 * - Fertigstände still in the planner's list are shown.
 * - Withdrawn ones are hidden, except the running (active) one, which stays
 *   until the operator switches.
 * - When the planner has a newer Fertigstand of the running plan (same
 *   recordId, different id), the HMI shows "Neue Version verfuegbar".
 * - A plan (recordId) is shown as its newest Fertigstand. An older one is
 *   shown as well only when it is needed: the running one, and the newest
 *   one that can be loaded while no newer one can.
 * - A Planvorschlag may only be taken over while the robot is not executing
 *   a program.
 */
#ifndef MULTIPACK_PLANNER_PLANNERPLANLIST_H
#define MULTIPACK_PLANNER_PLANNERPLANLIST_H

#include "multipack/planner/PlannerStore.h"
#include "multipack/robot/RobotEnums.h"

#include <QDateTime>
#include <QString>
#include <QVector>

#include <optional>

namespace multipack {
namespace planner {

struct PlanListRow {
    StoredFinal final;
    bool isActive = false;          ///< This Fertigstand is the running plan
    bool isWithdrawn = false;       ///< No longer in the planner's list (only shown when active)
    bool isNewVersionOfActive = false; ///< Newer Fertigstand of the running plan
    int versionNumber = 1;          ///< 1 = oldest visible Fertigstand of this plan (recordId)
    int versionCount = 1;           ///< Visible Fertigstände of this plan, shown or not
    bool isRecent = false;          ///< Marked as finished within RECENT_WINDOW_MS before `now`
};

/// A Fertigstand counts as new ("Neu") for one day after it was marked as finished.
constexpr qint64 RECENT_WINDOW_MS = 24LL * 60 * 60 * 1000;

struct PlanListView {
    QVector<PlanListRow> rows;
    /// Newer Fertigstand of the running plan, if the planner has one.
    std::optional<FinalEntry> newVersion;
};

/**
 * @brief Build the visible list
 * @param finals All locally known Fertigstände (PlannerStore::finals())
 * @param activeFinalId Running Fertigstand, empty when the running plan is not from the planner
 * @param now Current time for PlanListRow::isRecent; invalid = nothing is flagged as recent
 */
[[nodiscard]] PlanListView buildPlanList(const QVector<StoredFinal>& finals, const QString& activeFinalId,
                                         const QDateTime& now = QDateTime());

/**
 * @enum SwitchPermission
 */
enum class SwitchPermission {
    Allowed,
    ProgramRunning,  ///< Robot program is playing or paused
    RobotUnknown     ///< No current, connected dashboard status
};

/**
 * @brief May the HMI switch the running plan now (Planvorschlag takeover)?
 *
 * Only with a recent dashboard status that says the robot is connected and
 * its program is stopped. A paused program is still in progress, and an
 * unknown state is not proof that nothing runs, so both block.
 *
 * @param connected Dashboard connection state
 * @param programState Last program state
 * @param lastUpdate Time of that status
 * @param now Current time
 * @param maxAgeMs Status older than this counts as unknown
 */
[[nodiscard]] SwitchPermission canSwitchPlan(bool connected,
                                             robot::ProgramState programState,
                                             const QDateTime& lastUpdate,
                                             const QDateTime& now,
                                             qint64 maxAgeMs = 10'000);

} // namespace planner
} // namespace multipack

#endif // MULTIPACK_PLANNER_PLANNERPLANLIST_H
