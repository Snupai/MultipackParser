/**
 * @file MainWindow.h
 * @brief Main application window using Qt Designer UI
 */
#ifndef MULTIPACK_UI_MAINWINDOW_H
#define MULTIPACK_UI_MAINWINDOW_H

#include <QMainWindow>
#include <QListWidgetItem>
#include <memory>
#include <QTimer>

QT_BEGIN_NAMESPACE
class QPushButton;
class QLabel;
class QThread;
class QProcess;
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
}
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
class AutoUpdater;
struct UpdateProgress;
}
class PasswordDialog;

namespace ui {

class NotificationPopup;
class DimensionInputHandler;
class VisualizationWidget;

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

signals:
    void serverStartRequested();
    void serverStopRequested();
    void paletteLoadRequested(const QString& fileName);

protected:
    void closeEvent(QCloseEvent* event) override;

public slots:
    void updateRobotStatus();
    void updatePaletteInfo();
    void showMessage(const QString& message);
    void appendConsoleLog(const QString& text);
    void setServerRunning(bool running, const QString& error = QString());

private slots:
    // Navigation
    void showMainMenu();
    void showRobotParameters();
    void showSettings();
    void showExperimental();
    void showPasswordDialog();

    // Main menu actions
    void onLoadPaletteClicked();
    void onStartServerClicked();
    void onParameterRobotClicked();
    void onSettingsClicked();
    void onVolumeToggleClicked();
    void onExperimentalClicked();
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

    // Experimental actions
    void onRobFileSelected(QListWidgetItem* item);
    void onDeselectRobFile();
    void onFilterChanged();
    void onClearFiltersClicked();
    void onLoadSelectedRobFile();
    void onScannerStatusChanged(const QString& status, const QString& imagePath);
    void onStatusChanged(const QString& message, message::StatusType type);

private:
    void setupConnections();
    void loadSettings();
    void loadRobFileList();
    void updateEnabledStates();
    void updateVolumeIcon();
    void setupPalettePlanCompleter();
    QStringList loadPalettePlanWordlist();
    void maybeStartUr20Ui();
    void maybeStartSafetyMonitor();
    void maybeStartRobotStatusMonitor();
    void setupStatusTab();
    void updateStatusTab(const robot::RobotStatus& status);
    void onStatusDetailsUpdated(const QString& polyscopeVersion,
                                const QString& serialNumber,
                                const QString& loadedProgram);
    void setupDimensionHandlers();
    void applyHeightChange(int height);
    void applyWeightChange(double weight);
    void revertDimensionChanges();
    void updateVisualizationFromPaletteData(const database::PaletteData& data);
    void showPaletteConfigDialog();
    void setupUr20Timers();
    void updateZwischenlagePopup();
    void updatePaletteClearIndicators();
    void onPaletteClearClicked(int paletteNumber);
    bool ensureRobotConnected();

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
    system::AutoUpdater* m_autoUpdater = nullptr;
    QProcess* m_consoleProcess = nullptr;

    // Page indices (matching stackedWidget pages)
    enum PageIndex {
        PAGE_MAIN_MENU = 0,
        PAGE_ROBOT_PARAMS = 1,
        PAGE_SETTINGS = 2,
        PAGE_EXPERIMENTAL = 3
    };

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
    QPushButton* m_palette1ClearIndicator = nullptr;
    QPushButton* m_palette2ClearIndicator = nullptr;
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
};

} // namespace ui
} // namespace multipack

#endif // MULTIPACK_UI_MAINWINDOW_H
