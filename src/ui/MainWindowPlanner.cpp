/**
 * @file MainWindowPlanner.cpp
 * @brief MainWindow: Palettenplaner integration (HMI-Abgleich)
 *
 * - The plan list on "Pläne": planner Fertigstände (shown as released plans)
 *   and USB plans in one list with the 3D preview; a badge on the tab and a
 *   one-line notice on "Betrieb" point to it
 * - "QR-Code" tab with the planner page of this HMI and an online/offline hint
 * - Planvorschlag confirmation (never switches without the operator, never
 *   while the robot executes a program)
 * - "Planer" settings tab with the USB import of the configuration file
 */
#include "multipack/ui/MainWindow.h"
#include "ui_MainWindow.h"

#include "multipack/config/ConfigDefaults.h"
#include "multipack/config/SettingsManager.h"
#include "multipack/core/GlobalState.h"
#include "multipack/database/DatabaseManager.h"
#include "multipack/planner/PlannerConfig.h"
#include "multipack/planner/PlannerPlanList.h"
#include "multipack/planner/PlannerStore.h"
#include "multipack/planner/PlannerSyncService.h"
#include "multipack/system/UsbMonitor.h"
#include "multipack/ui/FinalsListDelegate.h"
#include "multipack/ui/PlanProposalDialog.h"
#include "multipack/ui/QrCodeWidget.h"
#include "multipack/ui/Theme.h"
#include "multipack/ui/VisualizationWidget.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScroller>
#include <QSet>
#include <QSignalBlocker>
#include <QStyle>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector>

#include <algorithm>

namespace multipack {
namespace ui {

namespace {

constexpr int ROLE_FINAL_ID = FinalsRole::FinalId;
constexpr int ROLE_DOWNLOADED = FinalsRole::Downloaded;

QString loadErrorText(const QString& loadError)
{
    if (loadError == QLatin1String(planner::LOAD_ERROR_INVALID_ROB)) {
        return QStringLiteral("Plan ungültig, nicht ladbar");
    }
    if (loadError == QLatin1String(planner::LOAD_ERROR_CHECKSUM)) {
        return QStringLiteral("Prüfsumme falsch, neuer Versuch folgt");
    }
    return QStringLiteral("nicht ladbar");
}

QString formatMarkedAt(const QDateTime& markedAt)
{
    return markedAt.isValid() ? markedAt.toLocalTime().toString(QStringLiteral("dd.MM.yy HH:mm")) : QString();
}

QString resolveAgainstWorkingDir(const QString& path)
{
    const QFileInfo info(path);
    return info.isAbsolute() ? info.absoluteFilePath() : QDir::current().absoluteFilePath(path);
}

} // namespace

QString MainWindow::paletteDisplayName(const QString& fileName) const
{
    const QString finalId = planner::finalIdFromPaletteFileName(fileName);
    if (!finalId.isEmpty()) {
        if (m_plannerSync) {
            const auto stored = m_plannerSync->store().find(finalId);
            if (stored) {
                return stored->entry.name + QStringLiteral(" (Planer)");
            }
        }
        return QStringLiteral("Planer-Plan");
    }

    QString displayName = QFileInfo(fileName).fileName().trimmed();
    if (displayName.endsWith(QStringLiteral(".rob"), Qt::CaseInsensitive)) {
        displayName.chop(4);
    }
    return displayName;
}

void MainWindow::setPlannerSync(planner::PlannerSyncService* sync)
{
    if (m_plannerSync == sync) {
        return;
    }
    if (m_plannerSync) {
        disconnect(m_plannerSync, nullptr, this, nullptr);
    }
    m_plannerSync = sync;

    if (m_plannerSync) {
        connect(m_plannerSync, &planner::PlannerSyncService::stateChanged,
                this, [this]() { updatePlannerConnectionState(); });
        connect(m_plannerSync, &planner::PlannerSyncService::cycleFinished,
                this, [this]() { updatePlannerConnectionState(); });
        connect(m_plannerSync, &planner::PlannerSyncService::finalsChanged,
                this, &MainWindow::refreshPlannerList);
        connect(m_plannerSync, &planner::PlannerSyncService::proposalAvailable,
                this, &MainWindow::onProposalAvailable);
        connect(m_plannerSync, &planner::PlannerSyncService::proposalCleared,
                this, &MainWindow::onProposalCleared);
        connect(m_plannerSync, &planner::PlannerSyncService::proposalAnswered,
                this, &MainWindow::onProposalAnswered);
        connect(m_plannerSync, &planner::PlannerSyncService::proposalUnloadable,
                this, &MainWindow::showUnloadableProposalNotices);
        if (const auto proposal = m_plannerSync->currentProposal()) {
            onProposalAvailable(*proposal);
        }
        // The first cycle can run during start-up, before this window exists.
        showUnloadableProposalNotices();
    }

    updatePlannerSettingsTab();
    updatePlannerConnectionState();
    refreshPlannerList();
}

// === Plan list ("Pläne") ===

void MainWindow::setupPlannerPanel()
{
    m_activePlanDebounce = new QTimer(this);
    m_activePlanDebounce->setSingleShot(true);
    m_activePlanDebounce->setInterval(500);
    connect(m_activePlanDebounce, &QTimer::timeout, this, &MainWindow::onActivePlanFileChanged);

    // The plan list and the QR page are laid out in MainWindow.ui.
    m_plannerList = ui->plannerFinalsList;
    m_plannerLoadButton = ui->plannerLoadButton;
    m_plannerProposalButton = ui->plannerProposalButton;
    m_plannerStateLabel = ui->plannerStateLabel;
    m_plannerHintLabel = ui->plannerHintLabel;
    m_plannerQr = ui->plannerQrCode;
    m_plannerQrStateLabel = ui->plannerQrStateLabel;
    m_plannerQrUrlLabel = ui->plannerQrUrlLabel;

    // One card per plan (FinalsListDelegate), dragged like a phone list.
    m_plannerList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_plannerList->setItemDelegate(new FinalsListDelegate(m_plannerList));
    m_plannerList->setResizeMode(QListView::Adjust);
    m_plannerList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_plannerList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_plannerList->setFocusPolicy(Qt::NoFocus);
    // Drag scrolling. Qt 6.2 replays the tap of a scroller gesture at unscaled
    // coordinates: on a scaled panel (DisplayScale) it would select the wrong
    // row. The list keeps its wide scroll bar there. Works in Qt 6.8.
    const bool scaledPanel = !qFuzzyCompare(m_plannerList->devicePixelRatioF(), qreal(1));
    if (!scaledPanel || QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)) {
        QScroller::grabGesture(m_plannerList->viewport(), QScroller::LeftMouseButtonGesture);
    }

