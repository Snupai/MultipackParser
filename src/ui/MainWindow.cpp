/**
 * @file MainWindow.cpp
 * @brief Implementation of main window using Qt Designer UI
 */
#include "multipack/ui/MainWindow.h"
#include "ui_MainWindow.h"

#include "multipack/config/SettingsManager.h"
#include "multipack/database/DatabaseManager.h"
#include "multipack/robot/RobotController.h"
#include "multipack/core/GlobalState.h"
#include "multipack/audio/AudioManager.h"
#include "multipack/audio/SafetyMonitor.h"
#include "multipack/config/ConfigDefaults.h"
#include "multipack/system/UsbKeyCheck.h"
#include "multipack/ui/PasswordDialog.h"
#include "multipack/ui/InputValidation.h"
#include "multipack/ui/PaletteConfigDialog.h"
#include "multipack/ui/NotificationPopup.h"
#include "multipack/message/StatusManager.h"
#include "multipack/robot/RobotStatusMonitor.h"
#include "multipack/system/RobFileParser.h"
#include "multipack/ui/VisualizationWidget.h"
#include "multipack/system/AutoUpdater.h"

#include <QApplication>
#include <QScreen>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QFileSystemModel>
#include <QCompleter>
#include <QDebug>
#include <QIcon>
#include <QPixmap>
#include <QCloseEvent>
#include <QShortcut>
#include <QPushButton>
#include <QTimer>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QThread>

namespace multipack {
namespace ui {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::Form)
    , m_usbKeyCheck(new system::UsbKeyCheck(this))
{
    qDebug() << "MainWindow - initializing with UI file";

    // Create central widget and setup UI
    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);
    ui->setupUi(m_centralWidget);

    auto* closeShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::ALT | Qt::Key_C), this);
    closeShortcut->setContext(Qt::ApplicationShortcut);
    connect(closeShortcut, &QShortcut::activated, qApp, &QApplication::quit);
    
    // Set up auto-completion for palette plan files
    setupPalettePlanCompleter();
    
    // Setup connections
    setupConnections();
    setupStatusTab();
    setupDimensionHandlers();

    if (ui->MatplotLibCanvasFrame) {
        auto* layout = new QVBoxLayout(ui->MatplotLibCanvasFrame);
        layout->setContentsMargins(0, 0, 0, 0);
        m_visualizationWidget = new VisualizationWidget(ui->MatplotLibCanvasFrame);
        layout->addWidget(m_visualizationWidget);
        m_visualizationWidget->lower();
    }

    auto& statusManager = message::StatusManager::instance();
    connect(&statusManager, &message::StatusManager::statusChanged,
            this, &MainWindow::onStatusChanged);
    connect(&statusManager, &message::StatusManager::statusCleared,
            this, [this]() { ui->label_GewichtInfo->clear(); });

    // Start on main menu
    ui->stackedWidget->setCurrentIndex(PAGE_MAIN_MENU);

    // Set window properties
    setWindowTitle("Palletierer");
    setFixedSize(1280, 720);

    qDebug() << "MainWindow - initialized";
}

MainWindow::~MainWindow()
{
    // Thread cleanup is already handled in closeEvent
    delete ui;
    qDebug() << "MainWindow - destroyed";
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    qDebug() << "MainWindow - close event received";

    // Stop status monitor (thread-safe via atomic flag)
    if (m_statusMonitor) {
        m_statusMonitor->stop();
    }

    // Quit the thread event loop
    if (m_statusThread) {
        m_statusThread->quit();
        // Wait for thread to finish (up to 3 seconds)
        if (!m_statusThread->wait(3000)) {
            qWarning() << "RobotStatusMonitor thread did not finish, terminating";
            m_statusThread->terminate();
            m_statusThread->wait(1000);
        }
        qDebug() << "RobotStatusMonitor thread stopped";
    }

    // Accept the close event
    event->accept();
}

void MainWindow::setSettingsManager(config::SettingsManager* settings)
{
    m_settings = settings;
    loadSettings();
    maybeStartUr20Ui();
    maybeStartRobotStatusMonitor();
}

void MainWindow::setDatabaseManager(database::DatabaseManager* database)
{
    m_database = database;
    loadRobFileList();
}

void MainWindow::setRobotController(robot::RobotController* robot)
{
    m_robot = robot;
    updateRobotStatus();
    maybeStartSafetyMonitor();
}

void MainWindow::setGlobalState(core::GlobalState* state)
{
    m_state = state;
    if (m_state) {
        connect(m_state, &core::GlobalState::scannerStatusChanged,
                this, &MainWindow::onScannerStatusChanged);
    }
    maybeStartUr20Ui();
}

void MainWindow::setAudioManager(audio::AudioManager* audio)
{
    m_audio = audio;
    if (m_audio) {
        m_audio->setEnabled(m_volumeOn);
    }
    maybeStartSafetyMonitor();
}

void MainWindow::setAutoUpdater(system::AutoUpdater* updater)
{
    m_autoUpdater = updater;
    if (m_autoUpdater) {
        connect(m_autoUpdater, &system::AutoUpdater::checkCompleted,
                this, &MainWindow::onUpdateCheckCompleted);
        connect(m_autoUpdater, &system::AutoUpdater::downloadProgress,
                this, &MainWindow::onUpdateDownloadProgress);
        connect(m_autoUpdater, &system::AutoUpdater::downloadCompleted,
                this, &MainWindow::onUpdateDownloadCompleted);
        connect(m_autoUpdater, &system::AutoUpdater::installationCompleted,
                this, &MainWindow::onUpdateInstallationCompleted);
        connect(m_autoUpdater, &system::AutoUpdater::updateFailed,
                this, &MainWindow::onUpdateFailed);
    }
}

