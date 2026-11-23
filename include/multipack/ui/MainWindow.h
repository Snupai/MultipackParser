/**
 * @file MainWindow.h
 * @brief Main application window using Qt Designer UI
 */
#ifndef MULTIPACK_UI_MAINWINDOW_H
#define MULTIPACK_UI_MAINWINDOW_H

#include <QMainWindow>
#include <QListWidgetItem>
#include <QDateTime>
#include <QPointer>
#include <QSet>
#include <QStringList>
#include <memory>
#include <optional>
#include <QTimer>

#include "multipack/planner/PlannerPlanList.h"
#include "multipack/planner/PlannerSyncService.h"
#include "multipack/robot/RobotEnums.h"

QT_BEGIN_NAMESPACE
class QPushButton;
class QLabel;
class QThread;
class QProcess;
class QCompleter;
class QStringListModel;
class QCheckBox;
class QDoubleSpinBox;
class QLineEdit;
class QListView;
class QListWidget;
class QSpinBox;
class QTableWidget;
class QTabWidget;
QT_END_NAMESPACE

// Forward declaration of generated UI class
namespace Ui {
class Form;
}

namespace multipack {

// Forward declarations
namespace config { class SettingsManager; }
namespace database {
class DatabaseManager;
struct PaletteData;
enum class SaveResult;
}
namespace network { class XmlRpcServer; }
namespace robot { class RobotController; }
namespace robot {
class RobotStatusMonitor;
struct RobotStatus;
}
namespace core { class GlobalState; }
namespace message { enum class StatusType; }
namespace audio {
class AudioManager;
class SafetyMonitor;
}
namespace system {
class UsbKeyCheck;
class UsbMonitor;
class AutoUpdater;
struct UpdateProgress;
}
class PasswordDialog;

namespace ui {

class NotificationPopup;
class DimensionInputHandler;
class VisualizationWidget;
class OnScreenKeyboard;
class QrCodeWidget;
class PlanProposalDialog;

/**
 * @class MainWindow
 * @brief Main application window using Qt Designer UI file
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // Set component references
    void setSettingsManager(config::SettingsManager* settings);
    void setDatabaseManager(database::DatabaseManager* database);
    void setRobotController(robot::RobotController* robot);
    void setGlobalState(core::GlobalState* state);
    void setAudioManager(audio::AudioManager* audio);
    void setAutoUpdater(system::AutoUpdater* updater);
    void setUsbMonitor(system::UsbMonitor* monitor);
    void setXmlRpcServer(network::XmlRpcServer* server);
    void setPlannerSync(planner::PlannerSyncService* sync);

signals:
    void serverStartRequested();
    void serverStopRequested();
    void paletteLoadRequested(const QString& fileName);

protected:
    void closeEvent(QCloseEvent* event) override;

public slots:
    void updateRobotStatus();
    /// Dashboard status from RobotStatusMonitor (also feeds the plan switch guard).
    void updateStatusTab(const robot::RobotStatus& status);
    void updatePaletteInfo();
    void showMessage(const QString& message);
    void appendConsoleLog(const QString& text);
    void setServerRunning(bool running, const QString& error = QString());

private slots:
    // Navigation (top bar)
    void showMainMenu();
    void showPlans();
    void showRobotParameters();
    void showQrCode();
    void showSettings();
    void showPasswordDialog();

    // Main menu actions
    void onLoadPaletteClicked();
    void onStartServerClicked();
    void onParameterRobotClicked();
    void onSettingsClicked();
    void onVolumeToggleClicked();
    void onEinzelpaketChanged(int state);
    void onLabelInvertChanged(int state);
    void onKartonhoeheChanged();
    void onGewichtChanged();
    void onStartlageChanged(int value);

    // Robot parameter actions
    void onRobotStartClicked();
    void onRobotStopClicked();
    void onRobotPauseClicked();
    void onStopRpcServerClicked();

    // Aufnahme tab actions
    void onVerschiebungXChanged(int value);
    void onVerschiebungYChanged(int value);
    void onKlemmungChanged(int state);
    void onAufnahmeServerStart();

    // Settings actions
    void onSaveSettingsClicked();
    void onUrModelChanged(int index);
    void onExitAppClicked();
    void onSearchUpdateClicked();
    void onUpdateCheckCompleted(bool success, const QString& message);
    void onUpdateDownloadProgress(const system::UpdateProgress& progress);
    void onUpdateDownloadCompleted(const QString& fileName);
    void onUpdateInstallationCompleted(bool success, const QString& message);
    void onUpdateFailed(const QString& error);
    void onSendCommandClicked();
    void onSelectRobPathClicked();
    void onSelectAudioPathClicked();
    void onSelectScannerSoundPathClicked();
    void onTestAlarmAudioClicked();
    void onTestScannerAudioClicked();
    void onImportRobFileClicked();
    void onOpenFileClicked();
    void onSaveOpenFileClicked();
    void onConsoleCommandEntered();
    void onScanner1OverwriteChanged(int state);
    void onScanner2OverwriteChanged(int state);
    void onScanner3OverwriteChanged(int state);

    // Plan library ("Pläne") actions
    void onDeselectRobFile();
    void onFilterChanged();
    void onClearFiltersClicked();
    void onDatabasePlanSelectionChanged();
    void onDatabaseSaveClicked();
    void onDatabaseNewClicked();
    void onDatabaseDuplicateClicked();
    void onDatabaseDeleteClicked();
    void onDatabaseAddPositionClicked();
    void onDatabaseRemovePositionClicked();
    void onDatabaseAddRawRowClicked();
    void onDatabaseRemoveRawRowClicked();
    void onDatabaseAddRawColumnClicked();
    void onDatabaseRemoveRawColumnClicked();
    void onScannerStatusChanged(const QString& status, const QString& imagePath);
    void onStatusChanged(const QString& message, message::StatusType type);

private:
    void setupConnections();
    void setupNavigation();
    void applyResourceIcons();
    void loadSettings();
    void loadRobFileList();
    void updateEnabledStates();
    void updateVolumeIcon();
    void setupPalettePlanCompleter();
    void refreshPalettePlanCompleter();
    void updatePalettePlanCompletionPopup(const QString& text);
    void hidePalettePlanCompletionPopup();
    void maybeStartUr20Ui();
    void maybeStartSafetyMonitor();
    void maybeStartRobotStatusMonitor();
    void stopRobotStatusMonitor();
    void setupStatusTab();
    void onStatusDetailsUpdated(const QString& polyscopeVersion,
                                const QString& serialNumber,
                                const QString& loadedProgram);
    void setupDimensionHandlers();
    void applyHeightChange(int height);
    void applyWeightChange(double weight);
    void revertDimensionChanges();
    void updateVisualizationFromPaletteData(const database::PaletteData& data);
    void showPaletteConfigDialog();
    void setupDatabaseManagerTab();
    void refreshDatabaseManagerPlans();
    void loadDatabaseEditor(const database::PaletteData& data);
    bool collectDatabaseEditorData(database::PaletteData& data) const;
    QString selectedDatabasePlanFileName() const;
    QString databasePlanNameInput() const;
    static QString paletteStorageName(const QString& displayName);
    void setupUr20Timers();
    void updateZwischenlagePopup();
    void updatePaletteClearIndicators();
    void onPaletteClearClicked(int paletteNumber);
    void incrementUseCycleCount();
    bool ensureRobotConnected();

    // Loading a plan into GlobalState (shared by USB plans and planner Fertigstände)
    void applyLoadedPalette(const database::PaletteData& data, const QString& displayName);
    [[nodiscard]] QString paletteDisplayName(const QString& fileName) const;

    // Planner (HMI-Abgleich) - implemented in MainWindowPlanner.cpp
    void setupPlannerPanel();
    void updatePlannerAttention(bool newVersion, int failedCount);
    void updatePlannerSelection();
    void setupPlannerSettingsTab();
    void refreshPlannerList();
    void updatePlannerConnectionState();
    void updatePlannerSettingsTab();
    void onPlannerLoadClicked();
    void onPlannerImportClicked();
    void onPlannerRemoveClicked();
    void onActivePlanFileChanged();
    void reportActivePlan(bool force);
    [[nodiscard]] QString currentActiveFinalId() const;
    [[nodiscard]] planner::SwitchPermission currentSwitchPermission() const;
    [[nodiscard]] static QString switchPermissionText(planner::SwitchPermission permission);
    void onProposalAvailable(const planner::Proposal& proposal);
    void onProposalCleared(const QString& proposalId);
    void showUnloadableProposalNotices();
    void onProposalAnswered(const QString& proposalId, bool accepted,
                            planner::PlannerSyncService::AnswerOutcome outcome);
    void openProposalDialog();
    void closeProposalDialog();
    void updateProposalUi();

    // Generated UI
    Ui::Form* ui = nullptr;

    // Component references (not owned)
    config::SettingsManager* m_settings = nullptr;
    database::DatabaseManager* m_database = nullptr;
    robot::RobotController* m_robot = nullptr;
    core::GlobalState* m_state = nullptr;
    audio::AudioManager* m_audio = nullptr;
    std::unique_ptr<audio::SafetyMonitor> m_safetyMonitor;
    robot::RobotStatusMonitor* m_statusMonitor = nullptr;
    QThread* m_statusThread = nullptr;
    std::unique_ptr<DimensionInputHandler> m_dimensionHandler;
    system::UsbKeyCheck* m_usbKeyCheck = nullptr;
    system::UsbMonitor* m_usbMonitor = nullptr;
    system::AutoUpdater* m_autoUpdater = nullptr;
    network::XmlRpcServer* m_xmlRpcServer = nullptr;
    QProcess* m_consoleProcess = nullptr;
    planner::PlannerSyncService* m_plannerSync = nullptr;

    // Page indices (matching stackedWidget pages)
    enum PageIndex {
        PAGE_MAIN_MENU = 0,
        PAGE_ROBOT_PARAMS = 1,
        PAGE_SETTINGS = 2,
        PAGE_PLANS = 3,
        PAGE_QR_CODE = 4
    };

    void setCurrentPage(PageIndex page);
    void syncNavigation();

    // State
    bool m_serverRunning = false;
    bool m_paletteLoaded = false;
    bool m_volumeOn = true;
    bool m_weightEstimated = false;
    bool m_ur20UiInitialized = false;
    QString m_currentPaletteFile;
    int m_packageLength = 0;
    int m_packageWidth = 0;
    int m_lastConfirmedHeight = 0;
    double m_lastConfirmedWeight = 0.0;

    QWidget* m_centralWidget = nullptr;
    QTimer* m_zwischenlageTimer = nullptr;
    QTimer* m_paletteClearTimer = nullptr;
    NotificationPopup* m_zwischenlagePopup = nullptr;
    VisualizationWidget* m_visualizationWidget = nullptr;
    QLabel* m_statusRobotIp = nullptr;
    QLabel* m_statusConnection = nullptr;
    QLabel* m_statusRobotMode = nullptr;
    QLabel* m_statusSafetyStatus = nullptr;
    QLabel* m_statusProgramState = nullptr;
    QLabel* m_statusLastUpdate = nullptr;
    QLabel* m_statusPolyscopeVersion = nullptr;
    QLabel* m_statusSerialNumber = nullptr;
    QLabel* m_statusLoadedProgram = nullptr;
    OnScreenKeyboard* m_onScreenKeyboard = nullptr;

    // Plan list on "Pläne" (planner plans and USB plans), QR code page and
    // planner settings tab. The widgets live in MainWindow.ui; these point into it.
    QListWidget* m_plannerList = nullptr;
    QPushButton* m_plannerLoadButton = nullptr;
    QPushButton* m_plannerProposalButton = nullptr;
    QLabel* m_plannerStateLabel = nullptr;
    QLabel* m_plannerHintLabel = nullptr;
    QLabel* m_plannerBadge = nullptr;       ///< Count on the "Pläne" tab
    bool m_plannerRowsShown = false;        ///< The list currently includes planner plans
    QString m_selectedPlanKey;              ///< Last previewed selection of the plan list
    bool m_planFilterActive = false;        ///< Package size filter of the plan list
    int m_planFilterLength = 0;             ///< 0 = any
    int m_planFilterWidth = 0;
    int m_planFilterHeight = 0;
    QSet<QString> m_planFilterNames;        ///< Local plans matching the filter (names without ".rob")
    QString m_newVersionFinalId;            ///< Newer Fertigstand of the running plan, if any
    QrCodeWidget* m_plannerQr = nullptr;
    QLabel* m_plannerQrStateLabel = nullptr;
    QLabel* m_plannerQrUrlLabel = nullptr;
    QLabel* m_plannerSettingsServer = nullptr;
    QLabel* m_plannerSettingsHmi = nullptr;
    QLabel* m_plannerSettingsToken = nullptr;
    QLabel* m_plannerSettingsCa = nullptr;
    QLabel* m_plannerSettingsState = nullptr;
    QTimer* m_activePlanDebounce = nullptr;
    QString m_reportedActiveFinalId;
    bool m_activePlanReported = false;

    // Planvorschlag
    std::optional<planner::Proposal> m_pendingProposal;
    QString m_deferredProposalId;
    QPointer<PlanProposalDialog> m_proposalDialog;

    // Last robot status from the dashboard monitor (for the switch guard)
    bool m_robotStatusConnected = false;
    robot::ProgramState m_robotProgramState = robot::ProgramState::Unknown;
    QDateTime m_robotStatusTime;
    QListView* m_palettePlanCompletionView = nullptr;
    QStringListModel* m_palettePlanCompleterModel = nullptr;
    QStringList m_palettePlanCompletionWords;
    QListWidget* m_databasePlanList = nullptr;
    QLineEdit* m_databasePlanNameEdit = nullptr;
    QSpinBox* m_databasePaketQuerSpin = nullptr;
    QDoubleSpinBox* m_databaseCogXSpin = nullptr;
    QDoubleSpinBox* m_databaseCogYSpin = nullptr;
    QDoubleSpinBox* m_databaseCogZSpin = nullptr;
    QSpinBox* m_databaseLayerTypesSpin = nullptr;
    QSpinBox* m_databaseLayerCountSpin = nullptr;
    QSpinBox* m_databasePackageCountSpin = nullptr;
    QSpinBox* m_databasePalletLengthSpin = nullptr;
    QSpinBox* m_databasePalletWidthSpin = nullptr;
    QSpinBox* m_databasePalletHeightSpin = nullptr;
    QSpinBox* m_databasePackageLengthSpin = nullptr;
    QSpinBox* m_databasePackageWidthSpin = nullptr;
    QSpinBox* m_databasePackageHeightSpin = nullptr;
    QSpinBox* m_databasePackageGapSpin = nullptr;
    QDoubleSpinBox* m_databasePackageWeightSpin = nullptr;
    QCheckBox* m_databaseEinzelpaketCheck = nullptr;
    QTableWidget* m_databasePositionsTable = nullptr;
    QTableWidget* m_databaseLayersTable = nullptr;
    QTableWidget* m_databaseRawTable = nullptr;
    std::unique_ptr<database::PaletteData> m_databaseEditorData;
    QString m_databaseEditorOriginalFileName;
};

} // namespace ui
} // namespace multipack

#endif // MULTIPACK_UI_MAINWINDOW_H