    m_plannerStateLabel->hide();
    m_plannerHintLabel->hide();
    m_plannerLoadButton->setEnabled(false);
    m_plannerProposalButton->hide();
    ui->plannerNewVersionNotice->hide();

    // Count of planner plans that need attention, on the tab itself.
    m_plannerBadge = new QLabel(ui->navPlaene);
    m_plannerBadge->setObjectName(QStringLiteral("navPlaeneBadge"));
    m_plannerBadge->setAlignment(Qt::AlignCenter);
    m_plannerBadge->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto* badgeLayout = new QHBoxLayout(ui->navPlaene);
    badgeLayout->setContentsMargins(0, 9, 7, 0);
    badgeLayout->addStretch();
    badgeLayout->addWidget(m_plannerBadge, 0, Qt::AlignTop);
    m_plannerBadge->hide();

    connect(m_plannerList, &QListWidget::itemSelectionChanged, this, &MainWindow::updatePlannerSelection);
    connect(m_plannerList, &QListWidget::itemDoubleClicked, this, [this]() { onPlannerLoadClicked(); });
    connect(m_plannerLoadButton, &QPushButton::clicked, this, &MainWindow::onPlannerLoadClicked);
    connect(m_plannerProposalButton, &QPushButton::clicked, this, [this]() {
        m_deferredProposalId.clear();
        openProposalDialog();
    });
    // The notice on "Betrieb" leads to the new version, ready to be loaded.
    connect(ui->plannerNewVersionNotice, &QPushButton::clicked, this, [this]() {
        showPlans();
        for (int row = 0; row < m_plannerList->count(); ++row) {
            QListWidgetItem* item = m_plannerList->item(row);
            if (!item->isHidden() && item->data(ROLE_FINAL_ID).toString() == m_newVersionFinalId) {
                m_plannerList->setCurrentItem(item);
                m_plannerList->scrollToItem(item);
                break;
            }
        }
    });

