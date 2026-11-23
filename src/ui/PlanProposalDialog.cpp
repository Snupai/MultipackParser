/**
 * @file PlanProposalDialog.cpp
 * @brief Operator confirmation for a Planvorschlag from the planner
 */
#include "multipack/ui/PlanProposalDialog.h"

#include "multipack/database/DatabaseManager.h"
#include "multipack/ui/VisualizationWidget.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace multipack {
namespace ui {

PlanProposalDialog::PlanProposalDialog(const planner::Proposal& proposal,
                                       const database::PaletteData& preview,
                                       QWidget* parent)
    : QDialog(parent)
    , m_proposal(proposal)
{
    setObjectName(QStringLiteral("PlanProposalDialog"));
    setWindowTitle(QStringLiteral("Planvorschlag"));
    setModal(true);
    setFixedSize(1000, 600);
    // Touch panel: readable from a step away, buttons large enough for gloves.
    setStyleSheet(QStringLiteral(
        "QLabel { font-size: 16px; }"
        "QPushButton { font-size: 18px; }"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(10);

    auto* title = new QLabel(
        QStringLiteral("Neuer Plan vorgeschlagen: %1 - Übernehmen?").arg(proposal.name), this);
    title->setTextFormat(Qt::PlainText);
    title->setObjectName(QStringLiteral("proposalTitle"));
    title->setWordWrap(true);
    title->setStyleSheet(QStringLiteral("font-size: 22px; font-weight: bold;"));
    root->addWidget(title);

    auto* body = new QHBoxLayout();
    body->setSpacing(16);

    auto* details = new QFormLayout();
    details->setLabelAlignment(Qt::AlignRight);
    auto addRow = [this, details](const QString& label, const QString& value) {
        auto* valueLabel = new QLabel(value, this);
        valueLabel->setWordWrap(true);
        details->addRow(label, valueLabel);
    };
    addRow(QStringLiteral("Vorgeschlagen von:"),
           proposal.proposedBy.isEmpty() ? QStringLiteral("-") : proposal.proposedBy);
    addRow(QStringLiteral("Zeitpunkt:"), proposal.createdAt.isValid()
               ? proposal.createdAt.toLocalTime().toString(QStringLiteral("dd.MM.yyyy HH:mm"))
               : QStringLiteral("-"));
    addRow(QStringLiteral("Palette:"), QStringLiteral("%1 x %2 x %3 mm")
               .arg(preview.paletteDimensions.length)
               .arg(preview.paletteDimensions.width)
               .arg(preview.paletteDimensions.height));
    addRow(QStringLiteral("Paket:"), QStringLiteral("%1 x %2 x %3 mm")
               .arg(preview.packageDimensions.length)
               .arg(preview.packageDimensions.width)
               .arg(preview.packageDimensions.height));
    addRow(QStringLiteral("Lagen:"), QString::number(preview.metadata.anzLagen));
    // Same count as the 3D preview: the .rob header field (anzahlPakete) is
    // only the pick count of the first layer type.
    const VisualPalette visual = visualPaletteFromPaletteData(preview, proposal.name);
    addRow(QStringLiteral("Pakete:"), QString::number(visual.totalPackages));
    addRow(QStringLiteral("Prüfsumme:"), proposal.sha256.left(12) + QStringLiteral("..."));

    auto* detailsBox = new QWidget(this);
    detailsBox->setLayout(details);
    detailsBox->setFixedWidth(360);
    body->addWidget(detailsBox, 0, Qt::AlignTop);

    m_preview = new VisualizationWidget(this);
    m_preview->setMinimumSize(560, 380);
    m_preview->setVisualizationData(visual);
    body->addWidget(m_preview, 1);
    root->addLayout(body, 1);

    m_robotLabel = new QLabel(this);
    m_robotLabel->setObjectName(QStringLiteral("proposalRobotState"));
    m_robotLabel->setWordWrap(true);
    m_robotLabel->setStyleSheet(QStringLiteral("color: #b00020; font-size: 16px;"));
    root->addWidget(m_robotLabel);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("proposalStatus"));
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet(QStringLiteral("font-size: 16px;"));
    root->addWidget(m_statusLabel);

    auto* buttons = new QHBoxLayout();
    m_laterButton = new QPushButton(QStringLiteral("Später"), this);
    m_laterButton->setObjectName(QStringLiteral("proposalLater"));
    m_laterButton->setMinimumSize(160, 60);
    m_laterButton->setAutoDefault(false);
    buttons->addWidget(m_laterButton);
    buttons->addStretch();
    m_rejectButton = new QPushButton(QStringLiteral("Ablehnen"), this);
    m_rejectButton->setObjectName(QStringLiteral("proposalReject"));
    m_rejectButton->setMinimumSize(200, 60);
    m_acceptButton = new QPushButton(QStringLiteral("Übernehmen"), this);
    m_acceptButton->setObjectName(QStringLiteral("proposalAccept"));
    m_acceptButton->setMinimumSize(200, 60);
    m_acceptButton->setAutoDefault(false);
    m_rejectButton->setAutoDefault(false);
    buttons->addWidget(m_rejectButton);
    buttons->addWidget(m_acceptButton);
    root->addLayout(buttons);

    connect(m_acceptButton, &QPushButton::clicked, this, [this]() {
        m_lastDecisionAccepted = true;
        emit acceptRequested();
    });
    connect(m_laterButton, &QPushButton::clicked, this, &PlanProposalDialog::reject);
    connect(m_rejectButton, &QPushButton::clicked, this, [this]() {
        m_lastDecisionAccepted = false;
        emit rejectRequested();
    });

    updateButtons();
}

void PlanProposalDialog::setTakeoverAllowed(bool allowed, const QString& reason)
{
    m_takeoverAllowed = allowed;
    m_robotLabel->setText(allowed ? QString() : reason);
    updateButtons();
}

void PlanProposalDialog::setBusy(bool busy, const QString& message)
{
    m_busy = busy;
    if (!message.isEmpty() || busy) {
        m_statusLabel->setText(message);
    }
    updateButtons();
}

void PlanProposalDialog::showError(const QString& message)
{
    m_busy = false;
    m_statusLabel->setText(message);
    updateButtons();
}

void PlanProposalDialog::reject()
{
    // "Spaeter", Escape or closing is not a decision: the proposal stays open
    // on the planner and can be reopened from the main menu. Not while the
    // answer is being reported, so its outcome is always shown.
    if (m_busy) {
        return;
    }
    emit deferRequested();
}

void PlanProposalDialog::updateButtons()
{
    m_acceptButton->setEnabled(!m_busy && m_takeoverAllowed);
    m_rejectButton->setEnabled(!m_busy);
    m_laterButton->setEnabled(!m_busy);
}

} // namespace ui
} // namespace multipack
