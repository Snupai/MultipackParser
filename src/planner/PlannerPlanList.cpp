/**
 * @file PlannerPlanList.cpp
 * @brief What the HMI shows of the planner's Fertigstände (pure logic)
 */
#include "multipack/planner/PlannerPlanList.h"

namespace multipack {
namespace planner {

PlanListView buildPlanList(const QVector<StoredFinal>& finals, const QString& activeFinalId)
{
    PlanListView view;

    const StoredFinal* active = nullptr;
    for (const StoredFinal& final : finals) {
        if (!activeFinalId.isEmpty() && final.entry.id == activeFinalId) {
            active = &final;
            break;
        }
    }

    // Newest listed Fertigstand of the running plan with a different id.
    if (active) {
        for (const StoredFinal& final : finals) {
            if (!final.inList || final.entry.id == active->entry.id
                || final.entry.recordId != active->entry.recordId) {
                continue;
            }
            if (!view.newVersion || final.entry.markedAt > view.newVersion->markedAt) {
                view.newVersion = final.entry;
            }
        }
    }

    for (const StoredFinal& final : finals) {
        const bool isActive = active && final.entry.id == active->entry.id;
        if (!final.inList && !isActive) {
            continue;  // withdrawn and not running: hidden
        }
        PlanListRow row;
        row.final = final;
        row.isActive = isActive;
        row.isWithdrawn = !final.inList;
        row.isNewVersionOfActive = view.newVersion && final.entry.id == view.newVersion->id;
        view.rows.append(row);
    }

    return view;
}

SwitchPermission canSwitchPlan(bool connected,
                               robot::ProgramState programState,
                               const QDateTime& lastUpdate,
                               const QDateTime& now,
                               qint64 maxAgeMs)
{
    if (!connected || !lastUpdate.isValid() || lastUpdate.msecsTo(now) > maxAgeMs
        || lastUpdate.msecsTo(now) < -maxAgeMs) {
        return SwitchPermission::RobotUnknown;
    }
    switch (programState) {
        case robot::ProgramState::Stopped:
            return SwitchPermission::Allowed;
        case robot::ProgramState::Playing:
        case robot::ProgramState::Paused:
            return SwitchPermission::ProgramRunning;
        case robot::ProgramState::Unknown:
        default:
            return SwitchPermission::RobotUnknown;
    }
}

} // namespace planner
} // namespace multipack