    refreshPlannerList();
}

void MainWindow::updatePlannerAttention(bool newVersion, int failedCount)
{
    // Betrieb: one tappable line while the running plan has a newer version.
    ui->plannerNewVersionNotice->setVisible(newVersion);

    // Tab: how many planner plans want a look (new version, load errors).
    const int count = failedCount + (newVersion ? 1 : 0);
    m_plannerBadge->setText(QString::number(count));
    m_plannerBadge->setProperty("tone", failedCount > 0 ? QStringLiteral("danger") : QStringLiteral("warning"));
    m_plannerBadge->style()->unpolish(m_plannerBadge);
    m_plannerBadge->style()->polish(m_plannerBadge);
    m_plannerBadge->setVisible(count > 0);
}

void MainWindow::updatePlannerSelection()
{
    const auto items = m_plannerList->selectedItems();
    const QListWidgetItem* item = items.isEmpty() ? nullptr : items.first();
    const bool loadable = item && item->data(ROLE_DOWNLOADED).toBool();
    m_plannerLoadButton->setEnabled(loadable);

    // The bar below the preview says what "Plan laden" will load, or why it cannot.
    QString text;
    Theme::Tone tone = Theme::Tone::Neutral;
    if (!item) {
        text = QStringLiteral("Plan antippen, dann \"Plan laden\".");
    } else if (loadable) {
        // Name and date: two versions of a planner plan share the name.
        const QString markedAt = item->data(FinalsRole::MarkedAt).toString();
        text = QStringLiteral("Ausgewählt: %1").arg(item->data(FinalsRole::Name).toString());
        if (!markedAt.isEmpty()) {
            text += QStringLiteral(", Stand %1").arg(markedAt);
        }
    } else if (!item->data(FinalsRole::LoadError).toString().isEmpty()) {
        text = QStringLiteral("Nicht ladbar: %1.").arg(item->data(FinalsRole::LoadError).toString());
        tone = Theme::Tone::Danger;
    } else {
        text = QStringLiteral("Wird noch vom Planer geladen.");
    }
    ui->plannerSelectionLabel->setText(text);
    ui->plannerSelectionLabel->setStyleSheet(
        QStringLiteral("color: %1;").arg(item ? Theme::toneColor(tone) : QString::fromLatin1(Theme::TEXT_MUTED)));

    // 3D preview of a newly selected plan; one that is not stored has none.
    // The key holds the availability: a plan gets its preview once it is stored.
    const QString fileName = item ? item->data(FinalsRole::FileName).toString() : QString();
    const QString key = item ? fileName + (loadable ? QStringLiteral("|stored") : QStringLiteral("|missing"))
                             : QString();
    if (key == m_selectedPlanKey) {
        return;
    }
    m_selectedPlanKey = key;
    if (!item) {
        return;
    }
    const auto data = loadable && m_database ? m_database->loadPaletteData(fileName) : std::nullopt;
    if (data.has_value()) {
        updateVisualizationFromPaletteData(*data);
    } else if (m_visualizationWidget) {
        m_visualizationWidget->setVisualizationData(VisualPalette());
    }
}

void MainWindow::refreshPlannerList()
{
    if (!m_plannerList) {
        return;
    }

    // One row per plan, whatever its source; newest first.
    struct Entry {
        qint64 timestamp = 0;
        QString summary;
        QString fileName;
        QString name;
        QString finalId;
        QString markedAt;
        QString loadError;
        QString sourceTag;
        QString toolTip;
        QStringList chips;
        bool downloaded = true;
    };
    QVector<Entry> entries;

    const QString activeFile = m_state ? m_state->currentFileName() : m_currentPaletteFile;
    const bool plannerConfigured = m_plannerSync && m_plannerSync->isConfigured();
    // Package size filter. The database answers it for the local plans (names
    // without ".rob"), asked on every rebuild because the plans change; a planner
    // plan is compared by its stored size and has none until it is stored.
    const bool filterActive = m_planFilterActive && m_database;
    QSet<QString> localMatches;
    if (filterActive) {
        const QStringList matching = m_database->findByPackageDimensions(
            m_planFilterLength, m_planFilterWidth, m_planFilterHeight);
        localMatches = QSet<QString>(matching.begin(), matching.end());
    }
    const auto passesFilter = [this, filterActive, &localMatches](const Entry& entry) {
        if (!filterActive) {
            return true;
        }
        if (entry.finalId.isEmpty()) {
            QString name = entry.fileName;
            name.replace(QStringLiteral(".rob"), QString());
            return localMatches.contains(name);
        }
        const auto data = entry.downloaded ? m_database->loadPaletteData(entry.fileName) : std::nullopt;
        if (!data.has_value()) {
            return false;
        }
        const auto& size = data->packageDimensions;
        return (m_planFilterLength == 0 || size.length == m_planFilterLength)
            && (m_planFilterWidth == 0 || size.width == m_planFilterWidth)
            && (m_planFilterHeight == 0 || size.height == m_planFilterHeight);
    };

    // Plans synced from the Palettenplaner (Fertigstände).
    int failedCount = 0;
    QString hint;
    m_newVersionFinalId.clear();
    if (plannerConfigured) {
        const planner::PlanListView view = planner::buildPlanList(
            m_plannerSync->store().finals(), currentActiveFinalId(), QDateTime::currentDateTimeUtc());
        if (view.newVersion) {
            m_newVersionFinalId = view.newVersion->id;
            hint = QStringLiteral("Neue Version verfügbar: %1").arg(view.newVersion->name);
        }

        QString groupRecord;
        qint64 groupTimestamp = 0;
        for (const planner::PlanListRow& row : view.rows) {
            const bool failed = !row.final.downloaded && !row.final.loadError.isEmpty();
            failedCount += failed ? 1 : 0;

            Entry entry;
            entry.fileName = planner::plannerPaletteFileName(row.final.entry.id);
            entry.finalId = row.final.entry.id;
            entry.name = row.final.entry.name;
            entry.markedAt = formatMarkedAt(row.final.entry.markedAt);
            entry.downloaded = row.final.downloaded;
            entry.sourceTag = QStringLiteral("Planer");
            entry.toolTip = row.final.entry.fileName;
            // Versions of a plan stay next to each other, at the place of the newest.
            if (row.final.entry.recordId != groupRecord) {
                groupRecord = row.final.entry.recordId;
                groupTimestamp = row.final.entry.markedAt.isValid() ? row.final.entry.markedAt.toMSecsSinceEpoch() : 0;
            }
            entry.timestamp = groupTimestamp;

            // Plain summary (item text) and the state chips of the card.
            entry.summary = entry.name;
            if (!entry.markedAt.isEmpty()) {
                entry.summary += QStringLiteral("   ") + entry.markedAt;
            }
            if (row.isActive) {
                entry.summary += QStringLiteral("   [läuft]");
                entry.chips.append(FinalsChip::encode(FinalsChip::Success, QStringLiteral("läuft")));
            }
            if (row.versionNumber < row.versionCount && !row.isActive) {
                entry.chips.append(FinalsChip::encode(FinalsChip::Neutral, QStringLiteral("ältere Version")));
            }
            if (row.isWithdrawn) {
                entry.summary += QStringLiteral("   [nicht mehr freigegeben]");
                entry.chips.append(FinalsChip::encode(FinalsChip::Neutral, QStringLiteral("nicht mehr freigegeben")));
            }
            if (row.isNewVersionOfActive) {
                entry.summary += QStringLiteral("   [Neue Version]");
                entry.chips.append(FinalsChip::encode(FinalsChip::Warning, QStringLiteral("Neue Version")));
            }
            if (failed) {
                entry.loadError = loadErrorText(row.final.loadError);
                entry.summary += QStringLiteral("   [Fehler: %1]").arg(entry.loadError);
                entry.chips.append(FinalsChip::encode(FinalsChip::Danger, QStringLiteral("Fehler")));
                entry.toolTip = QStringLiteral("Dieser Plan kann auf dem HMI nicht geladen werden. "
                                               "Bitte im Palettenplaner prüfen.");
            } else if (!row.final.downloaded) {
                entry.summary += QStringLiteral("   (wird geladen ...)");
                entry.chips.append(FinalsChip::encode(FinalsChip::Neutral, QStringLiteral("wird geladen")));
            } else if (row.isRecent && !row.isActive && !row.isNewVersionOfActive) {
                entry.chips.append(FinalsChip::encode(FinalsChip::Info, QStringLiteral("Neu")));
            }
            entries.append(entry);
        }
    }

    // Local plans (USB).
    if (m_database) {
        for (const auto& file : m_database->listAvailableFiles()) {
            Entry entry;
            entry.timestamp = file.timestamp;
            entry.fileName = file.fileName;
            entry.name = paletteDisplayName(file.fileName);
            entry.summary = entry.name;
            if (file.fileName == activeFile) {
                entry.summary += QStringLiteral("   [läuft]");
                entry.chips.append(FinalsChip::encode(FinalsChip::Success, QStringLiteral("läuft")));
            }
            entries.append(entry);
        }
    }

    std::stable_sort(entries.begin(), entries.end(),
                     [](const Entry& a, const Entry& b) { return a.timestamp > b.timestamp; });

    const QString selectedFile = m_plannerList->currentItem()
        ? m_plannerList->currentItem()->data(FinalsRole::FileName).toString()
        : QString();
    bool anyShown = false;
    {
        // One selection update at the end instead of one per removed item.
        const QSignalBlocker blocker(m_plannerList);
        m_plannerList->clear();
        for (const Entry& entry : entries) {
            auto* item = new QListWidgetItem(entry.summary, m_plannerList);
            item->setData(ROLE_FINAL_ID, entry.finalId);
            item->setData(ROLE_DOWNLOADED, entry.downloaded);
            item->setData(FinalsRole::FileName, entry.fileName);
            item->setData(FinalsRole::Name, entry.name);
            item->setData(FinalsRole::Chips, entry.chips);
            item->setData(FinalsRole::SourceTag, entry.sourceTag);
            item->setData(FinalsRole::MarkedAt, entry.markedAt);
            item->setData(FinalsRole::LoadError, entry.loadError);
            item->setToolTip(entry.toolTip);
            // The package size filter hides rows; a plan that is not stored has no size.
            if (!passesFilter(entry)) {
                item->setHidden(true);
                continue;
            }
            anyShown = true;
            if (entry.fileName == selectedFile) {
                m_plannerList->setCurrentItem(item);
            }
        }
    }
    m_plannerRowsShown = plannerConfigured;

    m_plannerList->setVisible(anyShown);
    ui->planListEmptyLabel->setVisible(!anyShown);
    ui->planListEmptyLabel->setText(filterActive
        ? QStringLiteral("Kein Palettierplan passt zum Filter.")
        : QStringLiteral("Keine Palettierpläne vorhanden."));
    m_plannerHintLabel->setText(hint);
    m_plannerHintLabel->setVisible(!hint.isEmpty());
    updatePlannerAttention(!m_newVersionFinalId.isEmpty(), failedCount);
    updatePlannerSelection();
}

void MainWindow::updatePlannerConnectionState()
{
    const bool configured = m_plannerSync && m_plannerSync->isConfigured();
    // The QR code has its own tab; without a planner there is nothing to scan.
    ui->navQrCode->setVisible(configured);
    if (!configured && ui->stackedWidget->currentIndex() == PAGE_QR_CODE) {
        showMainMenu();
    }
    // Planner plans come and go with the configuration.
    m_plannerStateLabel->setVisible(configured);
    if (configured != m_plannerRowsShown) {
        refreshPlannerList();
    }
    updatePlannerSettingsTab();
    if (!configured) {
        if (m_plannerQr) {
            m_plannerQr->setText(QString());
        }
        return;
    }

    using State = planner::PlannerSyncService::ConnectionState;
    const State state = m_plannerSync->state();
    Theme::Tone tone = Theme::Tone::Neutral;
    QString qrText;
    switch (state) {
        case State::Online:
            tone = Theme::Tone::Success;
            qrText = QStringLiteral("Online");
            break;
        case State::Offline:
            tone = Theme::Tone::Warning;
            qrText = QStringLiteral("Offline");
            break;
        case State::NotAuthorized:
            tone = Theme::Tone::Danger;
            qrText = QStringLiteral("Offline - HMI nicht freigeschaltet");
            break;
        case State::Connecting:
        case State::NotConfigured:
            qrText = QStringLiteral("Verbinde ...");
            break;
    }
    const QString color = tone == Theme::Tone::Neutral ? QString::fromLatin1(Theme::TEXT_MUTED)
                                                       : Theme::toneColor(tone);

    QString stateText = QStringLiteral("Planer: ") + m_plannerSync->stateText();
    const QDateTime lastSuccess = m_plannerSync->lastSuccess();
    if (state != State::Online && lastSuccess.isValid()) {
        stateText += QStringLiteral(" (zuletzt online %1)")
                         .arg(lastSuccess.toLocalTime().toString(QStringLiteral("dd.MM. HH:mm")));
    }
    m_plannerStateLabel->setText(stateText);
    m_plannerStateLabel->setStyleSheet(QStringLiteral("color: %1; font-weight: bold;").arg(color));

    // Only the planner page URL of this HMI, never a token.
    const QString pageUrl = m_plannerSync->config().hmiPageUrl();
    m_plannerQr->setText(pageUrl);
    m_plannerQrUrlLabel->setText(pageUrl);
    m_plannerQrStateLabel->setText(QStringLiteral("Planer: %1").arg(qrText));
    m_plannerQrStateLabel->setStyleSheet(QStringLiteral("color: %1;").arg(color));
}

void MainWindow::onPlannerLoadClicked()
{
    if (!m_plannerList || !m_database) {
        return;
    }
    // The status line is on "Betrieb": a refusal is also shown next to the
    // button that was pressed.
    const auto refuse = [this](const QString& reason) {
        showMessage(reason);
        ui->plannerSelectionLabel->setText(reason);
        ui->plannerSelectionLabel->setStyleSheet(
            QStringLiteral("color: %1;").arg(Theme::toneColor(Theme::Tone::Danger)));
    };
    const auto items = m_plannerList->selectedItems();
    if (items.isEmpty()) {
        refuse(QStringLiteral("Bitte Plan auswählen"));
        return;
    }
    const QString finalId = items.first()->data(ROLE_FINAL_ID).toString();
    if (finalId.isEmpty()) {
        // USB plan: loaded by name, as from the plan input on "Betrieb".
        ui->EingabePallettenplan->setText(items.first()->data(FinalsRole::Name).toString());
        showMainMenu();
        onLoadPaletteClicked();
        return;
    }
    // Never switch to a planner plan while a program runs. An unknown robot
    // state does not block a manual load (robot off, operator at the machine),
    // unlike a Planvorschlag.
    const planner::SwitchPermission permission = currentSwitchPermission();
    if (permission == planner::SwitchPermission::ProgramRunning) {
        refuse(switchPermissionText(permission));
        return;
    }
    const auto data = m_database->loadPaletteData(planner::plannerPaletteFileName(finalId));
    if (!data.has_value()) {
        refuse(QStringLiteral("Plan wird noch vom Planer geladen"));
        return;
    }
    applyLoadedPalette(*data, paletteDisplayName(data->metadata.fileName));
    // Back to the operating page with the plan loaded.
    showMainMenu();
}

// === Active plan / status report ===

QString MainWindow::currentActiveFinalId() const
{
    const QString fileName = m_state ? m_state->currentFileName() : m_currentPaletteFile;
    return planner::finalIdFromPaletteFileName(fileName);
}

void MainWindow::onActivePlanFileChanged()
{
    reportActivePlan(false);
}

void MainWindow::reportActivePlan(bool force)
{
    const QString activeId = currentActiveFinalId();
    if (force || !m_activePlanReported || activeId != m_reportedActiveFinalId) {
        m_reportedActiveFinalId = activeId;
        m_activePlanReported = true;
        if (m_plannerSync) {
            m_plannerSync->setActivePlan(activeId);
        }
    }
    refreshPlannerList();
}

// === Planvorschlag ===

planner::SwitchPermission MainWindow::currentSwitchPermission() const
{
    return planner::canSwitchPlan(m_robotStatusConnected, m_robotProgramState, m_robotStatusTime,
                                  QDateTime::currentDateTimeUtc());
}

QString MainWindow::switchPermissionText(planner::SwitchPermission permission)
{
    switch (permission) {
        case planner::SwitchPermission::Allowed:
            return QString();
        case planner::SwitchPermission::ProgramRunning:
            return QStringLiteral("Roboterprogramm läuft - Übernahme erst nach dem Stoppen möglich.");
        case planner::SwitchPermission::RobotUnknown:
            return QStringLiteral("Roboterstatus unbekannt - Übernahme nicht möglich.");
    }
    return QString();
}

void MainWindow::onProposalAvailable(const planner::Proposal& proposal)
{
    if (m_proposalDialog && m_proposalDialog->proposal().id != proposal.id && !m_proposalDialog->isBusy()) {
        // Superseded by a newer proposal.
        closeProposalDialog();
    }
    m_pendingProposal = proposal;
    updateProposalUi();
}

void MainWindow::onProposalCleared(const QString& proposalId)
{
    if (m_pendingProposal && m_pendingProposal->id == proposalId) {
        m_pendingProposal.reset();
    }
    if (m_proposalDialog && m_proposalDialog->proposal().id == proposalId && !m_proposalDialog->isBusy()) {
        closeProposalDialog();
        showMessage(QStringLiteral("Planvorschlag wurde zurückgenommen oder ist abgelaufen"));
    }
    updateProposalUi();
}

void MainWindow::showUnloadableProposalNotices()
{
    if (!m_plannerSync) {
        return;
    }
    // The sync service answers "rejected" to the planner. Tell the operator
    // why nothing happened; non-blocking so the HMI keeps running.
    for (const auto& notice : m_plannerSync->takeUnloadableNotices()) {
        const planner::Proposal& proposal = notice.proposal;
        const QString& loadError = notice.loadError;
        if (m_pendingProposal && m_pendingProposal->id == proposal.id) {
            m_pendingProposal.reset();
        }
        auto* box = new QMessageBox(
            QMessageBox::Warning, QStringLiteral("Planvorschlag"),
            QStringLiteral("Vorgeschlagener Plan \"%1\" kann auf diesem HMI nicht geladen werden (%2).\n"
                           "Der Vorschlag wird dem Planer als abgelehnt gemeldet. "
                           "Bitte den Plan im Palettenplaner prüfen.")
                .arg(proposal.name, loadErrorText(loadError)),
            QMessageBox::Ok, this);
        box->setObjectName(QStringLiteral("proposalUnloadableNotice"));
        box->setAttribute(Qt::WA_DeleteOnClose);
        box->open();
    }
    refreshPlannerList();
    updateProposalUi();
}

void MainWindow::updateProposalUi()
{
    const planner::SwitchPermission permission = currentSwitchPermission();
    const bool allowed = permission == planner::SwitchPermission::Allowed;

    if (m_proposalDialog) {
        m_proposalDialog->setTakeoverAllowed(allowed, switchPermissionText(permission));
    }

    if (m_plannerProposalButton) {
        if (m_pendingProposal && !m_proposalDialog) {
            const bool running = permission == planner::SwitchPermission::ProgramRunning;
            m_plannerProposalButton->setText(running
                ? QStringLiteral("Planvorschlag wartet (Roboter läuft)")
                : QStringLiteral("Planvorschlag anzeigen"));
            // Shown only while no program runs (contract); a deferred or
            // unknown-state proposal can still be opened and rejected.
            m_plannerProposalButton->setEnabled(!running);
            m_plannerProposalButton->show();
        } else {
            m_plannerProposalButton->hide();
        }
    }

    // Open automatically only when a takeover is possible right now and the
    // operator has not postponed this proposal.
    if (m_pendingProposal && !m_proposalDialog && allowed && m_deferredProposalId != m_pendingProposal->id) {
        openProposalDialog();
    }
}

void MainWindow::openProposalDialog()
{
    if (!m_pendingProposal || m_proposalDialog || !m_database || !m_plannerSync) {
        return;
    }
    if (currentSwitchPermission() == planner::SwitchPermission::ProgramRunning) {
        return;
    }

    const planner::Proposal proposal = *m_pendingProposal;
    const auto data = m_database->loadPaletteData(planner::plannerPaletteFileName(proposal.finalId));
    if (!data.has_value()) {
        qWarning() << "Planvorschlag: Fertigstand nicht lokal vorhanden:" << proposal.finalId;
        return;
    }

    auto* dialog = new PlanProposalDialog(proposal, *data, this);
    m_proposalDialog = dialog;

    connect(dialog, &PlanProposalDialog::acceptRequested, this, [this, dialog]() {
        const planner::SwitchPermission permission = currentSwitchPermission();
        if (permission != planner::SwitchPermission::Allowed) {
            dialog->setTakeoverAllowed(false, switchPermissionText(permission));
            return;
        }
        dialog->setBusy(true, QStringLiteral("Übernahme wird an den Planer gemeldet ..."));
        m_plannerSync->answerProposal(dialog->proposal().id, true);
    });
    connect(dialog, &PlanProposalDialog::rejectRequested, this, [this, dialog]() {
        dialog->setBusy(true, QStringLiteral("Ablehnung wird an den Planer gemeldet ..."));
        m_plannerSync->answerProposal(dialog->proposal().id, false);
    });
    connect(dialog, &PlanProposalDialog::deferRequested, this, [this, dialog]() {
        m_deferredProposalId = dialog->proposal().id;
        closeProposalDialog();
        updateProposalUi();
    });

    const planner::SwitchPermission permission = currentSwitchPermission();
    dialog->setTakeoverAllowed(permission == planner::SwitchPermission::Allowed, switchPermissionText(permission));
    dialog->open();
    updateProposalUi();
}

void MainWindow::closeProposalDialog()
{
    if (!m_proposalDialog) {
        return;
    }
    PlanProposalDialog* dialog = m_proposalDialog;
    m_proposalDialog = nullptr;
    disconnect(dialog, nullptr, this, nullptr);
    dialog->done(QDialog::Accepted);
    dialog->deleteLater();
}

void MainWindow::onProposalAnswered(const QString& proposalId, bool accepted,
                                    planner::PlannerSyncService::AnswerOutcome outcome)
{
    using Outcome = planner::PlannerSyncService::AnswerOutcome;

    const bool dialogForThis = m_proposalDialog && m_proposalDialog->proposal().id == proposalId;
    if (outcome == Outcome::NotDelivered) {
        // Never switch on an unconfirmed acceptance: keep the dialog, let the operator retry.
        if (dialogForThis) {
            m_proposalDialog->showError(QStringLiteral(
                "Keine Verbindung zum Planer - Antwort nicht übermittelt. Bitte erneut versuchen."));
        }
        return;
    }

    std::optional<planner::Proposal> proposal;
    if (dialogForThis) {
        proposal = m_proposalDialog->proposal();
    } else if (m_pendingProposal && m_pendingProposal->id == proposalId) {
        proposal = m_pendingProposal;
    }
    if (m_pendingProposal && m_pendingProposal->id == proposalId) {
        m_pendingProposal.reset();
    }
    if (m_deferredProposalId == proposalId) {
        m_deferredProposalId.clear();
    }
    if (dialogForThis) {
        closeProposalDialog();
    }

    if (outcome == Outcome::Closed) {
        showMessage(QStringLiteral(
            "Planvorschlag ist nicht mehr gültig (zurückgenommen, ersetzt oder abgelaufen) - Plan nicht gewechselt"));
        updateProposalUi();
        return;
    }

    if (!accepted) {
        showMessage(QStringLiteral("Planvorschlag abgelehnt"));
        updateProposalUi();
        return;
    }

    // Confirmed acceptance: switch now, but only while no program runs.
    const planner::SwitchPermission permission = currentSwitchPermission();
    const auto data = proposal && m_database
        ? m_database->loadPaletteData(planner::plannerPaletteFileName(proposal->finalId))
        : std::nullopt;
    if (permission != planner::SwitchPermission::Allowed || !data.has_value()) {
        // Non-blocking: the HMI keeps running while the notice is shown.
        auto* box = new QMessageBox(QMessageBox::Warning, QStringLiteral("Planvorschlag"),
                                    QStringLiteral("Der Vorschlag wurde angenommen, der Plan konnte aber nicht "
                                                   "gewechselt werden.\n%1\nBitte den Plan in der Liste "
                                                   "im Tab \"Pläne\" manuell laden.")
                                        .arg(switchPermissionText(permission)),
                                    QMessageBox::Ok, this);
        box->setObjectName(QStringLiteral("proposalNotSwitchedNotice"));
        box->setAttribute(Qt::WA_DeleteOnClose);
        box->open();
        updateProposalUi();
        return;
    }

    applyLoadedPalette(*data, paletteDisplayName(data->metadata.fileName));
    showMessage(QStringLiteral("Plan übernommen: %1").arg(proposal->name));
    updateProposalUi();
}

// === Settings tab ===

void MainWindow::setupPlannerSettingsTab()
{
    if (!ui->tabWidget_2) {
        return;
    }

    auto* tab = new QWidget(ui->tabWidget_2);
    tab->setObjectName(QStringLiteral("plannerSettingsTab"));
    auto* root = new QVBoxLayout(tab);
    root->setContentsMargins(16, 12, 16, 12);
    root->setSpacing(12);

    auto* card = new QFrame(tab);
    Theme::setCard(card);
    auto* grid = new QGridLayout(card);
    grid->setContentsMargins(14, 10, 14, 14);
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(10);
    grid->setColumnStretch(1, 1);

    auto* title = new QLabel(QStringLiteral("Verbindung zum Palettenplaner"), card);
    Theme::setRole(title, "heading");
    grid->addWidget(title, 0, 0, 1, 2);

    const auto addRow = [card, grid](int row, const QString& caption) {
        auto* captionLabel = new QLabel(caption, card);
        captionLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        Theme::setRole(captionLabel, "muted");
        auto* valueLabel = new QLabel(card);
        valueLabel->setWordWrap(true);
        grid->addWidget(captionLabel, row, 0);
        grid->addWidget(valueLabel, row, 1);
        return valueLabel;
    };
    m_plannerSettingsServer = addRow(1, QStringLiteral("Planer-Server"));
    m_plannerSettingsHmi = addRow(2, QStringLiteral("HMI-ID"));
    m_plannerSettingsToken = addRow(3, QStringLiteral("Token"));
    m_plannerSettingsCa = addRow(4, QStringLiteral("CA-Zertifikat"));
    m_plannerSettingsState = addRow(5, QStringLiteral("Status"));
    for (QLabel* label : {m_plannerSettingsServer, m_plannerSettingsHmi, m_plannerSettingsCa}) {
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    }
    root->addWidget(card);

    auto* help = new QLabel(QStringLiteral(
        "Konfigurationsdatei hmi-<Name>.json aus dem Palettenplaner (Admin > HMIs) auf den USB-Stick "
        "kopieren, optional mit dem Werks-CA-Zertifikat (.pem) daneben."), tab);
    help->setWordWrap(true);
    Theme::setRole(help, "muted");
    root->addWidget(help);

    auto* buttons = new QHBoxLayout();
    buttons->setSpacing(12);
    auto* importButton = new QPushButton(QStringLiteral("Von USB importieren"), tab);
    importButton->setObjectName(QStringLiteral("plannerImportButton"));
    importButton->setMinimumWidth(260);
    Theme::setRole(importButton, "primary");
    auto* syncButton = new QPushButton(QStringLiteral("Jetzt abgleichen"), tab);
    syncButton->setObjectName(QStringLiteral("plannerSyncNowButton"));
    syncButton->setMinimumWidth(220);
    auto* removeButton = new QPushButton(QStringLiteral("Konfiguration entfernen"), tab);
    removeButton->setObjectName(QStringLiteral("plannerRemoveButton"));
    removeButton->setMinimumWidth(280);
    Theme::setRole(removeButton, "danger");
    buttons->addWidget(importButton);
    buttons->addWidget(syncButton);
    buttons->addStretch();
    buttons->addWidget(removeButton);
    root->addLayout(buttons);
    root->addStretch();

    connect(importButton, &QPushButton::clicked, this, &MainWindow::onPlannerImportClicked);
    connect(removeButton, &QPushButton::clicked, this, &MainWindow::onPlannerRemoveClicked);
    connect(syncButton, &QPushButton::clicked, this, [this]() {
        if (m_plannerSync) {
            m_plannerSync->pollNow();
        }
    });

    ui->tabWidget_2->addTab(tab, QStringLiteral("Planer"));
    updatePlannerSettingsTab();
}

void MainWindow::updatePlannerSettingsTab()
{
    if (!m_plannerSettingsServer) {
        return;
    }

    planner::PlannerConfig config;
    if (m_plannerSync && m_plannerSync->isConfigured()) {
        config = m_plannerSync->config();
    } else if (m_settings) {
        config = planner::loadFromSettings(*m_settings);
    }

    const QString none = QStringLiteral("-");
    m_plannerSettingsServer->setText(config.serverUrl.isEmpty() ? none : config.serverUrl);
    m_plannerSettingsHmi->setText(config.hmiId.isEmpty() ? none : config.hmiId);
    // Never show the token itself.
    m_plannerSettingsToken->setText(config.token.isEmpty() ? QStringLiteral("nicht gesetzt")
                                                           : QStringLiteral("gesetzt"));
    m_plannerSettingsCa->setText(config.caCertPath.isEmpty()
                                     ? QStringLiteral("System-Zertifikate")
                                     : config.caCertPath);
    m_plannerSettingsState->setText(m_plannerSync ? m_plannerSync->stateText()
                                                  : QStringLiteral("Planer-Abgleich nicht verfügbar"));
}

void MainWindow::onPlannerImportClicked()
{
    if (!m_settings || !m_plannerSync) {
        return;
    }

    QString usbDir = m_usbMonitor ? m_usbMonitor->getUsbPath() : m_settings->usbPath();
    usbDir = resolveAgainstWorkingDir(usbDir);

    const QStringList files = planner::findConfigFiles(usbDir);
    if (files.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Planer-Konfiguration"),
                             QStringLiteral("Keine Konfigurationsdatei (hmi-*.json) auf dem USB-Stick gefunden:\n%1")
                                 .arg(usbDir));
        return;
    }