void MainWindow::setupConnections()
{
    // Main menu navigation
    connect(ui->ButtonSettings, &QPushButton::clicked, this, &MainWindow::onSettingsClicked);
    connect(ui->pushButtonVolumeOnOff, &QPushButton::clicked, this, &MainWindow::onVolumeToggleClicked);
    connect(ui->openExperimentalTab, &QPushButton::clicked, this, &MainWindow::onExperimentalClicked);
    connect(ui->ButtonOpenParameterRoboter, &QPushButton::clicked, this, &MainWindow::onParameterRobotClicked);

    // Main menu actions
    connect(ui->LadePallettenplan, &QPushButton::clicked, this, &MainWindow::onLoadPaletteClicked);
    connect(ui->ButtonDatenSenden, &QPushButton::clicked, this, &MainWindow::onStartServerClicked);
    connect(ui->EingabePallettenplan, &QLineEdit::returnPressed, this, &MainWindow::onLoadPaletteClicked);
    connect(ui->checkBoxEinzelpaket, &QCheckBox::checkStateChanged, this, &MainWindow::onEinzelpaketChanged);
    connect(ui->checkBoxLabelInvert, &QCheckBox::checkStateChanged, this, &MainWindow::onLabelInvertChanged);
    connect(ui->EingabeStartlage, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onStartlageChanged);

    // Robot parameters - back buttons
    connect(ui->ButtonZurueck, &QPushButton::clicked, this, &MainWindow::showMainMenu);
    connect(ui->ButtonZurueck_2, &QPushButton::clicked, this, &MainWindow::showMainMenu);

    // Robot control buttons
    connect(ui->ButtonRoboterStart, &QPushButton::clicked, this, &MainWindow::onRobotStartClicked);
    connect(ui->ButtonRoboterStop, &QPushButton::clicked, this, &MainWindow::onRobotStopClicked);
    connect(ui->ButtonRoboterPause, &QPushButton::clicked, this, &MainWindow::onRobotPauseClicked);
    connect(ui->ButtonStopRPCServer, &QPushButton::clicked, this, &MainWindow::onStopRpcServerClicked);

    // Aufnahme tab
    connect(ui->EingabeVerschiebungX, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onVerschiebungXChanged);
    connect(ui->EingabeVerschiebungY, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onVerschiebungYChanged);
    connect(ui->checkBoxKlemmung, &QCheckBox::checkStateChanged, this, &MainWindow::onKlemmungChanged);
    connect(ui->ButtonDatenSenden_2, &QPushButton::clicked, this, &MainWindow::onAufnahmeServerStart);

    // Settings - back buttons
    connect(ui->ButtonZurueck_3, &QPushButton::clicked, this, &MainWindow::showMainMenu);
    connect(ui->ButtonZurueck_4, &QPushButton::clicked, this, &MainWindow::showMainMenu);
    connect(ui->ButtonZurueck_5, &QPushButton::clicked, this, &MainWindow::showMainMenu);
    connect(ui->ButtonZurueck_6, &QPushButton::clicked, this, &MainWindow::showMainMenu);
    connect(ui->ButtonZurueck_7, &QPushButton::clicked, this, &MainWindow::showMainMenu);

    // Settings actions
    connect(ui->pushButtonSpeichern, &QPushButton::clicked, this, &MainWindow::onSaveSettingsClicked);
    connect(ui->pushButtonSpeichern_2, &QPushButton::clicked, this, &MainWindow::onSaveSettingsClicked);
    connect(ui->pushButtonSpeichern_3, &QPushButton::clicked, this, &MainWindow::onSaveSettingsClicked);
    connect(ui->pushButtonSpeichern_4, &QPushButton::clicked, this, &MainWindow::onSaveSettingsClicked);
    connect(ui->comboBoxChooseURModel, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onUrModelChanged);
    connect(ui->pushButtonExitApp, &QPushButton::clicked, this, &MainWindow::onExitAppClicked);
    connect(ui->pushButtonSearchUpdate, &QPushButton::clicked, this, &MainWindow::onSearchUpdateClicked);
    connect(ui->pushButtonSendCommandRemoteControl, &QPushButton::clicked, this, &MainWindow::onSendCommandClicked);
    connect(ui->buttonSelectRobPath, &QToolButton::clicked, this, &MainWindow::onSelectRobPathClicked);
    connect(ui->buttonSelectAudioFilePath, &QToolButton::clicked, this, &MainWindow::onSelectAudioPathClicked);
    connect(ui->buttonSelectScannerWarningSoundPath, &QToolButton::clicked, this, &MainWindow::onSelectScannerSoundPathClicked);
    connect(ui->pushButtonTestAlarmAudio, &QPushButton::clicked, this, &MainWindow::onTestAlarmAudioClicked);
    connect(ui->pushButtonTestScannerAudio, &QPushButton::clicked, this, &MainWindow::onTestScannerAudioClicked);
    connect(ui->pushButtonImportRobFile, &QPushButton::clicked, this, &MainWindow::onImportRobFileClicked);
    connect(ui->pushButtonOpenFile, &QPushButton::clicked, this, &MainWindow::onOpenFileClicked);
    connect(ui->lineEditCommand, &QLineEdit::returnPressed, this, &MainWindow::onConsoleCommandEntered);

    // Scanner overwrite checkboxes (UR20 specific)
    connect(ui->checkBoxScanner1Overwrite, &QCheckBox::checkStateChanged, this, &MainWindow::onScanner1OverwriteChanged);
    connect(ui->checkBoxScanner2Overwrite, &QCheckBox::checkStateChanged, this, &MainWindow::onScanner2OverwriteChanged);
    connect(ui->checkBoxScanner3Overwrite, &QCheckBox::checkStateChanged, this, &MainWindow::onScanner3OverwriteChanged);

    // Experimental - back button and actions
    connect(ui->ButtonZurueck_8, &QPushButton::clicked, this, &MainWindow::showMainMenu);
    connect(ui->robFilesListWidget, &QListWidget::itemClicked, this, &MainWindow::onRobFileSelected);
    connect(ui->deselectRobFile, &QPushButton::clicked, this, &MainWindow::onDeselectRobFile);
    connect(ui->pushButtonClearFilters, &QPushButton::clicked, this, &MainWindow::onClearFiltersClicked);
    connect(ui->LadePallettenplan_2, &QPushButton::clicked, this, &MainWindow::onLoadSelectedRobFile);
    connect(ui->lineEditFilterLength, &QLineEdit::textChanged, this, &MainWindow::onFilterChanged);
    connect(ui->lineEditFilterWidth, &QLineEdit::textChanged, this, &MainWindow::onFilterChanged);
    connect(ui->lineEditFilterHeight, &QLineEdit::textChanged, this, &MainWindow::onFilterChanged);
}

void MainWindow::loadSettings()
{
    if (!m_settings) return;

    // Load settings into UI fields
    ui->lineEditCurrentVersion->setText(config::Defaults::VERSION);
    ui->lineEditLastRestart->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));

    // Load UR model and info from settings
    QString urModel = m_settings->value(config::Keys::INFO_UR_MODEL, "UR10").toString();
    ui->comboBoxChooseURModel->setCurrentText(urModel);

    ui->lineEditURSerialNo->setText(m_settings->value(config::Keys::INFO_UR_SERIAL_NUMBER).toString());
    ui->lineEditURManufacturingDate->setText(m_settings->value(config::Keys::INFO_UR_MANUFACTURING_DATE).toString());
    ui->lineEditURSoftwareVer->setText(m_settings->value(config::Keys::INFO_UR_SOFTWARE_VERSION).toString());
    ui->lineEditURName->setText(m_settings->value(config::Keys::INFO_PALLETTIERER_NAME).toString());
    ui->lineEditURStandort->setText(m_settings->value(config::Keys::INFO_PALLETTIERER_STANDORT).toString());

    // Load paths
    ui->pathEdit->setText(m_settings->value(config::Keys::SERVER_USB_PATH).toString());
    ui->audioPathEdit->setText(m_settings->value(config::Keys::ADMIN_ALARM_SOUND_FILE).toString());
    ui->scannerWarningSoundPathEdit->setText(
        m_settings->value(config::Keys::ADMIN_SCANNER_WARNING_SOUND_FILE).toString());

    if (m_audio) {
        m_audio->setCustomFile(audio::AudioType::Alarm, ui->audioPathEdit->text());
        m_audio->setCustomFile(audio::AudioType::ScannerWarning,
                               ui->scannerWarningSoundPathEdit->text());
    }

    if (m_statusMonitor) {
        QString robotIp = m_settings->robotIp();
        QMetaObject::invokeMethod(m_statusMonitor, [this, robotIp]() {
            m_statusMonitor->setRobotIp(robotIp);
        }, Qt::QueuedConnection);
    }

    int verschiebungX = m_settings->value("aufnahme.verschiebung_x", 0).toInt();
    int verschiebungY = m_settings->value("aufnahme.verschiebung_y", 0).toInt();
    ui->EingabeVerschiebungX->setValue(verschiebungX);
    ui->EingabeVerschiebungY->setValue(verschiebungY);
    if (m_state) {
        m_state->setPickOffsetX(verschiebungX);
        m_state->setPickOffsetY(verschiebungY);
    }

    // Display info
    QScreen* screen = QApplication::primaryScreen();
    if (screen) {
        ui->lineEditDisplayWidth->setText(QString::number(screen->size().width()));
        ui->lineEditDisplayHeight->setText(QString::number(screen->size().height()));
        ui->lineEditDisplayRefreshRate->setText(QString::number(screen->refreshRate()) + " Hz");
    }

    // Update volume icon
    updateVolumeIcon();

    qDebug() << "MainWindow - settings loaded";
}

void MainWindow::maybeStartUr20Ui()
{
    if (m_ur20UiInitialized || !m_settings || !m_state) {
        return;
    }

    QString urModel = m_settings->value(config::Keys::INFO_UR_MODEL, "UR10").toString();
    if (urModel != "UR20") {
        m_ur20UiInitialized = true;
        return;
    }

    m_ur20UiInitialized = true;
    setupUr20Timers();
    QTimer::singleShot(0, this, &MainWindow::showPaletteConfigDialog);
}

void MainWindow::maybeStartSafetyMonitor()
{
    if (m_safetyMonitor || !m_audio || !m_robot) {
        return;
    }

    m_safetyMonitor = std::make_unique<audio::SafetyMonitor>(m_audio, m_robot, this);
    m_safetyMonitor->setAudioAlertsEnabled(m_volumeOn);
    m_safetyMonitor->start(1000);
}

