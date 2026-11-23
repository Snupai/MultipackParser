/**
 * @file PlannerPlanList.cpp
 * @brief What the HMI shows of the planner's Fertigstände (pure logic)
 */
#include "multipack/planner/PlannerPlanList.h"

#include <algorithm>

namespace multipack {
namespace planner {

PlanListView buildPlanList(const QVector<StoredFinal>& finals, const QString& activeFinalId,
                           const QDateTime& now)
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

    QVector<PlanListRow> visible;
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
        const qint64 ageMs = now.isValid() && final.entry.markedAt.isValid()
            ? final.entry.markedAt.msecsTo(now) : -1;
        row.isRecent = ageMs >= 0 && ageMs <= RECENT_WINDOW_MS;
        visible.append(row);
    }

    // Versions of one plan stay together: each plan sits where its first
    // Fertigstand is in the incoming order, its versions newest first.
    QVector<bool> placed(visible.size(), false);
    for (int i = 0; i < visible.size(); ++i) {
        if (placed[i]) {
            continue;
        }
        QVector<PlanListRow> versions;
        for (int j = i; j < visible.size(); ++j) {
            if (!placed[j] && visible[j].final.entry.recordId == visible[i].final.entry.recordId) {
                versions.append(visible[j]);
                placed[j] = true;
            }
        }
        std::stable_sort(versions.begin(), versions.end(), [](const PlanListRow& a, const PlanListRow& b) {
            return a.final.entry.markedAt > b.final.entry.markedAt;
        });
        const int count = static_cast<int>(versions.size());
        for (int k = 0; k < count; ++k) {
            versions[k].versionCount = count;
            versions[k].versionNumber = count - k;
            view.rows.append(versions[k]);
        }
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
