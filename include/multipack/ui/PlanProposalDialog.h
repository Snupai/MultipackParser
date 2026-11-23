/**
 * @file PlanProposalDialog.h
 * @brief Operator confirmation for a Planvorschlag from the planner
 *
 * Shows "Neuer Plan vorgeschlagen: {name} - Uebernehmen?" with a preview.
 * The dialog only collects the decision; MainWindow reports it to the
 * planner and switches the plan only after the planner confirmed an
 * acceptance and while the robot is not executing a program.
 */
#ifndef MULTIPACK_UI_PLANPROPOSALDIALOG_H
#define MULTIPACK_UI_PLANPROPOSALDIALOG_H

#include "multipack/planner/PlannerProtocol.h"

#include <QDialog>

QT_BEGIN_NAMESPACE
class QLabel;
class QPushButton;
QT_END_NAMESPACE

namespace multipack {
namespace database { struct PaletteData; }

namespace ui {

class VisualizationWidget;

class PlanProposalDialog : public QDialog
{
    Q_OBJECT

public:
    PlanProposalDialog(const planner::Proposal& proposal,
                       const database::PaletteData& preview,
                       QWidget* parent = nullptr);

    [[nodiscard]] const planner::Proposal& proposal() const { return m_proposal; }

    /// Enable/disable "Uebernehmen" with a reason shown to the operator.
    void setTakeoverAllowed(bool allowed, const QString& reason);
    /// While the answer is being reported: buttons disabled, message shown.
    void setBusy(bool busy, const QString& message = QString());
    [[nodiscard]] bool isBusy() const { return m_busy; }
    /// Show a message (e.g. connection error) and re-enable the buttons.
    void showError(const QString& message);

    /// Last decision the operator took (valid after acceptRequested/rejectRequested).
    [[nodiscard]] bool lastDecisionAccepted() const { return m_lastDecisionAccepted; }

signals:
    void acceptRequested();
    void rejectRequested();
    /// Operator postponed the decision; the proposal stays open.
    void deferRequested();

protected:
    void reject() override;

private:
    void updateButtons();

    planner::Proposal m_proposal;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_robotLabel = nullptr;
    QPushButton* m_acceptButton = nullptr;
    QPushButton* m_rejectButton = nullptr;
    QPushButton* m_laterButton = nullptr;
    VisualizationWidget* m_preview = nullptr;
    bool m_takeoverAllowed = false;
    bool m_busy = false;
    bool m_lastDecisionAccepted = false;
};

} // namespace ui
} // namespace multipack

#endif // MULTIPACK_UI_PLANPROPOSALDIALOG_H