void MainWindow::maybeStartRobotStatusMonitor()
{
    if (m_statusMonitor || !m_settings || !ui->tabWidget) {
        return;
    }

    m_statusThread = new QThread(this);
    m_statusMonitor = new robot::RobotStatusMonitor();
    m_statusMonitor->moveToThread(m_statusThread);

    connect(m_statusThread, &QThread::finished, m_statusMonitor, &QObject::deleteLater);
    connect(m_statusMonitor, &robot::RobotStatusMonitor::statusUpdated,
            this, &MainWindow::updateStatusTab);
    connect(m_statusMonitor, &robot::RobotStatusMonitor::detailsUpdated,
            this, &MainWindow::onStatusDetailsUpdated);
    connect(m_statusMonitor, &robot::RobotStatusMonitor::connectionChanged,
            this, [this](bool connected) {
                if (m_statusConnection) {
                    m_statusConnection->setText(connected ? "Connected" : "Disconnected");
                }
            });

    QString robotIp = m_settings->robotIp();
    m_statusThread->start();
    QMetaObject::invokeMethod(m_statusMonitor, [this, robotIp]() {
        m_statusMonitor->setRobotIp(robotIp);
        m_statusMonitor->start(robot::RobotStatusMonitor::DEFAULT_INTERVAL_MS);
    }, Qt::QueuedConnection);
}

void MainWindow::setupStatusTab()
{
    if (!ui->tabWidget) {
        return;
    }

    auto* statusWidget = new QWidget(ui->tabWidget);
    auto* rootLayout = new QVBoxLayout(statusWidget);
    rootLayout->setContentsMargins(20, 20, 20, 20);
    rootLayout->setSpacing(12);

    auto* topBar = new QHBoxLayout();
    auto* backButton = new QPushButton(tr("Zurueck"), statusWidget);
    backButton->setCursor(Qt::PointingHandCursor);
    backButton->setFlat(true);
    connect(backButton, &QPushButton::clicked, this, &MainWindow::showMainMenu);
    topBar->addWidget(backButton);
    topBar->addStretch();
    rootLayout->addLayout(topBar);

    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);
    form->setFormAlignment(Qt::AlignTop);

    m_statusRobotIp = new QLabel("-", statusWidget);
    m_statusConnection = new QLabel("-", statusWidget);
    m_statusRobotMode = new QLabel("-", statusWidget);
    m_statusSafetyStatus = new QLabel("-", statusWidget);
    m_statusProgramState = new QLabel("-", statusWidget);
    m_statusLastUpdate = new QLabel("-", statusWidget);
    m_statusPolyscopeVersion = new QLabel("-", statusWidget);
    m_statusSerialNumber = new QLabel("-", statusWidget);
    m_statusLoadedProgram = new QLabel("-", statusWidget);

    form->addRow(tr("Robot IP:"), m_statusRobotIp);
    form->addRow(tr("Connection:"), m_statusConnection);
    form->addRow(tr("Robot Mode:"), m_statusRobotMode);
    form->addRow(tr("Safety Status:"), m_statusSafetyStatus);
    form->addRow(tr("Program State:"), m_statusProgramState);
    form->addRow(tr("Last Update:"), m_statusLastUpdate);
    form->addRow(tr("Polyscope Version:"), m_statusPolyscopeVersion);
    form->addRow(tr("Serial Number:"), m_statusSerialNumber);
    form->addRow(tr("Loaded Program:"), m_statusLoadedProgram);

    rootLayout->addLayout(form);

    ui->tabWidget->addTab(statusWidget, tr("Status"));
}

void MainWindow::updateStatusTab(const robot::RobotStatus& status)
{
    if (!m_statusRobotIp) {
        return;
    }

    auto modeText = [](robot::RobotMode mode) {
        switch (mode) {
            case robot::RobotMode::Running: return QString("Running");
            case robot::RobotMode::Idle: return QString("Idle");
            case robot::RobotMode::PowerOff: return QString("Power Off");
            case robot::RobotMode::BackDrive: return QString("Backdrive");
            case robot::RobotMode::ConfirmSafety: return QString("Confirm Safety");
            case robot::RobotMode::Booting: return QString("Booting");
            case robot::RobotMode::NoController: return QString("No Controller");
            case robot::RobotMode::Disconnected: return QString("Disconnected");
            default: return QString("Unknown");
        }
    };

    auto safetyText = [](robot::SafetyStatus safety) {
        switch (safety) {
            case robot::SafetyStatus::Normal: return QString("Normal");
            case robot::SafetyStatus::ReducedMode: return QString("Reduced Mode");
            case robot::SafetyStatus::ProtectiveStop: return QString("Protective Stop");
            case robot::SafetyStatus::Recovery: return QString("Recovery");
            case robot::SafetyStatus::SafeguardStop: return QString("Safeguard Stop");
            case robot::SafetyStatus::SystemEmergencyStop: return QString("System E-Stop");
            case robot::SafetyStatus::RobotEmergencyStop: return QString("Robot E-Stop");
            case robot::SafetyStatus::Violation: return QString("Violation");
            case robot::SafetyStatus::Fault: return QString("Fault");
            default: return QString("Unknown");
        }
    };

    auto programText = [](robot::ProgramState state) {
        switch (state) {
            case robot::ProgramState::Stopped: return QString("Stopped");
            case robot::ProgramState::Playing: return QString("Playing");
            case robot::ProgramState::Paused: return QString("Paused");
            default: return QString("Unknown");
        }
    };

    if (m_settings) {
        m_statusRobotIp->setText(m_settings->robotIp());
    }
    m_statusConnection->setText(status.isConnected ? "Connected" : "Disconnected");
    m_statusRobotMode->setText(modeText(status.robotMode));
    m_statusSafetyStatus->setText(safetyText(status.safetyStatus));
    m_statusProgramState->setText(programText(status.programState));

    if (status.lastUpdate.isValid()) {
        m_statusLastUpdate->setText(status.lastUpdate.toString("yyyy-MM-dd HH:mm:ss"));
    }
}

void MainWindow::onStatusDetailsUpdated(const QString& polyscopeVersion,
                                        const QString& serialNumber,
                                        const QString& loadedProgram)
{
    if (m_statusPolyscopeVersion && !polyscopeVersion.isEmpty()) {
        m_statusPolyscopeVersion->setText(polyscopeVersion);
    }
    if (m_statusSerialNumber && !serialNumber.isEmpty()) {
        m_statusSerialNumber->setText(serialNumber);
    }
    if (m_statusLoadedProgram && !loadedProgram.isEmpty()) {
        m_statusLoadedProgram->setText(loadedProgram);
    }
}

void MainWindow::setupDimensionHandlers()
{
    if (!ui->EingabeKartonhoehe || !ui->EingabeKartonGewicht) {
        return;
    }

    m_dimensionHandler = std::make_unique<DimensionInputHandler>(this);
    connect(ui->EingabeKartonhoehe, &QLineEdit::textChanged,
            m_dimensionHandler.get(), &DimensionInputHandler::onHeightChanged);
    connect(ui->EingabeKartonGewicht, &QLineEdit::textChanged,
            m_dimensionHandler.get(), &DimensionInputHandler::onWeightChanged);
    connect(m_dimensionHandler.get(), &DimensionInputHandler::confirmationNeeded,
            this, [this](const QString& message) {
                QMessageBox::StandardButton response = QMessageBox::question(
                    this,
                    tr("Bestaetigung"),
                    message,
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::No
                );
                if (response == QMessageBox::Yes) {
                    m_dimensionHandler->confirmChanges();
                } else {
                    m_dimensionHandler->cancelChanges();
                }
            });
    connect(m_dimensionHandler.get(), &DimensionInputHandler::heightConfirmed,
            this, &MainWindow::applyHeightChange);
    connect(m_dimensionHandler.get(), &DimensionInputHandler::weightConfirmed,
            this, &MainWindow::applyWeightChange);
    connect(m_dimensionHandler.get(), &DimensionInputHandler::changesReverted,
            this, &MainWindow::revertDimensionChanges);
    connect(m_dimensionHandler.get(), &DimensionInputHandler::validationError,
            this, &MainWindow::showMessage);
}

