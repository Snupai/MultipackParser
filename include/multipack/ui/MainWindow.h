/**
 * @file MainWindow.h
 * @brief Main application window with full UI
 */
#ifndef MULTIPACK_UI_MAINWINDOW_H
#define MULTIPACK_UI_MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QTabWidget>
#include <QComboBox>
#include <QTextEdit>
#include <QTreeView>
#include <QListWidget>
#include <QFrame>
#include <QToolButton>
#include <QTimer>
#include <memory>

namespace multipack {

// Forward declarations
namespace config { class SettingsManager; }
namespace database { class DatabaseManager; }
namespace robot { class RobotController; }
namespace core { class GlobalState; }

namespace ui {

/**
 * @class MainWindow
 * @brief Main application window with all UI components
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

signals:
    void serverStartRequested();
    void serverStopRequested();
    void paletteLoadRequested(const QString& fileName);

public slots:
    void updateRobotStatus();
    void updatePaletteInfo();
    void showMessage(const QString& message);
    void appendConsoleLog(const QString& text);

private slots:
    // Navigation
    void showMainMenu();
    void showRobotParameters();
    void showSettings();
    void showExperimental();

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
    void onSendCommandClicked();
    void onSelectRobPathClicked();
    void onSelectAudioPathClicked();
    void onSelectScannerSoundPathClicked();
    void onOpenFileClicked();
    void onConsoleCommandEntered();

    // Experimental actions
    void onRobFileSelected(QListWidgetItem* item);
    void onDeselectRobFile();
    void onFilterChanged();
    void onClearFiltersClicked();
    void onLoadSelectedRobFile();

private:
    void setupUi();
    void setupMainMenuPage();
    void setupRobotParametersPage();
    void setupSettingsPage();
    void setupExperimentalPage();
    void setupConnections();
    void loadSettings();
    void loadRobFileList();
    void applyStyleSheet();
    void updateEnabledStates();

    // Component references (not owned)
    config::SettingsManager* m_settings = nullptr;
    database::DatabaseManager* m_database = nullptr;
    robot::RobotController* m_robot = nullptr;
    core::GlobalState* m_state = nullptr;

    // Main stacked widget
    QStackedWidget* m_stackedWidget = nullptr;

    // Page indices
    enum PageIndex {
        PAGE_MAIN_MENU = 0,
        PAGE_ROBOT_PARAMS = 1,
        PAGE_SETTINGS = 2,
        PAGE_EXPERIMENTAL = 3
    };

    // === Main Menu Widgets ===
    QWidget* m_mainMenuPage = nullptr;
    QPushButton* m_buttonSettings = nullptr;
    QPushButton* m_buttonVolumeOnOff = nullptr;
    QLabel* m_labelPaletteInfo = nullptr;
    QLabel* m_labelLogo = nullptr;
    QLineEdit* m_inputPalettePlan = nullptr;
    QPushButton* m_buttonLoadPalette = nullptr;
    QSpinBox* m_inputStartlage = nullptr;
    QLineEdit* m_inputKartonhoehe = nullptr;
    QLineEdit* m_inputKartonGewicht = nullptr;
    QLabel* m_labelGewichtInfo = nullptr;
    QCheckBox* m_checkEinzelpaket = nullptr;
    QCheckBox* m_checkLabelInvert = nullptr;
    QPushButton* m_buttonParameterRobot = nullptr;
    QPushButton* m_buttonStartServer = nullptr;
    QPushButton* m_buttonExperimental = nullptr;

    // === Robot Parameters Page ===
    QWidget* m_robotParamsPage = nullptr;
    QTabWidget* m_robotTabWidget = nullptr;

    // Roboter tab
    QPushButton* m_buttonZurueck1 = nullptr;
    QPushButton* m_buttonRobotStart = nullptr;
    QPushButton* m_buttonRobotStop = nullptr;
    QPushButton* m_buttonRobotPause = nullptr;
    QPushButton* m_buttonStopRpcServer = nullptr;

    // Aufnahme tab
    QPushButton* m_buttonZurueck2 = nullptr;
    QLabel* m_imageAufnahmePos = nullptr;
    QSpinBox* m_inputVerschiebungX = nullptr;
    QSpinBox* m_inputVerschiebungY = nullptr;
    QPushButton* m_buttonAufnahmeServer = nullptr;
    QCheckBox* m_checkKlemmung = nullptr;

    // === Settings Page ===
    QWidget* m_settingsPage = nullptr;
    QTabWidget* m_settingsTabWidget = nullptr;

    // Info tab
    QPushButton* m_buttonZurueck3 = nullptr;
    QPushButton* m_buttonSaveSettings1 = nullptr;
    QComboBox* m_comboUrModel = nullptr;
    QLineEdit* m_lineEditUrSerial = nullptr;
    QLineEdit* m_lineEditUrManufDate = nullptr;
    QLineEdit* m_lineEditUrSoftwareVer = nullptr;
    QLineEdit* m_lineEditUrName = nullptr;
    QLineEdit* m_lineEditUrStandort = nullptr;
    QLineEdit* m_lineEditNumPlans = nullptr;
    QLineEdit* m_lineEditNumCycles = nullptr;
    QLineEdit* m_lineEditLastRestart = nullptr;
    QLineEdit* m_lineEditCurrentVersion = nullptr;
    QLineEdit* m_lineEditDisplayModel = nullptr;
    QLineEdit* m_lineEditDisplayRefreshRate = nullptr;
    QLineEdit* m_lineEditDisplayWidth = nullptr;
    QLineEdit* m_lineEditDisplayHeight = nullptr;
    QPushButton* m_buttonExitApp = nullptr;
    QPushButton* m_buttonSearchUpdate = nullptr;

    // Admin tab
    QPushButton* m_buttonZurueck4 = nullptr;
    QPushButton* m_buttonSaveSettings2 = nullptr;
    QLineEdit* m_lineEditPassword = nullptr;
    QLineEdit* m_lineEditRobPath = nullptr;
    QToolButton* m_buttonSelectRobPath = nullptr;
    QLineEdit* m_lineEditAudioPath = nullptr;
    QToolButton* m_buttonSelectAudioPath = nullptr;
    QLineEdit* m_lineEditScannerSoundPath = nullptr;
    QToolButton* m_buttonSelectScannerSoundPath = nullptr;
    QCheckBox* m_checkScanner1Overwrite = nullptr;
    QCheckBox* m_checkScanner2Overwrite = nullptr;
    QCheckBox* m_checkScanner3Overwrite = nullptr;
    QPushButton* m_buttonSendCommand = nullptr;
    QComboBox* m_comboRemoteCommand = nullptr;

    // Editor tab
    QPushButton* m_buttonZurueck5 = nullptr;
    QPushButton* m_buttonSaveSettings3 = nullptr;
    QLineEdit* m_lineEditFilePath = nullptr;
    QPushButton* m_buttonOpenFile = nullptr;
    QTextEdit* m_textEditFile = nullptr;

    // Explorer tab
    QPushButton* m_buttonZurueck6 = nullptr;
    QPushButton* m_buttonSaveSettings4 = nullptr;
    QTreeView* m_treeView = nullptr;

    // Console tab
    QPushButton* m_buttonZurueck7 = nullptr;
    QTextEdit* m_textEditConsole = nullptr;
    QLineEdit* m_lineEditCommand = nullptr;

    // === Experimental Page ===
    QWidget* m_experimentalPage = nullptr;
    QPushButton* m_buttonZurueck8 = nullptr;
    QListWidget* m_robFilesListWidget = nullptr;
    QPushButton* m_buttonDeselectRobFile = nullptr;
    QPushButton* m_buttonLoadRobFile = nullptr;
    QFrame* m_matplotlibFrame = nullptr;
    QLineEdit* m_lineEditFilterLength = nullptr;
    QLineEdit* m_lineEditFilterWidth = nullptr;
    QLineEdit* m_lineEditFilterHeight = nullptr;
    QPushButton* m_buttonClearFilters = nullptr;

    // State
    bool m_serverRunning = false;
    bool m_paletteLoaded = false;
    bool m_volumeOn = true;
    QString m_currentPaletteFile;
};

} // namespace ui
} // namespace multipack

#endif // MULTIPACK_UI_MAINWINDOW_H