    QString chosen = files.first();
    if (files.size() > 1) {
        QStringList names;
        for (const QString& file : files) {
            names.append(QFileInfo(file).fileName());
        }
        bool ok = false;
        const QString name = QInputDialog::getItem(this, QStringLiteral("Planer-Konfiguration"),
                                                   QStringLiteral("Datei auswählen:"), names, 0, false, &ok);
        if (!ok || name.isEmpty()) {
            return;
        }
        chosen = files.at(names.indexOf(name));
    }

    const planner::ImportResult result = planner::importFromFile(chosen, QDir::currentPath());
    if (!result.ok) {
        QMessageBox::warning(this, QStringLiteral("Planer-Konfiguration"),
                             QStringLiteral("Import fehlgeschlagen: %1").arg(result.error));
        return;
    }

    planner::storeInSettings(*m_settings, result.config);
    if (!m_settings->save()) {
        QMessageBox::warning(this, QStringLiteral("Planer-Konfiguration"),
                             QStringLiteral("Einstellungen konnten nicht gespeichert werden."));
        return;
    }

    m_plannerSync->setConfig(result.config, QDir::currentPath());
    updatePlannerConnectionState();
    refreshPlannerList();

    QMessageBox::information(this, QStringLiteral("Planer-Konfiguration"),
                             QStringLiteral("Konfiguration importiert.\nServer: %1\nHMI-ID: %2\nCA-Zertifikat: %3")
                                 .arg(result.config.serverUrl, result.config.hmiId,
                                      result.caImported ? QStringLiteral("übernommen")
                                                        : QStringLiteral("System-Zertifikate")));
}

void MainWindow::onPlannerRemoveClicked()
{
    if (!m_settings || !m_plannerSync) {
        return;
    }
    if (!Theme::confirm(this, QStringLiteral("Planer-Konfiguration"),
                        QStringLiteral("Planer-Konfiguration entfernen? "
                                       "Bereits geladene Pläne bleiben erhalten."))) {
        return;
    }
    planner::clearFromSettings(*m_settings);
    if (!m_settings->save()) {
        qWarning() << "Planner: settings could not be saved after removing the configuration";
    }
    m_plannerSync->setConfig(planner::PlannerConfig(), QDir::currentPath());
    closeProposalDialog();
    m_pendingProposal.reset();
    updatePlannerConnectionState();
    updateProposalUi();
}

} // namespace ui
} // namespace multipack