void MainWindow::applyHeightChange(int height)
{
    QSignalBlocker blocker(ui->EingabeKartonhoehe);
    ui->EingabeKartonhoehe->setText(QString::number(height));
    m_lastConfirmedHeight = height;

    if (m_database && !m_currentPaletteFile.isEmpty()) {
        if (!m_database->updateBoxDimensions(m_currentPaletteFile, height, -1.0, -1)) {
            qWarning() << "Failed to update box height in database";
        }
    }

    if (m_packageLength > 0 && m_packageWidth > 0) {
        bool ok = false;
        double currentWeight = ui->EingabeKartonGewicht->text().toDouble(&ok);
        if (m_weightEstimated || !ok || currentWeight <= 0.0) {
            double estimated = InputValidation::calculateEstimatedWeight(
                m_packageLength,
                m_packageWidth,
                height);
            m_weightEstimated = true;
            QSignalBlocker weightBlocker(ui->EingabeKartonGewicht);
            ui->EingabeKartonGewicht->setText(QString::number(estimated, 'f', 2));
            m_lastConfirmedWeight = estimated;
            if (m_database && !m_currentPaletteFile.isEmpty()) {
                (void)m_database->updateBoxDimensions(m_currentPaletteFile, -1, estimated, -1);
            }
        }
    }
}

void MainWindow::applyWeightChange(double weight)
{
    QSignalBlocker blocker(ui->EingabeKartonGewicht);
    ui->EingabeKartonGewicht->setText(QString::number(weight, 'f', 2));
    m_lastConfirmedWeight = weight;
    m_weightEstimated = false;

    if (m_database && !m_currentPaletteFile.isEmpty()) {
        if (!m_database->updateBoxDimensions(m_currentPaletteFile, -1, weight, -1)) {
            qWarning() << "Failed to update box weight in database";
        }
    }
}

void MainWindow::revertDimensionChanges()
{
    QSignalBlocker heightBlock(ui->EingabeKartonhoehe);
    QSignalBlocker weightBlock(ui->EingabeKartonGewicht);

    if (m_lastConfirmedHeight > 0) {
        ui->EingabeKartonhoehe->setText(QString::number(m_lastConfirmedHeight));
    }
    if (m_lastConfirmedWeight > 0.0) {
        ui->EingabeKartonGewicht->setText(QString::number(m_lastConfirmedWeight, 'f', 2));
    }
}

void MainWindow::showPaletteConfigDialog()
{
    if (!m_state) {
        return;
    }

    PaletteConfigDialog dialog(this);
    dialog.setWindowModality(Qt::ApplicationModal);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QJsonObject config = dialog.getConfiguration();
    bool palette1Empty = config.value("palette1_empty").toBool();
    bool palette2Empty = config.value("palette2_empty").toBool();
    int activePalette = config.value("active_palette").toInt();

    if ((activePalette == 1 && !palette1Empty) || (activePalette == 2 && !palette2Empty)) {
        QMessageBox::warning(this,
                             "Invalid Configuration",
                             "Cannot set a non-empty palette as active. Active palette has been reset to none (0)."
        );
        activePalette = 0;
    }

    m_state->setUr20Palette1Empty(palette1Empty);
    m_state->setUr20Palette2Empty(palette2Empty);
    m_state->setUr20ActivePalette(activePalette);

    if (palette1Empty) {
        m_state->setPalette1NonEmptyTimestamp(0);
    }
    if (palette2Empty) {
        m_state->setPalette2NonEmptyTimestamp(0);
    }

    updatePaletteClearIndicators();
}

void MainWindow::setupUr20Timers()
{
    if (!m_state) {
        return;
    }

    if (!m_zwischenlageTimer) {
        m_zwischenlageTimer = new QTimer(this);
        connect(m_zwischenlageTimer, &QTimer::timeout, this, &MainWindow::updateZwischenlagePopup);
        m_zwischenlageTimer->start(500);
    }

    if (!m_paletteClearTimer) {
        m_paletteClearTimer = new QTimer(this);
        connect(m_paletteClearTimer, &QTimer::timeout, this, &MainWindow::updatePaletteClearIndicators);
        m_paletteClearTimer->start(1000);
    }

    connect(m_state, &core::GlobalState::ur20StateChanged,
            this, &MainWindow::updateZwischenlagePopup);
    connect(m_state, &core::GlobalState::ur20StateChanged,
            this, &MainWindow::updatePaletteClearIndicators);

    updateZwischenlagePopup();
    updatePaletteClearIndicators();
}

void MainWindow::updateZwischenlagePopup()
{
    if (!m_state) {
        return;
    }

    if (m_state->ur20Zwischenlage()) {
        if (!m_zwischenlagePopup) {
            m_zwischenlagePopup = new NotificationPopup(this);
        }
        if (!m_zwischenlagePopup->isVisible()) {
            m_zwischenlagePopup->showMessage(
                "Zwischenlage legen und mit Reset bestatigen.",
                NotificationPopup::Type::Warning,
                0
            );
        }
    } else if (m_zwischenlagePopup) {
        m_zwischenlagePopup->closePopup();
        m_zwischenlagePopup->deleteLater();
        m_zwischenlagePopup = nullptr;
    }
}

void MainWindow::updatePaletteClearIndicators()
{
    if (!m_state) {
        return;
    }

    constexpr qint64 minWaitMs = 10000;
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    auto updateIndicator = [this, now](QPushButton*& indicator,
                                       int paletteNumber,
                                       bool isEmpty,
                                       qint64 nonEmptyTimestamp) {
        if (isEmpty || nonEmptyTimestamp == 0 || (now - nonEmptyTimestamp) < minWaitMs) {
            if (indicator) {
                indicator->hide();
            }
            return;
        }

        if (!indicator) {
            indicator = new QPushButton(m_centralWidget ? m_centralWidget : this);
            int top = paletteNumber == 1 ? 380 : 440;
            indicator->setGeometry(950, top, 200, 50);
            indicator->setStyleSheet(
                "background-color: rgba(255, 200, 0, 200);"
                "color: black;"
                "border-radius: 5px;"
                "padding: 5px;"
                "font-weight: bold;"
            );
            indicator->setFlat(true);
            indicator->setCursor(Qt::PointingHandCursor);
            indicator->setText(QString("Palette %1 freigeben").arg(paletteNumber));
            indicator->setToolTip(QString("Klicken Sie hier, um Palette %1 zum palettieren freizugeben")
                                   .arg(paletteNumber));
            connect(indicator, &QPushButton::clicked, this, [this, paletteNumber]() {
                onPaletteClearClicked(paletteNumber);
            });
        }

        indicator->show();
        indicator->raise();
    };

    updateIndicator(m_palette1ClearIndicator,
                    1,
                    m_state->ur20Palette1Empty(),
                    m_state->palette1NonEmptyTimestamp());
    updateIndicator(m_palette2ClearIndicator,
                    2,
                    m_state->ur20Palette2Empty(),
                    m_state->palette2NonEmptyTimestamp());
}

