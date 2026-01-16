/**
 * @file MainWindow.h
 * @brief Main application window using Qt Designer UI
 */
#ifndef MULTIPACK_UI_MAINWINDOW_H
#define MULTIPACK_UI_MAINWINDOW_H

#include <QMainWindow>
#include <QListWidgetItem>
#include <memory>

// Forward declaration of generated UI class
namespace Ui {
class Form;
}

namespace multipack {

// Forward declarations
namespace config { class SettingsManager; }
namespace database { class DatabaseManager; }
namespace robot { class RobotController; }
namespace core { class GlobalState; }
namespace audio { class AudioManager; }

namespace ui {

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
    void onEinzelpaketChanged(Qt::CheckState state);
    void onLabelInvertChanged(Qt::CheckState state);
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
    void onKlemmungChanged(Qt::CheckState state);
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
    void setupConnections();
    void loadSettings();
    void loadRobFileList();
    void updateEnabledStates();
    void updateVolumeIcon();

    // Generated UI
    Ui::Form* ui = nullptr;

    // Component references (not owned)
    config::SettingsManager* m_settings = nullptr;
    database::DatabaseManager* m_database = nullptr;
    robot::RobotController* m_robot = nullptr;
    core::GlobalState* m_state = nullptr;
    audio::AudioManager* m_audio = nullptr;

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
    QString m_currentPaletteFile;
};

} // namespace ui
} // namespace multipack

#endif // MULTIPACK_UI_MAINWINDOW_H