void MainWindow::onPaletteClearClicked(int paletteNumber)
{
    if (!m_state) {
        return;
    }

    QMessageBox::StandardButton response = QMessageBox::question(
        this,
        "Palette freigeben",
        QString("Mochten Sie Palette %1 zum palettieren freigeben?").arg(paletteNumber),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (response != QMessageBox::Yes) {
        return;
    }

    if (paletteNumber == 1) {
        m_state->setUr20Palette1Empty(true);
        m_state->setPalette1NonEmptyTimestamp(0);
    } else if (paletteNumber == 2) {
        m_state->setUr20Palette2Empty(true);
        m_state->setPalette2NonEmptyTimestamp(0);
    }

    updatePaletteClearIndicators();
}

void MainWindow::loadRobFileList()
{
    if (!m_database) return;

    ui->robFilesListWidget->clear();

    auto files = m_database->listAvailableFiles();
    for (const auto& file : files) {
        ui->robFilesListWidget->addItem(file.fileName);
    }

    ui->lineEditNumberPlans->setText(QString::number(files.size()));

    qDebug() << "MainWindow - loaded" << files.size() << "rob files";
}

void MainWindow::updateEnabledStates()
{
    bool paletteLoaded = m_paletteLoaded;

    // Enable/disable controls based on palette loaded state
    ui->EingabeStartlage->setEnabled(paletteLoaded);
    ui->EingabeKartonhoehe->setEnabled(paletteLoaded);
    ui->EingabeKartonGewicht->setEnabled(paletteLoaded);
    ui->checkBoxEinzelpaket->setEnabled(paletteLoaded);
    ui->checkBoxLabelInvert->setEnabled(paletteLoaded);
    ui->ButtonOpenParameterRoboter->setEnabled(paletteLoaded);
    ui->ButtonDatenSenden->setEnabled(paletteLoaded && !m_serverRunning);

    // Robot controls
    bool serverRunning = m_serverRunning;
    ui->ButtonRoboterStart->setEnabled(serverRunning);
    ui->ButtonRoboterStop->setEnabled(serverRunning);
    ui->ButtonRoboterPause->setEnabled(serverRunning);
    ui->ButtonStopRPCServer->setEnabled(serverRunning);
}

void MainWindow::updateVolumeIcon()
{
    if (m_volumeOn) {
        ui->pushButtonVolumeOnOff->setIcon(QIcon(":/Sound/imgs/volume-on.png"));
    } else {
        ui->pushButtonVolumeOnOff->setIcon(QIcon(":/Sound/imgs/volume-off.png"));
    }
}

// === Navigation Slots ===

void MainWindow::showMainMenu()
{
    ui->stackedWidget->setCurrentIndex(PAGE_MAIN_MENU);
}

void MainWindow::showRobotParameters()
{
    ui->stackedWidget->setCurrentIndex(PAGE_ROBOT_PARAMS);
}

void MainWindow::showSettings()
{
    ui->stackedWidget->setCurrentIndex(PAGE_SETTINGS);
}

void MainWindow::showPasswordDialog()
{
    PasswordDialog dialog(this, m_settings);
    
    // Show the dialog and wait for user response
    if (dialog.exec() == QDialog::Accepted && dialog.wasAccepted()) {
        qDebug() << "Password authenticated - opening settings";
        showSettings();
    } else {
        qDebug() << "Password authentication cancelled or failed";
    }
}

void MainWindow::showExperimental()
{
    ui->stackedWidget->setCurrentIndex(PAGE_EXPERIMENTAL);
}

// === Main Menu Slots ===

void MainWindow::onLoadPaletteClicked()
{
    QString fileName = ui->EingabePallettenplan->text().trimmed();
    if (fileName.isEmpty()) {
        showMessage("Bitte Palletierplan eingeben");
        return;
    }

    qDebug() << "Loading palette:" << fileName;

    if (m_database) {
        auto data = m_database->loadPaletteData(fileName);
        if (data.has_value()) {
            m_currentPaletteFile = fileName;
            m_paletteLoaded = true;
            m_packageLength = data->packageDimensions.length;
            m_packageWidth = data->packageDimensions.width;

            // Update UI with loaded data
            QSignalBlocker heightBlocker(ui->EingabeKartonhoehe);
            QSignalBlocker weightBlocker(ui->EingabeKartonGewicht);
            ui->EingabeKartonhoehe->setText(QString::number(data->packageDimensions.height));

            double weight = data->packageDimensions.weight;
            if (weight <= 0.0 && m_packageLength > 0 && m_packageWidth > 0) {
                weight = InputValidation::calculateEstimatedWeight(
                    m_packageLength,
                    m_packageWidth,
                    data->packageDimensions.height);
                m_weightEstimated = true;
                if (m_database && !m_currentPaletteFile.isEmpty()) {
                    (void)m_database->updateBoxDimensions(m_currentPaletteFile, -1, weight, -1);
                }
            } else {
                m_weightEstimated = false;
            }

            ui->EingabeKartonGewicht->setText(QString::number(weight, 'f', 2));
            m_lastConfirmedHeight = data->packageDimensions.height;
            m_lastConfirmedWeight = weight;
            ui->EingabeStartlage->setMaximum(data->metadata.anzLagen);
            ui->checkBoxEinzelpaket->setChecked(data->packageDimensions.einzelpaketLaengs);

            ui->LabelPalletenplanInfo->setText(QString("Geladen: %1 - %2 Lagen, %3 Pakete")
                .arg(fileName)
                .arg(data->metadata.anzLagen)
                .arg(data->metadata.anzahlPakete));

            updateEnabledStates();
            updateVisualizationFromPaletteData(*data);
            emit paletteLoadRequested(fileName);

            qDebug() << "Palette loaded successfully:" << fileName;
        } else {
            showMessage("Palletierplan nicht gefunden: " + fileName);
        }
    }
}

void MainWindow::onStartServerClicked()
{
    if (!m_paletteLoaded) {
        showMessage("Bitte zuerst Palletierplan laden");
        return;
    }

    m_serverRunning = true;
    ui->ButtonDatenSenden->setText("Server laeuft...");
    ui->ButtonDatenSenden->setEnabled(false);
    updateEnabledStates();

    emit serverStartRequested();
    qDebug() << "Server start requested";
}

void MainWindow::onParameterRobotClicked()
{
    showRobotParameters();
}

void MainWindow::onSettingsClicked()
{
    qDebug() << "Settings button clicked - checking for USB key or password";
    
    // First check for valid USB key
    if (m_usbKeyCheck && m_usbKeyCheck->isKeyPresent()) {
        qDebug() << "Valid USB key found - opening settings directly";
        showSettings();
    } else {
        qDebug() << "No valid USB key found - showing password dialog";
        showPasswordDialog();
    }
}

void MainWindow::onVolumeToggleClicked()
{
    m_volumeOn = !m_volumeOn;
    qDebug() << "Volume toggled:" << (m_volumeOn ? "ON" : "OFF");

    updateVolumeIcon();

    // Update audio manager
    if (m_audio) {
        m_audio->setEnabled(m_volumeOn);
    }

    if (m_safetyMonitor) {
        m_safetyMonitor->setAudioAlertsEnabled(m_volumeOn);
    }

    // Update global state
    if (m_state) {
        m_state->setAudioMuted(!m_volumeOn);
    }
}

void MainWindow::onScannerStatusChanged(const QString& status, const QString& imagePath)
{
    if (!imagePath.isEmpty() && ui->label_7) {
        ui->label_7->setPixmap(QPixmap(imagePath));
    }

    if (!m_audio || !m_settings || !m_state || !m_volumeOn) {
        return;
    }

    if (status == "True,True,True") {
        return;
    }

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 lastWarning = m_state->lastScannerWarningTime();
    QString previousStatus = m_state->previousScannerStatus();

    if (previousStatus != "True,True,True" && lastWarning != 0 && (now - lastWarning) < 15000) {
        return;
    }

    m_audio->playNotification(audio::AudioType::ScannerWarning);

    m_state->setLastScannerWarningTime(now);
}

void MainWindow::onExperimentalClicked()
{
    showExperimental();
}

void MainWindow::onEinzelpaketChanged(int state)
{
    bool checked = (state == Qt::Checked);
    qDebug() << "Einzelpaket changed:" << checked;

    // Update database
    if (m_database && !m_currentPaletteFile.isEmpty()) {
        if (!m_database->updateBoxDimensions(m_currentPaletteFile, -1, -1.0, checked ? 1 : 0)) {
            qWarning() << "Failed to update einzelpaket setting in database";
        }
    }

    // Update global state
    if (m_state) {
        m_state->setEinzelpaketLaengs(checked);
    }
}

void MainWindow::onLabelInvertChanged(int state)
{
    bool checked = (state == Qt::Checked);
    qDebug() << "Label invert changed:" << checked;

    // Update global state - label invert adds 180 to rotation in UR_PaketPos
    if (m_state) {
        m_state->setLabelInvert(checked);
    }
}

void MainWindow::onKartonhoeheChanged()
{
    QString text = ui->EingabeKartonhoehe->text();
    int height = text.toInt();
    qDebug() << "Kartonhoehe changed:" << height;
    applyHeightChange(height);
}

void MainWindow::onGewichtChanged()
{
    QString text = ui->EingabeKartonGewicht->text();
    double weight = text.toDouble();
    qDebug() << "Gewicht changed:" << weight;
    applyWeightChange(weight);
}

void MainWindow::onStartlageChanged(int value)
{
    qDebug() << "Startlage changed:" << value;

    // Update global state
    if (m_state) {
        m_state->setStartLayer(value);
        m_state->setCurrentLayer(value);
    }
}

// === Robot Parameter Slots ===

void MainWindow::onRobotStartClicked()
{
    qDebug() << "Robot start clicked";
    if (m_robot && m_robot->isConnected()) {
        m_robot->play();
    }
}

void MainWindow::onRobotStopClicked()
{
    qDebug() << "Robot stop clicked";
    if (m_robot && m_robot->isConnected()) {
        m_robot->stop();
    }
}

void MainWindow::onRobotPauseClicked()
{
    qDebug() << "Robot pause clicked";
    if (m_robot && m_robot->isConnected()) {
        m_robot->pause();
    }
}

void MainWindow::onStopRpcServerClicked()
{
    m_serverRunning = false;
    ui->ButtonDatenSenden->setText("Server starten");
    updateEnabledStates();

    emit serverStopRequested();
    qDebug() << "Server stop requested";
}

void MainWindow::onVerschiebungXChanged(int value)
{
    qDebug() << "Verschiebung X changed:" << value;

    // Store in settings for RPC server access
    if (m_settings) {
        m_settings->setValue("aufnahme.verschiebung_x", value);
    }

    if (m_state) {
        m_state->setPickOffsetX(value);
    }
}

void MainWindow::onVerschiebungYChanged(int value)
{
    qDebug() << "Verschiebung Y changed:" << value;

    // Store in settings for RPC server access
    if (m_settings) {
        m_settings->setValue("aufnahme.verschiebung_y", value);
    }

    if (m_state) {
        m_state->setPickOffsetY(value);
    }
}

void MainWindow::onKlemmungChanged(int state)
{
    bool checked = (state == Qt::Checked);
    qDebug() << "Klemmung changed:" << checked;

    // Update global state
    if (m_state) {
        m_state->setKlemmungAktiv(checked);
    }
}

void MainWindow::onAufnahmeServerStart()
{
    onStartServerClicked();
}

void MainWindow::onScanner1OverwriteChanged(int state)
{
    bool checked = (state == Qt::Checked);
    qDebug() << "Scanner 1 overwrite changed:" << checked;

    // Update global state for UR20 scanner override
    if (m_state) {
        m_state->setScannerOverride(0, checked);
    }
}

void MainWindow::onScanner2OverwriteChanged(int state)
{
    bool checked = (state == Qt::Checked);
    qDebug() << "Scanner 2 overwrite changed:" << checked;

    // Update global state for UR20 scanner override
    if (m_state) {
        m_state->setScannerOverride(1, checked);
    }
}

void MainWindow::onScanner3OverwriteChanged(int state)
{
    bool checked = (state == Qt::Checked);
    qDebug() << "Scanner 3 overwrite changed:" << checked;

    // Update global state for UR20 scanner override
    if (m_state) {
        m_state->setScannerOverride(2, checked);
    }
}

// === Settings Slots ===

void MainWindow::onSaveSettingsClicked()
{
    qDebug() << "Save settings clicked";

    if (!m_settings) {
        showMessage("Fehler: Einstellungen nicht verfuegbar");
        return;
    }

    // Save all settings to SettingsManager
    m_settings->setValue(config::Keys::INFO_UR_MODEL, ui->comboBoxChooseURModel->currentText());
    m_settings->setValue(config::Keys::INFO_UR_SERIAL_NUMBER, ui->lineEditURSerialNo->text());
    m_settings->setValue(config::Keys::INFO_UR_MANUFACTURING_DATE, ui->lineEditURManufacturingDate->text());
    m_settings->setValue(config::Keys::INFO_UR_SOFTWARE_VERSION, ui->lineEditURSoftwareVer->text());
    m_settings->setValue(config::Keys::INFO_PALLETTIERER_NAME, ui->lineEditURName->text());
    m_settings->setValue(config::Keys::INFO_PALLETTIERER_STANDORT, ui->lineEditURStandort->text());

    // Save admin settings
    m_settings->setValue(config::Keys::SERVER_USB_PATH, ui->pathEdit->text());
    m_settings->setValue(config::Keys::ADMIN_ALARM_SOUND_FILE, ui->audioPathEdit->text());
    m_settings->setValue(config::Keys::ADMIN_SCANNER_WARNING_SOUND_FILE,
                         ui->scannerWarningSoundPathEdit->text());

    // Update password if changed (not empty)
    QString newPassword = ui->passwordEdit->text();
    if (!newPassword.isEmpty()) {
        m_settings->setAdminPassword(newPassword);
        ui->passwordEdit->clear();
    }

    // Save scanner overrides
    QVector<bool> scannerOverrides = {
        ui->checkBoxScanner1Overwrite->isChecked(),
        ui->checkBoxScanner2Overwrite->isChecked(),
        ui->checkBoxScanner3Overwrite->isChecked()
    };
    if (m_state) {
        m_state->setScannerOverride(scannerOverrides);
    }

    if (m_settings->save()) {
        showMessage("Einstellungen gespeichert");
    } else {
        showMessage("Fehler beim Speichern der Einstellungen");
    }

    if (m_audio) {
        m_audio->setCustomFile(audio::AudioType::Alarm, ui->audioPathEdit->text());
        m_audio->setCustomFile(audio::AudioType::ScannerWarning,
                               ui->scannerWarningSoundPathEdit->text());
    }
}

void MainWindow::onUrModelChanged(int index)
{
    Q_UNUSED(index);
    QString model = ui->comboBoxChooseURModel->currentText();
    qDebug() << "UR Model changed:" << model;

    // Update settings immediately
    if (m_settings) {
        m_settings->setValue(config::Keys::INFO_UR_MODEL, model);
    }
}

void MainWindow::onExitAppClicked()
{
    if (QMessageBox::question(this, "Bestaetigung",
            "Anwendung wirklich neustarten?") == QMessageBox::Yes) {
        QApplication::quit();
    }
}

void MainWindow::onSearchUpdateClicked()
{
    qDebug() << "Search update clicked";

    if (!m_autoUpdater) {
        showMessage("Update-System nicht verfuegbar");
        return;
    }

    if (m_autoUpdater->currentStatus() == system::UpdateStatus::Checking) {
        showMessage("Update-Pruefung laeuft bereits...");
        return;
    }

    showMessage("Pruefe auf Updates...");
    m_autoUpdater->checkForUpdates();
}

void MainWindow::onUpdateCheckCompleted(bool success, const QString& message)
{
    if (!success) {
        showMessage("Update-Pruefung fehlgeschlagen: " + message);
        return;
    }

    if (!m_autoUpdater) {
        return;
    }

    auto updateInfo = m_autoUpdater->availableUpdate();
    if (updateInfo.version.isEmpty()) {
        showMessage("Keine Updates verfuegbar. Aktuelle Version: " + m_autoUpdater->currentVersion());
    } else {
        QString msg = QString("Neue Version verfuegbar: %1\n\nAktuelle Version: %2\n\nRelease Notes:\n%3")
                          .arg(updateInfo.version)
                          .arg(m_autoUpdater->currentVersion())
                          .arg(updateInfo.releaseNotes);

        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            "Update Verfuegbar",
            msg,
            QMessageBox::Yes | QMessageBox::No
        );

        if (reply == QMessageBox::Yes) {
            showMessage("Lade Update herunter...");
            m_autoUpdater->downloadAndInstallUpdate();
        }
    }
}

void MainWindow::onUpdateDownloadProgress(const system::UpdateProgress& progress)
{
    if (progress.bytesTotal > 0) {
        qint64 percent = (progress.bytesReceived * 100) / progress.bytesTotal;
        showMessage(QString("Download: %1%").arg(percent));
    }
}

void MainWindow::onUpdateDownloadCompleted(const QString& fileName)
{
    qDebug() << "Update downloaded:" << fileName;
    showMessage("Update heruntergeladen. Installation wird gestartet...");
}

void MainWindow::onUpdateInstallationCompleted(bool success, const QString& message)
{
    if (success) {
        showMessage("Update erfolgreich installiert! Anwendung wird neu gestartet.");
        QTimer::singleShot(2000, qApp, &QApplication::quit);
    } else {
        showMessage("Update-Installation fehlgeschlagen: " + message);
    }
}

void MainWindow::onUpdateFailed(const QString& error)
{
    showMessage("Update fehlgeschlagen: " + error);
}

void MainWindow::onSendCommandClicked()
{
    QString command = ui->comboBoxCommandRemoteControl->currentText();
    qDebug() << "Send command:" << command;

    if (m_robot && m_robot->isConnected()) {
        QString response = m_robot->sendCommand(command);
        appendConsoleLog(">> " + command);
        appendConsoleLog("<< " + response);
    } else {
        showMessage("Roboter nicht verbunden");
    }
}

void MainWindow::onSelectRobPathClicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Pfad Palettenplaene waehlen");
    if (!dir.isEmpty()) {
        ui->pathEdit->setText(dir);
    }
}

void MainWindow::onSelectAudioPathClicked()
{
    QString file = QFileDialog::getOpenFileName(this, "Audio-Datei waehlen",
        QString(), "Audio Files (*.wav *.mp3 *.ogg)");
    if (!file.isEmpty()) {
        ui->audioPathEdit->setText(file);
        if (m_audio) {
            m_audio->setCustomFile(audio::AudioType::Alarm, file);
        }
    }
}

void MainWindow::onSelectScannerSoundPathClicked()
{
    QString file = QFileDialog::getOpenFileName(this, "Scanner Warning Sound waehlen",
        QString(), "Audio Files (*.wav *.mp3 *.ogg)");
    if (!file.isEmpty()) {
        ui->scannerWarningSoundPathEdit->setText(file);
        if (m_audio) {
            m_audio->setCustomFile(audio::AudioType::ScannerWarning, file);
        }
    }
}

void MainWindow::onTestAlarmAudioClicked()
{
    if (!m_audio) {
        showMessage("Audio nicht verfuegbar");
        return;
    }

    m_audio->playNotification(audio::AudioType::Alarm);
}

void MainWindow::onTestScannerAudioClicked()
{
    if (!m_audio) {
        showMessage("Audio nicht verfuegbar");
        return;
    }

    m_audio->playNotification(audio::AudioType::ScannerWarning);
}

void MainWindow::onImportRobFileClicked()
{
    if (!m_database) {
        showMessage("Datenbank nicht verfuegbar");
        return;
    }

    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr(".rob Datei auswaehlen"),
        QString(),
        tr("ROB Files (*.rob)")
    );

    if (filePath.isEmpty()) {
        return;
    }

    ::multipack::RobFileParser parser;
    auto data = parser.parseFileFullPath(filePath);
    if (!data.isValid) {
        showMessage(QString("Import fehlgeschlagen: %1").arg(data.errorMessage));
        return;
    }

    database::PaletteData paletteData;
    paletteData.metadata.fileName = QFileInfo(filePath).fileName();
    paletteData.metadata.fileTimestamp = data.fileTimestamp.toMSecsSinceEpoch();
    paletteData.metadata.paketQuer = data.g_paket_quer;
    paletteData.metadata.centerOfGravity = data.g_CenterOfGravity;
    paletteData.metadata.lageArten = data.g_LageArten;
    paletteData.metadata.anzLagen = data.g_AnzLagen;
    paletteData.metadata.anzahlPakete = data.g_AnzahlPakete;

    if (data.g_PalettenDim.size() >= 3) {
        paletteData.paletteDimensions.length = data.g_PalettenDim[0];
        paletteData.paletteDimensions.width = data.g_PalettenDim[1];
        paletteData.paletteDimensions.height = data.g_PalettenDim[2];
    }

    if (data.g_PaketDim.size() >= 4) {
        paletteData.packageDimensions.length = data.g_PaketDim[0];
        paletteData.packageDimensions.width = data.g_PaketDim[1];
        paletteData.packageDimensions.height = data.g_PaketDim[2];
        paletteData.packageDimensions.gap = data.g_PaketDim[3];
    }

    paletteData.rawData = data.g_Daten;
    paletteData.layerAssignments = data.g_LageZuordnung;
    paletteData.intermediaryLayers = data.g_Zwischenlagen;
    paletteData.packagesPerLayerType = data.g_PaketeZuordnung;

    for (const auto& pos : data.g_PaketPos) {
        if (pos.size() < 9) {
            continue;
        }
        database::PackagePosition position;
        position.xp = pos[0];
        position.yp = pos[1];
        position.ap = pos[2];
        position.xd = pos[3];
        position.yd = pos[4];
        position.ad = pos[5];
        position.nop = pos[6];
        position.xvec = pos[7];
        position.yvec = pos[8];
        paletteData.packagePositions.append(position);
    }

    if (!m_database->savePaletteData(paletteData)) {
        showMessage("Import fehlgeschlagen: Datenbankfehler");
        return;
    }

    showMessage(".rob Datei importiert");
    loadRobFileList();
    updateVisualizationFromPaletteData(paletteData);
}

void MainWindow::updateVisualizationFromPaletteData(const database::PaletteData& data)
{
    if (!m_visualizationWidget) {
        return;
    }

    auto calculatePackageCenters = [](double centerX, double centerY, int rotation, int count, int spacing) {
        QVector<QPair<double, double>> centers;
        if (count <= 1) {
            centers.append({centerX, centerY});
            return centers;
        }

        for (int i = 0; i < count; ++i) {
            double offset = (i - (count - 1) / 2.0) * spacing;
            double x = centerX;
            double y = centerY;
            switch (rotation) {
                case 0:
                    x = centerX + offset;
                    break;
                case 90:
                    y = centerY + offset;
                    break;
                case 180:
                    x = centerX - offset;
                    break;
                case 270:
                    y = centerY - offset;
                    break;
                default:
                    x = centerX + offset;
                    break;
            }
            centers.append({x, y});
        }
        return centers;
    };

    ui::VisualPalette palette;
    palette.name = data.metadata.fileName;

    const auto& lines = data.rawData;
    bool hasRawData = lines.size() >= 4 && lines[0].size() >= 2 && lines[1].size() >= 4
                      && !lines[2].isEmpty() && !lines[3].isEmpty();

    if (hasRawData) {
        int palletLength = lines[0][0];
        int palletWidth = lines[0][1];
        int packageWidth = lines[1][0];
        int packageLength = lines[1][1];
        int packageHeight = lines[1][2];
        int direction = lines[1][3];
        if (direction == 1) {
            std::swap(packageWidth, packageLength);
        }

        int numUniqueLayers = lines[2][0];
        int numLayers = lines[3][0];

        palette.length = palletLength;
        palette.width = palletWidth;
        palette.height = data.paletteDimensions.height;
        palette.packageWidth = packageWidth;
        palette.packageLength = packageLength;
        palette.packageHeight = packageHeight;
        palette.totalLayers = numLayers;
        palette.conveyorDirection = direction;

        int currentLine = 5;
        QVector<int> layerOrder;
        for (int i = 0; i < numLayers && currentLine < lines.size(); ++i, ++currentLine) {
            if (!lines[currentLine].isEmpty()) {
                layerOrder.append(lines[currentLine][0]);
            }
        }

        QVector<QVector<ui::VisualPackage>> uniqueLayers;
        uniqueLayers.resize(numUniqueLayers);

        for (int layerId = 0; layerId < numUniqueLayers && currentLine < lines.size(); ++layerId) {
            if (lines[currentLine].isEmpty()) {
                break;
            }

            int numCoordinates = lines[currentLine][0];
            ++currentLine;

            QVector<ui::VisualPackage> layerPackages;
            for (int coord = 0; coord < numCoordinates && currentLine < lines.size(); ++coord, ++currentLine) {
                if (lines[currentLine].size() < 9) {
                    continue;
                }

                int x = lines[currentLine][3];
                int y = lines[currentLine][4];
                int rotation = lines[currentLine][5];
                int numPackages = lines[currentLine][6];

                auto centers = calculatePackageCenters(x, y, rotation, numPackages, packageWidth);
                for (const auto& center : centers) {
                    ui::VisualPackage pkg;
                    pkg.x = center.first;
                    pkg.y = center.second;
                    pkg.width = packageWidth;
                    pkg.length = packageLength;
                    pkg.height = packageHeight;
                    pkg.rotation = rotation;
                    layerPackages.append(pkg);
                }
            }

            uniqueLayers[layerId] = layerPackages;
        }

        for (int layerIndex = 0; layerIndex < layerOrder.size(); ++layerIndex) {
            int layerId = layerOrder[layerIndex] - 1;
            if (layerId < 0 || layerId >= uniqueLayers.size()) {
                continue;
            }
            for (auto pkg : uniqueLayers[layerId]) {
                pkg.layer = layerIndex;
                palette.packages.append(pkg);
                palette.totalPackages++;
            }
        }
    } else {
        palette.length = data.paletteDimensions.length;
        palette.width = data.paletteDimensions.width;
        palette.height = data.paletteDimensions.height;
        palette.packageWidth = data.packageDimensions.width;
        palette.packageLength = data.packageDimensions.length;
        palette.packageHeight = data.packageDimensions.height;
        palette.totalLayers = data.metadata.anzLagen;
        palette.conveyorDirection = data.metadata.paketQuer;

        for (int layerNum = 0; layerNum < data.metadata.anzLagen; ++layerNum) {
            int layerType = 0;
            if (layerNum < data.layerAssignments.size()) {
                layerType = data.layerAssignments[layerNum];
            }

            int layerTypeIndex = layerType;
            if (layerTypeIndex > 0 && layerTypeIndex <= data.packagesPerLayerType.size()) {
                layerTypeIndex -= 1;
            }

            int typeStartIndex = 0;
            for (int t = 0; t < layerTypeIndex; ++t) {
                if (t < data.packagesPerLayerType.size()) {
                    typeStartIndex += data.packagesPerLayerType[t];
                }
            }

            int packagesInType = 1;
            if (layerTypeIndex >= 0 && layerTypeIndex < data.packagesPerLayerType.size()) {
                packagesInType = data.packagesPerLayerType[layerTypeIndex];
            }

            for (int pkgIdx = 0; pkgIdx < packagesInType; ++pkgIdx) {
                int posIdx = typeStartIndex + pkgIdx;
                if (posIdx >= data.packagePositions.size()) {
                    continue;
                }

                const auto& pos = data.packagePositions[posIdx];
                int rotation = pos.ad;
                int count = pos.nop > 0 ? pos.nop : 1;
                auto centers = calculatePackageCenters(pos.xd, pos.yd, rotation, count, palette.packageWidth);
                for (const auto& center : centers) {
                    ui::VisualPackage pkg;
                    pkg.x = center.first;
                    pkg.y = center.second;
                    pkg.width = palette.packageWidth;
                    pkg.length = palette.packageLength;
                    pkg.height = palette.packageHeight;
                    pkg.rotation = rotation;
                    pkg.layer = layerNum;
                    palette.packages.append(pkg);
                    palette.totalPackages++;
                }
            }
        }
    }

    m_visualizationWidget->setVisualizationData(palette);
}

void MainWindow::onOpenFileClicked()
{
    QString path = ui->lineEditFilePath->text();
    if (path.isEmpty()) {
        path = QFileDialog::getOpenFileName(this, "Datei oeffnen");
        if (path.isEmpty()) return;
        ui->lineEditFilePath->setText(path);
    }

    QFile file(path);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        ui->textEditFile->setText(QString::fromUtf8(file.readAll()));
        file.close();
    } else {
        showMessage("Datei konnte nicht geoeffnet werden");
    }
}

void MainWindow::onConsoleCommandEntered()
{
    QString command = ui->lineEditCommand->text().trimmed();
    if (command.isEmpty()) return;

    appendConsoleLog("$ " + command);
    ui->lineEditCommand->clear();

    // Process console command
    if (command == "clear") {
        ui->textEditConsole->clear();
    } else if (command == "help") {
        appendConsoleLog("Available commands: clear, help, status, version");
    } else if (command == "status") {
        appendConsoleLog("Server: " + QString(m_serverRunning ? "running" : "stopped"));
        appendConsoleLog("Palette: " + QString(m_paletteLoaded ? m_currentPaletteFile : "none"));
    } else if (command == "version") {
        appendConsoleLog(QString("MultipackParser C++ v%1").arg(config::Defaults::VERSION));
    } else {
        appendConsoleLog("Unknown command: " + command);
    }
}

// === Experimental Slots ===

void MainWindow::onRobFileSelected(QListWidgetItem* item)
{
    if (!item) return;

    QString fileName = item->text();
    qDebug() << "Rob file selected:" << fileName;

    // Immediately load and visualize the selected file
    if (m_database) {
        auto data = m_database->loadPaletteData(fileName);
        if (data.has_value()) {
            updateVisualizationFromPaletteData(*data);
            qDebug() << "Visualization updated for:" << fileName;
        }
    }
}

void MainWindow::onDeselectRobFile()
{
    ui->robFilesListWidget->clearSelection();
}

void MainWindow::onFilterChanged()
{
    if (!m_database) return;

    int length = ui->lineEditFilterLength->text().toInt();
    int width = ui->lineEditFilterWidth->text().toInt();
    int height = ui->lineEditFilterHeight->text().toInt();

    if (length == 0 && width == 0 && height == 0) {
        loadRobFileList();
        return;
    }

    auto matching = m_database->findByPackageDimensions(length, width, height);

    ui->robFilesListWidget->clear();
    for (const auto& name : matching) {
        ui->robFilesListWidget->addItem(name);
    }
}

void MainWindow::onClearFiltersClicked()
{
    ui->lineEditFilterLength->clear();
    ui->lineEditFilterWidth->clear();
    ui->lineEditFilterHeight->clear();
    loadRobFileList();
}

void MainWindow::onLoadSelectedRobFile()
{
    auto items = ui->robFilesListWidget->selectedItems();
    if (items.isEmpty()) {
        showMessage("Bitte Datei auswaehlen");
        return;
    }

    ui->EingabePallettenplan->setText(items.first()->text());
    showMainMenu();
    onLoadPaletteClicked();
}

// === Public Slots ===

void MainWindow::updateRobotStatus()
{
    if (!m_robot) return;

    bool connected = m_robot->isConnected();

    // Update UI based on robot status
    if (connected) {
        ui->lineEditURSerialNo->setText(m_robot->getSerialNumber());
        ui->lineEditURSoftwareVer->setText(m_robot->getSoftwareVersion());
    }
}

void MainWindow::updatePaletteInfo()
{
    // Called when palette data changes
    loadRobFileList();
}

void MainWindow::showMessage(const QString& message)
{
    message::StatusManager::instance().showTemporaryStatus(message, message::StatusType::Normal, 3000);
}

void MainWindow::onStatusChanged(const QString& message, message::StatusType type)
{
    QString color;
    switch (type) {
        case message::StatusType::Busy:
            color = "#1f5fbf";
            break;
        case message::StatusType::Success:
            color = "#2f8f3f";
            break;
        case message::StatusType::Warning:
            color = "#d9822b";
            break;
        case message::StatusType::Error:
            color = "#c23030";
            break;
        default:
            color = "#222222";
            break;
    }

    ui->label_GewichtInfo->setText(message);
    ui->label_GewichtInfo->setStyleSheet(QString("color: %1;").arg(color));
}

void MainWindow::setupPalettePlanCompleter()
{
    qDebug() << "Setting up palette plan auto-completion";
    
    if (!ui->lineEditFilePath) {
        return;
    }
    
    // Create completer for .rob files
    QStringList wordList;
    
    // Load initial wordlist from current USB path
    wordList = loadPalettePlanWordlist();
    
    QCompleter* completer = new QCompleter(wordList, this);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchStartsWith);
    
    ui->lineEditFilePath->setCompleter(completer);
    
    qDebug() << "Palette plan completer set up with" << wordList.size() << "entries";
}

void MainWindow::appendConsoleLog(const QString& text)
{
    if (ui && ui->textEditConsole) {
        ui->textEditConsole->append(text);
    }
}

QStringList MainWindow::loadPalettePlanWordlist()
{
    QString usbPath = m_settings ? m_settings->usbPath() : "../";
    QDir usbDir(usbPath);
    
    if (!usbDir.exists()) {
        qWarning() << "USB directory does not exist:" << usbPath;
        return QStringList();
    }
    
    // Get all .rob files
    QStringList nameFilter;
    nameFilter << "*.rob";
    QFileInfoList robFiles = usbDir.entryInfoList(nameFilter, QDir::Files, QDir::Name);
    
    // Convert to wordlist (remove .rob extension)
    QStringList wordList;
    wordList.clear();
    for (const QFileInfo& fileInfo : robFiles) {
        wordList.append(fileInfo.completeBaseName());
    }
    
    qDebug() << "Loaded" << wordList.size() << "palette plan files from USB";
    return wordList;
}
} // namespace ui
} // namespace multipack
