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
#include "multipack/system/UsbMonitor.h"
#include "multipack/ui/PasswordDialog.h"
#include "multipack/ui/InputValidation.h"
#include "multipack/ui/PaletteConfigDialog.h"
#include "multipack/ui/NotificationPopup.h"
#include "multipack/ui/OnScreenKeyboard.h"
#include "multipack/ui/Theme.h"
#include "multipack/message/StatusManager.h"
#include "multipack/network/XmlRpcServer.h"
#include "multipack/robot/RobotStatusMonitor.h"
#include "multipack/system/RobFileParser.h"
#include "multipack/ui/VisualizationWidget.h"
#include "multipack/system/AutoUpdater.h"

#include <QApplication>
#include <QScreen>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QCompleter>
#include <QAbstractItemView>
#include <QDebug>
#include <QFileInfo>
#include <QIcon>
#include <QPixmap>
#include <QCloseEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QShortcut>
#include <QPushButton>
#include <QTimer>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QHeaderView>
#include <QInputDialog>
#include <QListView>
#include <QListWidget>
#include <QProcess>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStringListModel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QThread>

namespace multipack {
namespace ui {

namespace {
QString palettePlanDisplayName(const QString& fileName)
{
    QString displayName = QFileInfo(fileName).fileName().trimmed();
    if (displayName.endsWith(QStringLiteral(".rob"), Qt::CaseInsensitive)) {
        displayName.chop(4);
    }
    return displayName;
}

} // namespace

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
    Theme::applyTo(this);
    applyResourceIcons();

    auto* closeShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::ALT | Qt::Key_C), this);
    closeShortcut->setContext(Qt::ApplicationShortcut);
    connect(closeShortcut, &QShortcut::activated, qApp, &QApplication::quit);
    
    // Set up auto-completion for palette plan files
    setupPalettePlanCompleter();
    
    // Setup connections
    setupConnections();
    setupNavigation();
    setupStatusTab();
    setupDatabaseManagerTab();
    setupDimensionHandlers();
    setupPlannerPanel();
    setupPlannerSettingsTab();

    if (ui->planPreviewFrame) {
        auto* layout = new QVBoxLayout(ui->planPreviewFrame);
        layout->setContentsMargins(6, 6, 6, 6);
        m_visualizationWidget = new VisualizationWidget(ui->planPreviewFrame);
        layout->addWidget(m_visualizationWidget);
    }

    auto& statusManager = message::StatusManager::instance();
    connect(&statusManager, &message::StatusManager::statusChanged,
            this, &MainWindow::onStatusChanged);
    connect(&statusManager, &message::StatusManager::statusCleared,
            this, [this]() { ui->label_GewichtInfo->clear(); });

    // Start on the operating page
    setCurrentPage(PAGE_MAIN_MENU);
    if (ui->EingabePallettenplan) {
        ui->EingabePallettenplan->setInputMethodHints(
            Qt::ImhFormattedNumbersOnly | Qt::ImhNoPredictiveText);
    }
    if (ui->lineEditCommand) {
        ui->lineEditCommand->setText("> ");
        ui->lineEditCommand->setPlaceholderText("command");
    }
    updateEnabledStates();

    // Set window properties
    setWindowTitle("Palletierer");
    setFixedSize(1280, 720);

    // Embedded on-screen keyboard. Parented to the central widget so it can
    // overlay the bottom of the UI; raised above siblings on demand. Avoids
    // QDockWidget (no room under setFixedSize) and Qt::Tool top-level (z-order
    // issues under non-compositing X11 kiosk).
    if (QApplication::platformName() != QStringLiteral("offscreen")
        && qEnvironmentVariable("MULTIPACK_VIRTUAL_KEYBOARD") != QStringLiteral("0")) {
        m_onScreenKeyboard = new OnScreenKeyboard(m_centralWidget);
        if (!m_onScreenKeyboard->isReady()) {
            qWarning() << "MainWindow - on-screen keyboard failed to initialize; disabling";
            delete m_onScreenKeyboard;
            m_onScreenKeyboard = nullptr;
        } else {
            qInfo() << "MainWindow - on-screen keyboard ready";
        }
    } else {
        qInfo() << "MainWindow - virtual keyboard disabled (MULTIPACK_VIRTUAL_KEYBOARD="
                << qEnvironmentVariable("MULTIPACK_VIRTUAL_KEYBOARD") << ", platform="
                << QApplication::platformName() << ")";
    }

    qDebug() << "MainWindow - initialized";
}

MainWindow::~MainWindow()
{
    stopRobotStatusMonitor();
    delete ui;
    qDebug() << "MainWindow - destroyed";
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    qDebug() << "MainWindow - close event received";
    stopRobotStatusMonitor();

    // Accept the close event
    event->accept();
}

void MainWindow::setSettingsManager(config::SettingsManager* settings)
{
    m_settings = settings;
    if (m_usbKeyCheck) {
        m_usbKeyCheck->setSettingsManager(settings);
    }
    loadSettings();
    maybeStartUr20Ui();
    maybeStartRobotStatusMonitor();
}

void MainWindow::setDatabaseManager(database::DatabaseManager* database)
{
    if (m_database) {
        disconnect(m_database, nullptr, this, nullptr);
    }

    m_database = database;
    if (m_database) {
        connect(m_database, &database::DatabaseManager::dataChanged,
                this, &MainWindow::loadRobFileList);
    }
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
        // The robot can also load a plan via XML-RPC (worker thread); debounce
        // so clear() + apply collapse into one status report.
        connect(m_state, &core::GlobalState::fileChanged,
                this, [this](const QString&) {
                    if (m_activePlanDebounce) {
                        m_activePlanDebounce->start();
                    }
                }, Qt::QueuedConnection);
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

void MainWindow::setUsbMonitor(system::UsbMonitor* monitor)
{
    if (m_usbMonitor == monitor) {
        return;
    }

    if (m_usbMonitor) {
        disconnect(m_usbMonitor, nullptr, this, nullptr);
    }

    m_usbMonitor = monitor;
    if (!m_usbMonitor) {
        return;
    }

    connect(m_usbMonitor, &system::UsbMonitor::databaseUpdateCompleted,
            this, [this](const QStringList& filesUpdated) {
                qInfo() << "MainWindow - palette plan folder update completed:" << filesUpdated;
                loadRobFileList();
            });
    connect(m_usbMonitor, &system::UsbMonitor::newFilesDetected,
            this, [](const QStringList& files) {
                qInfo() << "MainWindow - new palette plan files detected:" << files;
            });
    connect(m_usbMonitor, &system::UsbMonitor::filesModified,
            this, [](const QStringList& files) {
                qInfo() << "MainWindow - modified palette plan files detected:" << files;
            });
    connect(m_usbMonitor, &system::UsbMonitor::databaseUpdateFailed,
            this, [](const QString& error) {
                qWarning() << "MainWindow - palette plan folder update failed:" << error;
            });

    QTimer::singleShot(0, m_usbMonitor, &system::UsbMonitor::updateDatabaseFromUsbAsync);
}

void MainWindow::setXmlRpcServer(network::XmlRpcServer* server)
{
    if (m_xmlRpcServer == server) {
        return;
    }

    if (m_xmlRpcServer) {
        disconnect(m_xmlRpcServer, nullptr, this, nullptr);
    }

    m_xmlRpcServer = server;

    if (m_xmlRpcServer) {
        connect(m_xmlRpcServer, &network::XmlRpcServer::started,
                this, [this]() { setServerRunning(true); });
        connect(m_xmlRpcServer, &network::XmlRpcServer::stopped,
                this, [this]() { setServerRunning(false); });
        connect(m_xmlRpcServer, &network::XmlRpcServer::error,
                this, [this](const QString& message) { setServerRunning(false, message); });
        connect(m_xmlRpcServer, &network::XmlRpcServer::paletteDataLoaded,
                this, [this](const QString& fileName) {
                    qInfo() << "MainWindow - robot loaded palette plan:" << fileName;
                    incrementUseCycleCount();
                });
        setServerRunning(m_xmlRpcServer->isRunning());
    } else {
        setServerRunning(false);
    }
}

void MainWindow::applyResourceIcons()
{
    auto iconFromResource = [](const char* path) -> QIcon {
        const QIcon icon(QString::fromUtf8(path));
        if (icon.isNull()) {
            qWarning() << "MainWindow - missing resource icon:" << path;
        }
        return icon;
    };

    if (ui->ButtonSettings) {
        ui->ButtonSettings->setIcon(iconFromResource(":/icons/ui/gear.png"));
    }

    updateVolumeIcon();
}

void MainWindow::setupConnections()
{
    // Header and main page
    connect(ui->pushButtonVolumeOnOff, &QPushButton::clicked, this, &MainWindow::onVolumeToggleClicked);
    connect(ui->ButtonOpenParameterRoboter, &QPushButton::clicked, this, &MainWindow::onParameterRobotClicked);

    // Main menu actions
    connect(ui->LadePallettenplan, &QPushButton::clicked, this, &MainWindow::onLoadPaletteClicked);
    connect(ui->ButtonDatenSenden, &QPushButton::clicked, this, &MainWindow::onStartServerClicked);
    connect(ui->EingabePallettenplan, &QLineEdit::returnPressed, this, &MainWindow::onLoadPaletteClicked);
    connect(ui->checkBoxEinzelpaket, QOverload<int>::of(&QCheckBox::stateChanged), this, &MainWindow::onEinzelpaketChanged);
    connect(ui->checkBoxLabelInvert, QOverload<int>::of(&QCheckBox::stateChanged), this, &MainWindow::onLabelInvertChanged);
    connect(ui->EingabeStartlage, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onStartlageChanged);

    // Robot control buttons
    connect(ui->ButtonRoboterStart, &QPushButton::clicked, this, &MainWindow::onRobotStartClicked);
    connect(ui->ButtonRoboterStop, &QPushButton::clicked, this, &MainWindow::onRobotStopClicked);
    connect(ui->ButtonRoboterPause, &QPushButton::clicked, this, &MainWindow::onRobotPauseClicked);
    connect(ui->ButtonStopRPCServer, &QPushButton::clicked, this, &MainWindow::onStopRpcServerClicked);

    // Aufnahme tab
    connect(ui->EingabeVerschiebungX, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onVerschiebungXChanged);
    connect(ui->EingabeVerschiebungY, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onVerschiebungYChanged);
    connect(ui->checkBoxKlemmung, QOverload<int>::of(&QCheckBox::stateChanged), this, &MainWindow::onKlemmungChanged);
    connect(ui->ButtonDatenSenden_2, &QPushButton::clicked, this, &MainWindow::onAufnahmeServerStart);

    // Settings actions
    connect(ui->pushButtonSpeichern, &QPushButton::clicked, this, &MainWindow::onSaveSettingsClicked);
    connect(ui->pushButtonSpeichern_2, &QPushButton::clicked, this, &MainWindow::onSaveSettingsClicked);
    connect(ui->pushButtonSpeichern_3, &QPushButton::clicked, this, &MainWindow::onSaveOpenFileClicked);
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
    connect(ui->checkBoxScanner1Overwrite, QOverload<int>::of(&QCheckBox::stateChanged), this, &MainWindow::onScanner1OverwriteChanged);
    connect(ui->checkBoxScanner2Overwrite, QOverload<int>::of(&QCheckBox::stateChanged), this, &MainWindow::onScanner2OverwriteChanged);
    connect(ui->checkBoxScanner3Overwrite, QOverload<int>::of(&QCheckBox::stateChanged), this, &MainWindow::onScanner3OverwriteChanged);

    // UR20: release a cleared palette (notice bar, visible on every page)
    ui->palette1ClearButton->setToolTip(QStringLiteral("Palette 1 zum Palettieren freigeben"));
    ui->palette2ClearButton->setToolTip(QStringLiteral("Palette 2 zum Palettieren freigeben"));
    connect(ui->palette1ClearButton, &QPushButton::clicked, this, [this]() { onPaletteClearClicked(1); });
    connect(ui->palette2ClearButton, &QPushButton::clicked, this, [this]() { onPaletteClearClicked(2); });

    // Plan library ("Pläne")
    connect(ui->deselectRobFile, &QPushButton::clicked, this, &MainWindow::onDeselectRobFile);
    connect(ui->pushButtonClearFilters, &QPushButton::clicked, this, &MainWindow::onClearFiltersClicked);
    connect(ui->lineEditFilterLength, &QLineEdit::textChanged, this, &MainWindow::onFilterChanged);
    connect(ui->lineEditFilterWidth, &QLineEdit::textChanged, this, &MainWindow::onFilterChanged);
    connect(ui->lineEditFilterHeight, &QLineEdit::textChanged, this, &MainWindow::onFilterChanged);
}

void MainWindow::setupNavigation()
{
    // Not every site uses these: the QR tab needs a configured planner, the
    // notice bar a UR20 with a palette waiting for release.
    ui->navQrCode->hide();
    ui->noticeBar->hide();
    ui->palette1ClearButton->hide();
    ui->palette2ClearButton->hide();

    connect(ui->navBetrieb, &QPushButton::clicked, this, &MainWindow::showMainMenu);
    connect(ui->navPlaene, &QPushButton::clicked, this, &MainWindow::showPlans);
    connect(ui->navRoboter, &QPushButton::clicked, this, &MainWindow::showRobotParameters);
    connect(ui->navQrCode, &QPushButton::clicked, this, &MainWindow::showQrCode);
    connect(ui->ButtonSettings, &QPushButton::clicked, this, &MainWindow::onSettingsClicked);
}

void MainWindow::setCurrentPage(PageIndex page)
{
    hidePalettePlanCompletionPopup();
    ui->stackedWidget->setCurrentIndex(page);
    syncNavigation();
}

void MainWindow::syncNavigation()
{
    // A click toggles a checkable button before the slot runs; the page decides.
    const int page = ui->stackedWidget->currentIndex();
    ui->navBetrieb->setChecked(page == PAGE_MAIN_MENU);
    ui->navPlaene->setChecked(page == PAGE_PLANS);
    ui->navRoboter->setChecked(page == PAGE_ROBOT_PARAMS);
    ui->navQrCode->setChecked(page == PAGE_QR_CODE);
    ui->ButtonSettings->setChecked(page == PAGE_SETTINGS);
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
    ui->lineEditNumberCycles->setText(QString::number(m_settings->numberOfUseCycles()));

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

    const QString robotIp = m_settings->robotIp();
    if (m_state) {
        m_state->setRobotIp(robotIp);
    }

    if (m_statusMonitor) {
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
        // The panel's own pixels; the UI may be scaled to them (DisplayScale).
        const QSize pixels = (QSizeF(screen->size()) * screen->devicePixelRatio()).toSize();
        ui->lineEditDisplayWidth->setText(QString::number(pixels.width()));
        ui->lineEditDisplayHeight->setText(QString::number(pixels.height()));
        ui->lineEditDisplayRefreshRate->setText(QString::number(screen->refreshRate()) + " Hz");
    }

    // Update volume icon
    updateVolumeIcon();

    qDebug() << "MainWindow - settings loaded";
}

bool MainWindow::ensureRobotConnected()
{
    if (!m_robot || !m_settings) {
        showMessage("Roboter nicht verfügbar");
        return false;
    }

    if (m_robot->isConnected()) {
        return true;
    }

    const QString robotIp = m_settings->robotIp();
    if (robotIp.isEmpty()) {
        showMessage("Keine Roboter-IP konfiguriert");
        return false;
    }

    if (m_state) {
        m_state->setRobotIp(robotIp);
    }

    if (!m_robot->connect(robotIp)) {
        showMessage("Verbindung zum Roboter fehlgeschlagen");
        return false;
    }

    return true;
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

    if (qEnvironmentVariableIsSet("MULTIPACK_DISABLE_ROBOT_STATUS_MONITOR")) {
        qDebug() << "RobotStatusMonitor disabled by environment";
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
                    m_statusConnection->setText(connected ? QStringLiteral("Verbunden")
                                                          : QStringLiteral("Getrennt"));
                    Theme::setTone(m_statusConnection,
                                   connected ? Theme::Tone::Success : Theme::Tone::Danger);
                }
                if (!connected) {
                    m_robotStatusConnected = false;
                    updateProposalUi();
                }
            });

    QString robotIp = m_settings->robotIp();
    m_statusThread->start();
    QMetaObject::invokeMethod(m_statusMonitor, [this, robotIp]() {
        m_statusMonitor->setRobotIp(robotIp);
        m_statusMonitor->start(robot::RobotStatusMonitor::DEFAULT_INTERVAL_MS);
    }, Qt::QueuedConnection);
}

void MainWindow::stopRobotStatusMonitor()
{
    robot::RobotStatusMonitor* statusMonitor = m_statusMonitor;
    QThread* statusThread = m_statusThread;

    if (!statusMonitor && !statusThread) {
        return;
    }

    m_statusMonitor = nullptr;
    m_statusThread = nullptr;
    m_robotStatusConnected = false;
    m_robotProgramState = robot::ProgramState::Unknown;

    if (statusMonitor) {
        if (statusMonitor->thread() == QThread::currentThread()) {
            statusMonitor->stop();
        } else {
            QMetaObject::invokeMethod(
                statusMonitor,
                [statusMonitor]() { statusMonitor->stop(); },
                Qt::BlockingQueuedConnection);
        }
    }

    if (statusThread) {
        statusThread->quit();
        if (!statusThread->wait(3000)) {
            qWarning() << "RobotStatusMonitor thread did not finish, terminating";
            statusThread->terminate();
            statusThread->wait(1000);
        }
        qDebug() << "RobotStatusMonitor thread stopped";
    }
}

void MainWindow::setupStatusTab()
{
    if (!ui->tabWidget) {
        return;
    }

    auto* statusWidget = new QWidget(ui->tabWidget);
    statusWidget->setObjectName(QStringLiteral("robotStatusTab"));
    auto* rootLayout = new QVBoxLayout(statusWidget);
    rootLayout->setContentsMargins(24, 24, 24, 24);

    auto* card = new QFrame(statusWidget);
    Theme::setCard(card);
    auto* grid = new QGridLayout(card);
    grid->setContentsMargins(24, 18, 24, 18);
    grid->setHorizontalSpacing(24);
    grid->setVerticalSpacing(14);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(3, 1);

    // Live state on the left, robot identity on the right.
    auto addRow = [card, grid](int row, int column, const QString& caption) {
        auto* captionLabel = new QLabel(caption, card);
        captionLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        Theme::setRole(captionLabel, "muted");
        auto* valueLabel = new QLabel(QStringLiteral("-"), card);
        Theme::setRole(valueLabel, "value");
        grid->addWidget(captionLabel, row, column);
        grid->addWidget(valueLabel, row, column + 1);
        return valueLabel;
    };

    m_statusConnection = addRow(0, 0, QStringLiteral("Verbindung"));
    m_statusRobotMode = addRow(1, 0, QStringLiteral("Robotermodus"));
    m_statusSafetyStatus = addRow(2, 0, QStringLiteral("Sicherheitsstatus"));
    m_statusProgramState = addRow(3, 0, QStringLiteral("Programmstatus"));
    m_statusLastUpdate = addRow(4, 0, QStringLiteral("Letzte Aktualisierung"));
    m_statusRobotIp = addRow(0, 2, QStringLiteral("Roboter-IP"));
    m_statusSerialNumber = addRow(1, 2, QStringLiteral("Seriennummer"));
    m_statusPolyscopeVersion = addRow(2, 2, QStringLiteral("Polyscope-Version"));
    m_statusLoadedProgram = addRow(3, 2, QStringLiteral("Geladenes Programm"));
    m_statusLoadedProgram->setWordWrap(true);

    rootLayout->addWidget(card);
    rootLayout->addStretch(1);

    ui->tabWidget->addTab(statusWidget, tr("Status"));
}

void MainWindow::updateStatusTab(const robot::RobotStatus& status)
{
    // Remember the dashboard state for the Planvorschlag switch guard.
    m_robotStatusConnected = status.isConnected;
    m_robotProgramState = status.programState;
    m_robotStatusTime = QDateTime::currentDateTimeUtc();
    updateProposalUi();

    if (!m_statusRobotIp) {
        return;
    }

    auto modeText = [](robot::RobotMode mode) {
        switch (mode) {
            case robot::RobotMode::Running: return QStringLiteral("Läuft");
            case robot::RobotMode::Idle: return QStringLiteral("Bereit");
            case robot::RobotMode::PowerOff: return QStringLiteral("Ausgeschaltet");
            case robot::RobotMode::BackDrive: return QStringLiteral("Backdrive");
            case robot::RobotMode::ConfirmSafety: return QStringLiteral("Sicherheit bestätigen");
            case robot::RobotMode::Booting: return QStringLiteral("Startet");
            case robot::RobotMode::NoController: return QStringLiteral("Kein Controller");
            case robot::RobotMode::Disconnected: return QStringLiteral("Getrennt");
            default: return QStringLiteral("Unbekannt");
        }
    };

    auto safetyText = [](robot::SafetyStatus safety) {
        switch (safety) {
            case robot::SafetyStatus::Normal: return QStringLiteral("Normal");
            case robot::SafetyStatus::ReducedMode: return QStringLiteral("Reduzierter Modus");
            case robot::SafetyStatus::ProtectiveStop: return QStringLiteral("Schutzstopp");
            case robot::SafetyStatus::Recovery: return QStringLiteral("Wiederherstellung");
            case robot::SafetyStatus::SafeguardStop: return QStringLiteral("Sicherheitsstopp");
            case robot::SafetyStatus::SystemEmergencyStop: return QStringLiteral("Not-Halt (Anlage)");
            case robot::SafetyStatus::RobotEmergencyStop: return QStringLiteral("Not-Halt (Roboter)");
            case robot::SafetyStatus::Violation: return QStringLiteral("Sicherheitsverletzung");
            case robot::SafetyStatus::Fault: return QStringLiteral("Fehler");
            default: return QStringLiteral("Unbekannt");
        }
    };

    auto programText = [](robot::ProgramState state) {
        switch (state) {
            case robot::ProgramState::Stopped: return QStringLiteral("Gestoppt");
            case robot::ProgramState::Playing: return QStringLiteral("Läuft");
            case robot::ProgramState::Paused: return QStringLiteral("Pausiert");
            default: return QStringLiteral("Unbekannt");
        }
    };

    if (m_settings) {
        m_statusRobotIp->setText(m_settings->robotIp());
    }
    m_statusConnection->setText(status.isConnected ? QStringLiteral("Verbunden")
                                                   : QStringLiteral("Getrennt"));
    Theme::setTone(m_statusConnection,
                   status.isConnected ? Theme::Tone::Success : Theme::Tone::Danger);
    m_statusRobotMode->setText(modeText(status.robotMode));
    m_statusSafetyStatus->setText(safetyText(status.safetyStatus));
    const bool safetyNormal = status.safetyStatus == robot::SafetyStatus::Normal;
    const bool safetyReduced = status.safetyStatus == robot::SafetyStatus::ReducedMode;
    Theme::setTone(m_statusSafetyStatus,
                   !status.isConnected ? Theme::Tone::Neutral
                   : safetyNormal ? Theme::Tone::Success
                   : safetyReduced ? Theme::Tone::Warning
                   : Theme::Tone::Danger);
    m_statusProgramState->setText(programText(status.programState));

    if (status.lastUpdate.isValid()) {
        m_statusLastUpdate->setText(status.lastUpdate.toString(QStringLiteral("dd.MM.yyyy HH:mm:ss")));
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
                if (Theme::confirm(this, QStringLiteral("Bestätigung"), message)) {
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
                             QStringLiteral("Ungültige Konfiguration"),
                             QStringLiteral("Eine belegte Palette kann nicht aktiv sein. "
                                            "Die aktive Palette wurde auf \"keine\" (0) zurückgesetzt."));
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
                "Zwischenlage legen und mit Reset bestätigen.",
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

    // A palette that has been full for a while may be released by the operator.
    const auto isDue = [now](bool isEmpty, qint64 nonEmptyTimestamp) {
        return !isEmpty && nonEmptyTimestamp != 0 && (now - nonEmptyTimestamp) >= minWaitMs;
    };
    const bool palette1Due = isDue(m_state->ur20Palette1Empty(), m_state->palette1NonEmptyTimestamp());
    const bool palette2Due = isDue(m_state->ur20Palette2Empty(), m_state->palette2NonEmptyTimestamp());

    ui->palette1ClearButton->setVisible(palette1Due);
    ui->palette2ClearButton->setVisible(palette2Due);
    ui->noticeBar->setVisible(palette1Due || palette2Due);
}

void MainWindow::onPaletteClearClicked(int paletteNumber)
{
    if (!m_state) {
        return;
    }

    if (!Theme::confirm(this, QStringLiteral("Palette freigeben"),
                        QStringLiteral("Möchten Sie Palette %1 zum Palettieren freigeben?")
                            .arg(paletteNumber))) {
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

void MainWindow::incrementUseCycleCount()
{
    if (!m_settings) {
        return;
    }

    const int nextCount = m_settings->numberOfUseCycles() + 1;
    m_settings->setNumberOfUseCycles(nextCount);
    ui->lineEditNumberCycles->setText(QString::number(nextCount));

    if (!m_settings->save()) {
        qWarning() << "MainWindow - failed to persist use cycle count";
    }
}

void MainWindow::loadRobFileList()
{
    const int count = m_database ? static_cast<int>(m_database->listAvailableFiles().size()) : 0;
    if (m_database) {
        ui->lineEditNumberPlans->setText(QString::number(count));
    }

    // The plan list on "Pläne" (MainWindowPlanner.cpp), the completion of the
    // plan input and the database editor all show the stored plans. A stored
    // plan may have new content: the selected one is previewed again.
    m_selectedPlanKey.clear();
    refreshPlannerList();
    refreshPalettePlanCompleter();
    refreshDatabaseManagerPlans();

    qDebug() << "MainWindow - loaded" << count << "rob files";
}

void MainWindow::updateEnabledStates()
{
    bool paletteLoaded = m_paletteLoaded;
    bool canStartServer = !m_serverRunning && m_xmlRpcServer != nullptr;
    const QString startServerText = m_serverRunning ? QStringLiteral("Server läuft ...")
                                                    : QStringLiteral("Server starten");

    // Enable/disable controls based on palette loaded state
    ui->EingabeStartlage->setEnabled(paletteLoaded);
    ui->EingabeKartonhoehe->setEnabled(paletteLoaded);
    ui->EingabeKartonGewicht->setEnabled(paletteLoaded);
    ui->checkBoxEinzelpaket->setEnabled(paletteLoaded);
    ui->checkBoxLabelInvert->setEnabled(paletteLoaded);
    // Robot parameters stay locked until a plan is loaded (button and tab alike).
    ui->ButtonOpenParameterRoboter->setEnabled(paletteLoaded);
    ui->navRoboter->setEnabled(paletteLoaded);
    ui->ButtonDatenSenden->setEnabled(canStartServer);
    ui->ButtonDatenSenden_2->setEnabled(canStartServer);
    ui->ButtonDatenSenden->setText(startServerText);
    ui->ButtonDatenSenden_2->setText(startServerText);

    // Robot controls
    bool serverRunning = m_serverRunning;
    ui->ButtonRoboterStart->setEnabled(serverRunning);
    ui->ButtonRoboterStop->setEnabled(serverRunning);
    ui->ButtonRoboterPause->setEnabled(serverRunning);
    ui->ButtonStopRPCServer->setEnabled(serverRunning);
}

void MainWindow::updateVolumeIcon()
{
    if (!ui->pushButtonVolumeOnOff) {
        return;
    }
    if (m_volumeOn) {
        ui->pushButtonVolumeOnOff->setIcon(QIcon(QStringLiteral(":/icons/volume-on.png")));
    } else {
        ui->pushButtonVolumeOnOff->setIcon(QIcon(QStringLiteral(":/icons/volume-off.png")));
    }
}

// === Navigation Slots ===

void MainWindow::showMainMenu()
{
    setCurrentPage(PAGE_MAIN_MENU);
}

void MainWindow::showPlans()
{
    setCurrentPage(PAGE_PLANS);
}

void MainWindow::showRobotParameters()
{
    // Same lock as the button: no robot parameters without a loaded plan.
    if (!m_paletteLoaded) {
        syncNavigation();
        return;
    }
    setCurrentPage(PAGE_ROBOT_PARAMS);
}

void MainWindow::showQrCode()
{
    setCurrentPage(PAGE_QR_CODE);
}

void MainWindow::showSettings()
{
    setCurrentPage(PAGE_SETTINGS);
}

void MainWindow::showPasswordDialog()
{
    PasswordDialog dialog(this, m_settings);
    if (m_onScreenKeyboard) {
        m_onScreenKeyboard->setHostWidget(&dialog, true);
    }
    QTimer::singleShot(0, &dialog, [&dialog]() {
        dialog.focusPasswordInput();
    });
    
    // Show the dialog and wait for user response
    const int result = dialog.exec();

    if (m_onScreenKeyboard && m_centralWidget) {
        m_onScreenKeyboard->setHostWidget(m_centralWidget);
    }

    if (result == QDialog::Accepted && dialog.wasAccepted()) {
        qDebug() << "Password authenticated - opening settings";
        showSettings();
    } else {
        qDebug() << "Password authentication cancelled or failed";
        syncNavigation();
    }
}

// === Main Menu Slots ===

void MainWindow::onLoadPaletteClicked()
{
    hidePalettePlanCompletionPopup();

    QString fileName = ui->EingabePallettenplan->text().trimmed();
    if (fileName.isEmpty()) {
        showMessage("Bitte Palletierplan eingeben");
        return;
    }

    qDebug() << "Loading palette:" << fileName;

    if (m_database) {
        auto data = m_database->loadPaletteData(fileName);
        if (data.has_value()) {
            applyLoadedPalette(*data, paletteDisplayName(data->metadata.fileName));
        } else {
            showMessage("Palletierplan nicht gefunden: " + fileName);
        }
    }
}

void MainWindow::applyLoadedPalette(const database::PaletteData& data, const QString& displayName)
{
    if (m_state) {
        m_state->applyPaletteData(data);
    }

    m_currentPaletteFile = data.metadata.fileName;
    m_paletteLoaded = true;
    m_packageLength = data.packageDimensions.length;
    m_packageWidth = data.packageDimensions.width;

    // Update UI with loaded data
    QSignalBlocker heightBlocker(ui->EingabeKartonhoehe);
    QSignalBlocker weightBlocker(ui->EingabeKartonGewicht);
    ui->EingabeKartonhoehe->setText(QString::number(data.packageDimensions.height));

    double weight = data.packageDimensions.weight;
    if (weight <= 0.0 && m_packageLength > 0 && m_packageWidth > 0) {
        weight = InputValidation::calculateEstimatedWeight(
            m_packageLength,
            m_packageWidth,
            data.packageDimensions.height);
        m_weightEstimated = true;
        if (m_database && !m_currentPaletteFile.isEmpty()) {
            (void)m_database->updateBoxDimensions(m_currentPaletteFile, -1, weight, -1);
        }
    } else {
        m_weightEstimated = false;
    }

    if (m_state) {
        m_state->setPackageWeight(weight);
        m_state->setMassePaket(weight);
        m_state->setStartLayer(ui->EingabeStartlage->value());
    }

    ui->EingabeKartonGewicht->setText(QString::number(weight, 'f', 2));
    m_lastConfirmedHeight = data.packageDimensions.height;
    m_lastConfirmedWeight = weight;
    ui->EingabeStartlage->setMaximum(data.metadata.anzLagen);
    ui->checkBoxEinzelpaket->setChecked(data.packageDimensions.einzelpaketLaengs);

    // Package count as in the 3D preview and the Planvorschlag dialog; the .rob
    // header field (anzahlPakete) is only the pick count of the first layer type.
    ui->LabelPalletenplanInfo->setText(QStringLiteral("Geladen: %1 - %2 Lagen, %3 Pakete")
        .arg(displayName)
        .arg(data.metadata.anzLagen)
        .arg(visualPaletteFromPaletteData(data, displayName).totalPackages));

    updateEnabledStates();
    updateVisualizationFromPaletteData(data);
    emit paletteLoadRequested(m_currentPaletteFile);

    qDebug() << "Palette loaded successfully:" << m_currentPaletteFile;

    // The running plan changed: refresh the planner list and report it.
    reportActivePlan(true);
}

void MainWindow::onStartServerClicked()
{
    if (!m_xmlRpcServer) {
        showMessage("XML-RPC-Server nicht verfügbar");
        return;
    }

    const int port = m_settings ? m_settings->xmlRpcPort() : config::Defaults::XMLRPC_PORT;
    if (!m_xmlRpcServer->start(port)) {
        qWarning() << "Failed to start XML-RPC server on port" << port;
        return;
    }

    emit serverStartRequested();
    qDebug() << "Server start requested on port" << port;
}

void MainWindow::onParameterRobotClicked()
{
    showRobotParameters();
}

void MainWindow::onSettingsClicked()
{
    if (ui->stackedWidget->currentIndex() == PAGE_SETTINGS) {
        syncNavigation();
        return;
    }

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
    if (ensureRobotConnected()) {
        m_robot->play();
    }
}

void MainWindow::onRobotStopClicked()
{
    qDebug() << "Robot stop clicked";
    if (ensureRobotConnected()) {
        m_robot->stop();
    }
}

void MainWindow::onRobotPauseClicked()
{
    qDebug() << "Robot pause clicked";
    if (ensureRobotConnected()) {
        m_robot->pause();
    }
}

void MainWindow::onStopRpcServerClicked()
{
    if (!m_xmlRpcServer) {
        showMessage("XML-RPC-Server nicht verfügbar");
        return;
    }

    m_xmlRpcServer->stop();
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
        showMessage("Fehler: Einstellungen nicht verfügbar");
        return;
    }

    const QString previousRobotIp = m_settings->robotIp();
    const QString previousUsbPath = m_settings->usbPath().trimmed();
    const QString requestedUsbPath = ui->pathEdit->text().trimmed();

    // Save all settings to SettingsManager
    m_settings->setValue(config::Keys::INFO_UR_MODEL, ui->comboBoxChooseURModel->currentText());
    m_settings->setValue(config::Keys::INFO_UR_SERIAL_NUMBER, ui->lineEditURSerialNo->text());
    m_settings->setValue(config::Keys::INFO_UR_MANUFACTURING_DATE, ui->lineEditURManufacturingDate->text());
    m_settings->setValue(config::Keys::INFO_UR_SOFTWARE_VERSION, ui->lineEditURSoftwareVer->text());
    m_settings->setValue(config::Keys::INFO_PALLETTIERER_NAME, ui->lineEditURName->text());
    m_settings->setValue(config::Keys::INFO_PALLETTIERER_STANDORT, ui->lineEditURStandort->text());

    // Save admin settings
    m_settings->setValue(config::Keys::SERVER_USB_PATH, requestedUsbPath);
    m_settings->setValue(config::Keys::ADMIN_ALARM_SOUND_FILE, ui->audioPathEdit->text());
    m_settings->setValue(config::Keys::ADMIN_SCANNER_WARNING_SOUND_FILE,
                         ui->scannerWarningSoundPathEdit->text());
    m_settings->setRobotIp(previousRobotIp);

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

    if (m_usbMonitor && requestedUsbPath != previousUsbPath) {
        m_usbMonitor->stopMonitoring();
        m_usbMonitor->setUsbPath(requestedUsbPath);
        if (requestedUsbPath.isEmpty()) {
            qInfo() << "MainWindow - palette plan folder monitoring disabled";
        } else if (m_usbMonitor->startMonitoring()) {
            qInfo() << "MainWindow - palette plan folder monitoring restarted for" << requestedUsbPath;
        } else {
            qWarning() << "MainWindow - failed to restart palette plan folder monitoring for" << requestedUsbPath;
            showMessage("Palettierplan-Ordner konnte nicht überwacht werden");
        }
    }

    loadSettings();
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
    if (Theme::confirm(this, QStringLiteral("Bestätigung"),
                       QStringLiteral("Anwendung wirklich neu starten?"))) {
        QApplication::quit();
    }
}

void MainWindow::onSearchUpdateClicked()
{
    qDebug() << "Search update clicked";

    if (!m_autoUpdater) {
        showMessage("Update-System nicht verfügbar");
        return;
    }

    if (m_autoUpdater->currentStatus() == system::UpdateStatus::Checking) {
        showMessage("Update-Prüfung läuft bereits...");
        return;
    }

    showMessage("Prüfe auf Updates...");
    m_autoUpdater->checkForUpdates();
}

void MainWindow::onUpdateCheckCompleted(bool success, const QString& message)
{
    if (!success) {
        showMessage("Update-Prüfung fehlgeschlagen: " + message);
        return;
    }

    if (!m_autoUpdater) {
        return;
    }

    auto updateInfo = m_autoUpdater->availableUpdate();
    if (updateInfo.version.isEmpty()) {
        showMessage("Keine Updates verfügbar. Aktuelle Version: " + m_autoUpdater->currentVersion());
    } else {
        QString msg = QString("Neue Version verfügbar: %1\n\nAktuelle Version: %2\n\nRelease Notes:\n%3")
                          .arg(updateInfo.version)
                          .arg(m_autoUpdater->currentVersion())
                          .arg(updateInfo.releaseNotes);

        if (Theme::confirm(this, QStringLiteral("Update verfügbar"), msg)) {
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

    if (ensureRobotConnected()) {
        QString response = m_robot->sendCommand(command);
        appendConsoleLog(">> " + command);
        appendConsoleLog("<< " + response);
    } else {
        showMessage("Roboter nicht verbunden");
    }
}

void MainWindow::onSelectRobPathClicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Pfad Palettenpläne wählen");
    if (!dir.isEmpty()) {
        ui->pathEdit->setText(dir);
    }
}

void MainWindow::onSelectAudioPathClicked()
{
    QString file = QFileDialog::getOpenFileName(this, "Audio-Datei wählen",
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
    QString file = QFileDialog::getOpenFileName(this, "Scanner Warning Sound wählen",
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
        showMessage("Audio nicht verfügbar");
        return;
    }

    m_audio->playNotification(audio::AudioType::Alarm);
}

void MainWindow::onTestScannerAudioClicked()
{
    if (!m_audio) {
        showMessage("Audio nicht verfügbar");
        return;
    }

    m_audio->playNotification(audio::AudioType::ScannerWarning);
}

void MainWindow::onImportRobFileClicked()
{
    if (!m_database) {
        showMessage("Datenbank nicht verfügbar");
        return;
    }

    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Palettierplan auswählen"),
        QString(),
        tr("Palettierplan-Dateien (*)")
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

    const database::SaveResult saveResult = m_database->savePaletteData(paletteData);
    if (saveResult == database::SaveResult::Error) {
        showMessage("Import fehlgeschlagen: Datenbankfehler");
        return;
    }

    if (saveResult == database::SaveResult::Unchanged) {
        showMessage("Palettierplan bereits aktuell");
    } else {
        showMessage("Palettierplan importiert");
    }
    loadRobFileList();
    updateVisualizationFromPaletteData(paletteData);
}

void MainWindow::updateVisualizationFromPaletteData(const database::PaletteData& data)
{
    if (!m_visualizationWidget) {
        return;
    }
    m_visualizationWidget->setVisualizationData(
        visualPaletteFromPaletteData(data, paletteDisplayName(data.metadata.fileName)));
}

void MainWindow::onOpenFileClicked()
{
    QString path = ui->lineEditFilePath->text();
    if (path.isEmpty()) {
        path = QFileDialog::getOpenFileName(this, "Datei öffnen");
        if (path.isEmpty()) return;
        ui->lineEditFilePath->setText(path);
    }

    QFile file(path);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        ui->textEditFile->setText(QString::fromUtf8(file.readAll()));
        file.close();
    } else {
        showMessage("Datei konnte nicht geöffnet werden");
    }
}

void MainWindow::onSaveOpenFileClicked()
{
    QString path = ui->lineEditFilePath->text().trimmed();
    if (path.isEmpty()) {
        showMessage("Bitte Datei auswählen");
        return;
    }

    if (QFile::exists(path)) {
        if (!Theme::confirm(this, tr("Datei überschreiben?"),
                            tr("Die Datei %1 existiert bereits. Soll sie überschrieben werden?").arg(path))) {
            return;
        }
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        showMessage("Datei konnte nicht gespeichert werden");
        return;
    }

    const QByteArray data = ui->textEditFile->toPlainText().toUtf8();
    if (file.write(data) != data.size()) {
        showMessage("Datei konnte nicht vollständig gespeichert werden");
        return;
    }

    file.close();
    showMessage("Datei gespeichert");
}

void MainWindow::onConsoleCommandEntered()
{
    QString enteredText = ui->lineEditCommand->text();
    if (enteredText.trimmed().isEmpty()) {
        return;
    }

    QString command = enteredText.trimmed();
    appendConsoleLog("$ " + command);
    ui->lineEditCommand->setText("> ");

    QString actualCommand = command;
    if (actualCommand == ">") {
        return;
    }
    if (actualCommand.startsWith('>')) {
        actualCommand = actualCommand.mid(1).trimmed();
    }

    if (actualCommand.isEmpty()) {
        return;
    }

    if (actualCommand.compare("clear", Qt::CaseInsensitive) == 0) {
        ui->textEditConsole->clear();
        return;
    }

    if (m_consoleProcess) {
        m_consoleProcess->disconnect(this);
        m_consoleProcess->kill();
        m_consoleProcess->deleteLater();
        m_consoleProcess = nullptr;
    }

    m_consoleProcess = new QProcess(this);
    m_consoleProcess->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_consoleProcess, &QProcess::readyRead, this, [this]() {
        if (!m_consoleProcess) {
            return;
        }
        const QString output = QString::fromUtf8(m_consoleProcess->readAll());
        if (!output.isEmpty()) {
            appendConsoleLog(output.trimmed());
        }
    });
    connect(m_consoleProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
        if (exitStatus != QProcess::NormalExit) {
            appendConsoleLog("Command terminated unexpectedly");
        } else if (exitCode != 0) {
            appendConsoleLog(QString("Command exited with code %1").arg(exitCode));
        }

        if (m_consoleProcess) {
            m_consoleProcess->deleteLater();
            m_consoleProcess = nullptr;
        }
    });

#if defined(Q_OS_WIN)
    m_consoleProcess->start("cmd.exe", {"/C", actualCommand});
#else
    m_consoleProcess->start("sh", {"-c", actualCommand});
#endif

    if (!m_consoleProcess->waitForStarted(1000)) {
        appendConsoleLog("Failed to start command");
        m_consoleProcess->deleteLater();
        m_consoleProcess = nullptr;
    }
}

// === Plan library ("Pläne") Slots ===

void MainWindow::onDeselectRobFile()
{
    ui->plannerFinalsList->clearSelection();
    ui->plannerFinalsList->setCurrentItem(nullptr);
}

void MainWindow::onFilterChanged()
{
    const int length = ui->lineEditFilterLength->text().toInt();
    const int width = ui->lineEditFilterWidth->text().toInt();
    const int height = ui->lineEditFilterHeight->text().toInt();

    // The filter narrows the whole plan list, planner plans included.
    m_planFilterActive = length != 0 || width != 0 || height != 0;
    m_planFilterLength = length;
    m_planFilterWidth = width;
    m_planFilterHeight = height;
    refreshPlannerList();
}

void MainWindow::onClearFiltersClicked()
{
    ui->lineEditFilterLength->clear();
    ui->lineEditFilterWidth->clear();
    ui->lineEditFilterHeight->clear();
    onFilterChanged();
}

// === Database Manager Tab ===

void MainWindow::setupDatabaseManagerTab()
{
    if (!ui->consoleSettingsTab) {
        return;
    }

    // The former console tab hosts the plan database editor; the console
    // widgets stay (appendConsoleLog still writes to them) but are not shown.
    if (ui->textEditConsole) {
        ui->textEditConsole->hide();
    }
    if (ui->lineEditCommand) {
        ui->lineEditCommand->hide();
    }

    // Admin tool with many fields: dense controls, all inputs above the keyboard.
    Theme::setCompact(ui->consoleSettingsTab);

    auto* rootLayout = new QHBoxLayout(ui->consoleSettingsTab);
    rootLayout->setContentsMargins(16, 10, 16, 10);
    rootLayout->setSpacing(12);

    auto* sidePanel = new QWidget(ui->consoleSettingsTab);
    sidePanel->setFixedWidth(260);
    auto* sideLayout = new QVBoxLayout(sidePanel);
    sideLayout->setContentsMargins(0, 0, 0, 0);
    sideLayout->setSpacing(6);

    auto* listLabel = new QLabel(tr("Palettierpläne"), sidePanel);
    Theme::setRole(listLabel, "heading");
    sideLayout->addWidget(listLabel);

    m_databasePlanList = new QListWidget(sidePanel);
    m_databasePlanList->setSelectionMode(QAbstractItemView::SingleSelection);
    sideLayout->addWidget(m_databasePlanList, 1);

    auto* actionGrid = new QGridLayout();
    actionGrid->setHorizontalSpacing(6);
    actionGrid->setVerticalSpacing(6);

    auto* refreshButton = new QPushButton(tr("Aktualisieren"), sidePanel);
    auto* importButton = new QPushButton(tr("Import"), sidePanel);
    auto* newButton = new QPushButton(tr("Neu"), sidePanel);
    auto* duplicateButton = new QPushButton(tr("Duplizieren"), sidePanel);
    auto* deleteButton = new QPushButton(tr("Löschen"), sidePanel);
    Theme::setRole(deleteButton, "danger");

    actionGrid->addWidget(refreshButton, 0, 0, 1, 2);
    actionGrid->addWidget(importButton, 1, 0);
    actionGrid->addWidget(newButton, 1, 1);
    actionGrid->addWidget(duplicateButton, 2, 0);
    actionGrid->addWidget(deleteButton, 2, 1);
    sideLayout->addLayout(actionGrid);

    auto* editorPanel = new QWidget(ui->consoleSettingsTab);
    auto* editorLayout = new QVBoxLayout(editorPanel);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(8);

    auto* formGroup = new QFrame(editorPanel);
    Theme::setCard(formGroup);
    auto* form = new QGridLayout(formGroup);
    form->setContentsMargins(12, 10, 12, 10);
    form->setHorizontalSpacing(8);
    form->setVerticalSpacing(6);

    auto makeIntSpin = [](int min, int max) {
        auto* spin = new QSpinBox();
        spin->setRange(min, max);
        return spin;
    };
    auto makeDoubleSpin = [](double min, double max, int decimals) {
        auto* spin = new QDoubleSpinBox();
        spin->setRange(min, max);
        spin->setDecimals(decimals);
        return spin;
    };

    m_databasePlanNameEdit = new QLineEdit(formGroup);
    m_databasePaketQuerSpin = makeIntSpin(0, 999999);
    m_databaseCogXSpin = makeDoubleSpin(-999999.0, 999999.0, 3);
    m_databaseCogYSpin = makeDoubleSpin(-999999.0, 999999.0, 3);
    m_databaseCogZSpin = makeDoubleSpin(-999999.0, 999999.0, 3);
    m_databaseLayerTypesSpin = makeIntSpin(0, 999999);
    m_databaseLayerCountSpin = makeIntSpin(0, 999999);
    m_databasePackageCountSpin = makeIntSpin(0, 999999);
    m_databasePalletLengthSpin = makeIntSpin(0, 999999);
    m_databasePalletWidthSpin = makeIntSpin(0, 999999);
    m_databasePalletHeightSpin = makeIntSpin(0, 999999);
    m_databasePackageLengthSpin = makeIntSpin(0, 999999);
    m_databasePackageWidthSpin = makeIntSpin(0, 999999);
    m_databasePackageHeightSpin = makeIntSpin(0, 999999);
    m_databasePackageGapSpin = makeIntSpin(0, 999999);
    m_databasePackageWeightSpin = makeDoubleSpin(0.0, 999999.0, 3);
    m_databaseEinzelpaketCheck = new QCheckBox(tr("Einzelpaket längs"), formGroup);

    // Six rows of three fields; every input stays above the on-screen keyboard.
    const auto caption = [formGroup](const QString& text) {
        auto* label = new QLabel(text, formGroup);
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        return label;
    };
    form->addWidget(caption(tr("Name")), 0, 0);
    form->addWidget(m_databasePlanNameEdit, 0, 1, 1, 3);
    form->addWidget(m_databaseEinzelpaketCheck, 0, 5);
    form->addWidget(caption(tr("Palette L/B/H")), 1, 0);
    form->addWidget(m_databasePalletLengthSpin, 1, 1);
    form->addWidget(m_databasePalletWidthSpin, 1, 3);
    form->addWidget(m_databasePalletHeightSpin, 1, 5);
    form->addWidget(caption(tr("Karton L/B/H")), 2, 0);
    form->addWidget(m_databasePackageLengthSpin, 2, 1);
    form->addWidget(m_databasePackageWidthSpin, 2, 3);
    form->addWidget(m_databasePackageHeightSpin, 2, 5);
    form->addWidget(caption(tr("Schwerpunkt X/Y/Z")), 3, 0);
    form->addWidget(m_databaseCogXSpin, 3, 1);
    form->addWidget(m_databaseCogYSpin, 3, 3);
    form->addWidget(m_databaseCogZSpin, 3, 5);
    form->addWidget(caption(tr("Paket quer")), 4, 0);
    form->addWidget(m_databasePaketQuerSpin, 4, 1);
    form->addWidget(caption(tr("Lagearten")), 4, 2);
    form->addWidget(m_databaseLayerTypesSpin, 4, 3);
    form->addWidget(caption(tr("Lagen")), 4, 4);
    form->addWidget(m_databaseLayerCountSpin, 4, 5);
    form->addWidget(caption(tr("Pakete")), 5, 0);
    form->addWidget(m_databasePackageCountSpin, 5, 1);
    form->addWidget(caption(tr("Spalt")), 5, 2);
    form->addWidget(m_databasePackageGapSpin, 5, 3);
    form->addWidget(caption(tr("Gewicht")), 5, 4);
    form->addWidget(m_databasePackageWeightSpin, 5, 5);
    for (int column : {1, 3, 5}) {
        form->setColumnStretch(column, 1);
    }
    for (int column : {2, 4}) {
        form->setColumnMinimumWidth(column, 84);
    }

    editorLayout->addWidget(formGroup, 0);

    auto* tableTabs = new QTabWidget(editorPanel);
    tableTabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto* positionTab = new QWidget(tableTabs);
    auto* positionLayout = new QVBoxLayout(positionTab);
    positionLayout->setContentsMargins(0, 0, 0, 0);
    positionLayout->setSpacing(6);
    auto* positionButtons = new QHBoxLayout();
    auto* addPositionButton = new QPushButton(tr("Zeile +"), positionTab);
    auto* removePositionButton = new QPushButton(tr("Zeile -"), positionTab);
    positionButtons->addWidget(addPositionButton);
    positionButtons->addWidget(removePositionButton);
    positionButtons->addStretch(1);
    m_databasePositionsTable = new QTableWidget(positionTab);
    m_databasePositionsTable->setColumnCount(9);
    m_databasePositionsTable->setHorizontalHeaderLabels(
        {tr("xp"), tr("yp"), tr("ap"), tr("xd"), tr("yd"), tr("ad"), tr("nop"), tr("xvec"), tr("yvec")});
    m_databasePositionsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_databasePositionsTable->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    positionLayout->addLayout(positionButtons);
    positionLayout->addWidget(m_databasePositionsTable, 1);
    tableTabs->addTab(positionTab, tr("Paketpositionen"));

    auto* layerTab = new QWidget(tableTabs);
    auto* layerLayout = new QVBoxLayout(layerTab);
    layerLayout->setContentsMargins(0, 0, 0, 0);
    layerLayout->setSpacing(6);
    m_databaseLayersTable = new QTableWidget(layerTab);
    m_databaseLayersTable->setColumnCount(3);
    m_databaseLayersTable->setHorizontalHeaderLabels(
        {tr("Lage-Zuordnung"), tr("Zwischenlage"), tr("Pakete je Lageart")});
    m_databaseLayersTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_databaseLayersTable->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layerLayout->addWidget(m_databaseLayersTable, 1);
    tableTabs->addTab(layerTab, tr("Lagen"));

    auto* rawTab = new QWidget(tableTabs);
    auto* rawLayout = new QVBoxLayout(rawTab);
    rawLayout->setContentsMargins(0, 0, 0, 0);
    rawLayout->setSpacing(6);
    auto* rawButtons = new QHBoxLayout();
    auto* addRawRowButton = new QPushButton(tr("Zeile +"), rawTab);
    auto* removeRawRowButton = new QPushButton(tr("Zeile -"), rawTab);
    auto* addRawColumnButton = new QPushButton(tr("Spalte +"), rawTab);
    auto* removeRawColumnButton = new QPushButton(tr("Spalte -"), rawTab);
    rawButtons->addWidget(addRawRowButton);
    rawButtons->addWidget(removeRawRowButton);
    rawButtons->addWidget(addRawColumnButton);
    rawButtons->addWidget(removeRawColumnButton);
    rawButtons->addStretch(1);
    m_databaseRawTable = new QTableWidget(rawTab);
    m_databaseRawTable->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    rawLayout->addLayout(rawButtons);
    rawLayout->addWidget(m_databaseRawTable, 1);
    tableTabs->addTab(rawTab, tr("Rohdaten"));

    editorLayout->addWidget(tableTabs, 1);

    auto* saveRow = new QHBoxLayout();
    saveRow->addStretch(1);
    auto* saveButton = new QPushButton(tr("Speichern / Umbenennen"), editorPanel);
    Theme::setRole(saveButton, "primary");
    saveButton->setMinimumWidth(280);
    saveRow->addWidget(saveButton);
    editorLayout->addLayout(saveRow);

    rootLayout->addWidget(sidePanel, 0);
    rootLayout->addWidget(editorPanel, 1);

    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::refreshDatabaseManagerPlans);
    connect(importButton, &QPushButton::clicked, this, &MainWindow::onImportRobFileClicked);
    connect(newButton, &QPushButton::clicked, this, &MainWindow::onDatabaseNewClicked);
    connect(duplicateButton, &QPushButton::clicked, this, &MainWindow::onDatabaseDuplicateClicked);
    connect(deleteButton, &QPushButton::clicked, this, &MainWindow::onDatabaseDeleteClicked);
    connect(saveButton, &QPushButton::clicked, this, &MainWindow::onDatabaseSaveClicked);
    connect(addPositionButton, &QPushButton::clicked, this, &MainWindow::onDatabaseAddPositionClicked);
    connect(removePositionButton, &QPushButton::clicked, this, &MainWindow::onDatabaseRemovePositionClicked);
    connect(addRawRowButton, &QPushButton::clicked, this, &MainWindow::onDatabaseAddRawRowClicked);
    connect(removeRawRowButton, &QPushButton::clicked, this, &MainWindow::onDatabaseRemoveRawRowClicked);
    connect(addRawColumnButton, &QPushButton::clicked, this, &MainWindow::onDatabaseAddRawColumnClicked);
    connect(removeRawColumnButton, &QPushButton::clicked, this, &MainWindow::onDatabaseRemoveRawColumnClicked);
    connect(m_databasePlanList, &QListWidget::currentItemChanged,
            this, &MainWindow::onDatabasePlanSelectionChanged);

    refreshDatabaseManagerPlans();
}

void MainWindow::refreshDatabaseManagerPlans()
{
    if (!m_databasePlanList) {
        return;
    }

    {
        const QString previousSelection = selectedDatabasePlanFileName();
        QSignalBlocker blocker(m_databasePlanList);
        m_databasePlanList->clear();

        if (!m_database) {
            return;
        }

        const auto files = m_database->listAvailableFiles();
        int rowToSelect = -1;
        for (const auto& file : files) {
            auto* item = new QListWidgetItem(palettePlanDisplayName(file.fileName), m_databasePlanList);
            item->setData(Qt::UserRole, file.fileName);
            item->setData(Qt::UserRole + 1, file.id);
            item->setToolTip(file.timestampStr);
            if (file.fileName == previousSelection || file.fileName == m_databaseEditorOriginalFileName) {
                rowToSelect = m_databasePlanList->row(item);
            }
        }

        if (rowToSelect < 0 && m_databasePlanList->count() > 0) {
            rowToSelect = 0;
        }

        if (rowToSelect >= 0) {
            m_databasePlanList->setCurrentRow(rowToSelect);
        }
    }

    if (m_databasePlanList->currentItem()) {
        onDatabasePlanSelectionChanged();
    }
}

void MainWindow::onDatabasePlanSelectionChanged()
{
    if (!m_database || !m_databasePlanList || !m_databasePlanList->currentItem()) {
        return;
    }

    const int metadataId = m_databasePlanList->currentItem()->data(Qt::UserRole + 1).toInt();
    auto data = m_database->loadPaletteData(QString(), metadataId);
    if (!data.has_value()) {
        showMessage("Palettierplan konnte nicht geladen werden");
        return;
    }

    loadDatabaseEditor(*data);
}

void MainWindow::loadDatabaseEditor(const database::PaletteData& data)
{
    m_databaseEditorData = std::make_unique<database::PaletteData>(data);
    m_databaseEditorOriginalFileName = data.metadata.fileName;

    m_databasePlanNameEdit->setText(palettePlanDisplayName(data.metadata.fileName));
    m_databasePaketQuerSpin->setValue(data.metadata.paketQuer);
    m_databaseCogXSpin->setValue(data.metadata.centerOfGravity.value(0, 0.0));
    m_databaseCogYSpin->setValue(data.metadata.centerOfGravity.value(1, 0.0));
    m_databaseCogZSpin->setValue(data.metadata.centerOfGravity.value(2, 0.0));
    m_databaseLayerTypesSpin->setValue(data.metadata.lageArten);
    m_databaseLayerCountSpin->setValue(data.metadata.anzLagen);
    m_databasePackageCountSpin->setValue(data.metadata.anzahlPakete);
    m_databasePalletLengthSpin->setValue(data.paletteDimensions.length);
    m_databasePalletWidthSpin->setValue(data.paletteDimensions.width);
    m_databasePalletHeightSpin->setValue(data.paletteDimensions.height);
    m_databasePackageLengthSpin->setValue(data.packageDimensions.length);
    m_databasePackageWidthSpin->setValue(data.packageDimensions.width);
    m_databasePackageHeightSpin->setValue(data.packageDimensions.height);
    m_databasePackageGapSpin->setValue(data.packageDimensions.gap);
    m_databasePackageWeightSpin->setValue(data.packageDimensions.weight);
    m_databaseEinzelpaketCheck->setChecked(data.packageDimensions.einzelpaketLaengs);

    m_databasePositionsTable->setRowCount(data.packagePositions.size());
    for (int row = 0; row < data.packagePositions.size(); ++row) {
        const auto& pos = data.packagePositions.at(row);
        const QVector<int> values = {pos.xp, pos.yp, pos.ap, pos.xd, pos.yd, pos.ad, pos.nop, pos.xvec, pos.yvec};
        for (int col = 0; col < values.size(); ++col) {
            m_databasePositionsTable->setItem(row, col, new QTableWidgetItem(QString::number(values.at(col))));
        }
    }

    const int layerRows = qMax(data.layerAssignments.size(),
                               qMax(data.intermediaryLayers.size(), data.packagesPerLayerType.size()));
    m_databaseLayersTable->setRowCount(layerRows);
    for (int row = 0; row < layerRows; ++row) {
        const QVector<int> values = {
            data.layerAssignments.value(row, 0),
            data.intermediaryLayers.value(row, 0),
            data.packagesPerLayerType.value(row, 0)
        };
        for (int col = 0; col < values.size(); ++col) {
            m_databaseLayersTable->setItem(row, col, new QTableWidgetItem(QString::number(values.at(col))));
        }
    }

    int rawColumns = 0;
    for (const auto& row : data.rawData) {
        rawColumns = qMax(rawColumns, row.size());
    }
    m_databaseRawTable->setRowCount(data.rawData.size());
    m_databaseRawTable->setColumnCount(rawColumns);
    for (int row = 0; row < data.rawData.size(); ++row) {
        for (int col = 0; col < rawColumns; ++col) {
            m_databaseRawTable->setItem(row, col,
                                        new QTableWidgetItem(QString::number(data.rawData.at(row).value(col, 0))));
        }
    }
}

bool MainWindow::collectDatabaseEditorData(database::PaletteData& data) const
{
    const QString displayName = databasePlanNameInput();
    if (displayName.isEmpty()) {
        return false;
    }

    data.metadata.fileName = paletteStorageName(displayName);
    data.metadata.fileTimestamp = QDateTime::currentMSecsSinceEpoch();
    data.metadata.paketQuer = m_databasePaketQuerSpin->value();
    data.metadata.centerOfGravity = {
        m_databaseCogXSpin->value(),
        m_databaseCogYSpin->value(),
        m_databaseCogZSpin->value()
    };
    data.metadata.lageArten = m_databaseLayerTypesSpin->value();
    data.metadata.anzLagen = m_databaseLayerCountSpin->value();
    data.metadata.anzahlPakete = m_databasePackageCountSpin->value();

    data.paletteDimensions.length = m_databasePalletLengthSpin->value();
    data.paletteDimensions.width = m_databasePalletWidthSpin->value();
    data.paletteDimensions.height = m_databasePalletHeightSpin->value();

    data.packageDimensions.length = m_databasePackageLengthSpin->value();
    data.packageDimensions.width = m_databasePackageWidthSpin->value();
    data.packageDimensions.height = m_databasePackageHeightSpin->value();
    data.packageDimensions.gap = m_databasePackageGapSpin->value();
    data.packageDimensions.weight = m_databasePackageWeightSpin->value();
    data.packageDimensions.einzelpaketLaengs = m_databaseEinzelpaketCheck->isChecked();

    auto tableInt = [](QTableWidget* table, int row, int col) {
        QTableWidgetItem* item = table->item(row, col);
        return item ? item->text().trimmed().toInt() : 0;
    };

    data.packagePositions.clear();
    for (int row = 0; row < m_databasePositionsTable->rowCount(); ++row) {
        database::PackagePosition pos;
        pos.xp = tableInt(m_databasePositionsTable, row, 0);
        pos.yp = tableInt(m_databasePositionsTable, row, 1);
        pos.ap = tableInt(m_databasePositionsTable, row, 2);
        pos.xd = tableInt(m_databasePositionsTable, row, 3);
        pos.yd = tableInt(m_databasePositionsTable, row, 4);
        pos.ad = tableInt(m_databasePositionsTable, row, 5);
        pos.nop = tableInt(m_databasePositionsTable, row, 6);
        pos.xvec = tableInt(m_databasePositionsTable, row, 7);
        pos.yvec = tableInt(m_databasePositionsTable, row, 8);
        data.packagePositions.append(pos);
    }

    data.layerAssignments.clear();
    data.intermediaryLayers.clear();
    data.packagesPerLayerType.clear();
    for (int row = 0; row < m_databaseLayersTable->rowCount(); ++row) {
        data.layerAssignments.append(tableInt(m_databaseLayersTable, row, 0));
        data.intermediaryLayers.append(tableInt(m_databaseLayersTable, row, 1));
        data.packagesPerLayerType.append(tableInt(m_databaseLayersTable, row, 2));
    }

    data.rawData.clear();
    for (int row = 0; row < m_databaseRawTable->rowCount(); ++row) {
        QVector<int> rawRow;
        for (int col = 0; col < m_databaseRawTable->columnCount(); ++col) {
            rawRow.append(tableInt(m_databaseRawTable, row, col));
        }
        data.rawData.append(rawRow);
    }

    return true;
}

void MainWindow::onDatabaseSaveClicked()
{
    if (!m_database || !m_databaseEditorData) {
        showMessage("Kein Palettierplan ausgewählt");
        return;
    }

    database::PaletteData data = *m_databaseEditorData;
    if (!collectDatabaseEditorData(data)) {
        showMessage("Bitte Namen eingeben");
        return;
    }

    if (data.metadata.fileName != m_databaseEditorOriginalFileName) {
        const auto files = m_database->listAvailableFiles();
        for (const auto& file : files) {
            if (file.fileName.compare(data.metadata.fileName, Qt::CaseInsensitive) == 0) {
                showMessage("Name bereits vorhanden");
                return;
            }
        }
    }

    const database::SaveResult result = m_database->savePaletteData(data);
    if (result == database::SaveResult::Error) {
        showMessage("Speichern fehlgeschlagen");
        return;
    }

    if (!m_databaseEditorOriginalFileName.isEmpty()
        && data.metadata.fileName != m_databaseEditorOriginalFileName) {
        (void)m_database->deletePalette(m_databaseEditorOriginalFileName);
    }

    m_databaseEditorOriginalFileName = data.metadata.fileName;
    m_databaseEditorData = std::make_unique<database::PaletteData>(data);
    loadRobFileList();
    showMessage("Palettierplan gespeichert");
}

void MainWindow::onDatabaseNewClicked()
{
    database::PaletteData data;
    data.metadata.fileName = paletteStorageName("neuer-plan");
    data.metadata.fileTimestamp = QDateTime::currentMSecsSinceEpoch();
    data.metadata.paketQuer = 1;
    data.metadata.centerOfGravity = {0.0, 0.0, 0.0};
    data.metadata.lageArten = 1;
    data.metadata.anzLagen = 1;
    data.metadata.anzahlPakete = 0;
    data.paletteDimensions.length = 1200;
    data.paletteDimensions.width = 800;
    data.packageDimensions.einzelpaketLaengs = false;

    m_databaseEditorOriginalFileName.clear();
    loadDatabaseEditor(data);
    m_databasePlanNameEdit->setFocus();
    showMessage("Neuer Palettierplan vorbereitet");
}

void MainWindow::onDatabaseDuplicateClicked()
{
    if (!m_database || !m_databaseEditorData) {
        showMessage("Bitte Palettierplan auswählen");
        return;
    }

    database::PaletteData data = *m_databaseEditorData;
    const QString baseName = palettePlanDisplayName(data.metadata.fileName);
    QString candidate = baseName + QStringLiteral("-kopie");
    int suffix = 2;
    const auto files = m_database->listAvailableFiles();
    auto exists = [&files](const QString& storageName) {
        for (const auto& file : files) {
            if (file.fileName.compare(storageName, Qt::CaseInsensitive) == 0) {
                return true;
            }
        }
        return false;
    };
    while (exists(paletteStorageName(candidate))) {
        candidate = QStringLiteral("%1-kopie-%2").arg(baseName).arg(suffix++);
    }

    data.metadata.fileName = paletteStorageName(candidate);
    data.metadata.fileTimestamp = QDateTime::currentMSecsSinceEpoch();
    const database::SaveResult result = m_database->savePaletteData(data);
    if (result == database::SaveResult::Error) {
        showMessage("Duplizieren fehlgeschlagen");
        return;
    }

    m_databaseEditorOriginalFileName = data.metadata.fileName;
    loadRobFileList();
    showMessage("Palettierplan dupliziert");
}

void MainWindow::onDatabaseDeleteClicked()
{
    if (!m_database || !m_databaseEditorData) {
        showMessage("Bitte Palettierplan auswählen");
        return;
    }

    const QString displayName = palettePlanDisplayName(m_databaseEditorOriginalFileName);
    if (!Theme::confirm(this, tr("Palettierplan löschen"),
                        tr("Palettierplan \"%1\" wirklich löschen?").arg(displayName))) {
        return;
    }

    if (!m_database->deletePalette(m_databaseEditorOriginalFileName)) {
        showMessage("Löschen fehlgeschlagen");
        return;
    }

    m_databaseEditorData.reset();
    m_databaseEditorOriginalFileName.clear();
    loadRobFileList();
    showMessage("Palettierplan gelöscht");
}

void MainWindow::onDatabaseAddPositionClicked()
{
    if (!m_databasePositionsTable) {
        return;
    }
    const int row = m_databasePositionsTable->rowCount();
    m_databasePositionsTable->insertRow(row);
    for (int col = 0; col < m_databasePositionsTable->columnCount(); ++col) {
        m_databasePositionsTable->setItem(row, col, new QTableWidgetItem(QStringLiteral("0")));
    }
}

void MainWindow::onDatabaseRemovePositionClicked()
{
    if (!m_databasePositionsTable) {
        return;
    }
    const int row = m_databasePositionsTable->currentRow() >= 0
        ? m_databasePositionsTable->currentRow()
        : m_databasePositionsTable->rowCount() - 1;
    if (row >= 0) {
        m_databasePositionsTable->removeRow(row);
    }
}

void MainWindow::onDatabaseAddRawRowClicked()
{
    if (!m_databaseRawTable) {
        return;
    }
    if (m_databaseRawTable->columnCount() == 0) {
        m_databaseRawTable->setColumnCount(1);
    }
    const int row = m_databaseRawTable->rowCount();
    m_databaseRawTable->insertRow(row);
    for (int col = 0; col < m_databaseRawTable->columnCount(); ++col) {
        m_databaseRawTable->setItem(row, col, new QTableWidgetItem(QStringLiteral("0")));
    }
}

void MainWindow::onDatabaseRemoveRawRowClicked()
{
    if (!m_databaseRawTable) {
        return;
    }
    const int row = m_databaseRawTable->currentRow() >= 0
        ? m_databaseRawTable->currentRow()
        : m_databaseRawTable->rowCount() - 1;
    if (row >= 0) {
        m_databaseRawTable->removeRow(row);
    }
}

void MainWindow::onDatabaseAddRawColumnClicked()
{
    if (!m_databaseRawTable) {
        return;
    }
    const int col = m_databaseRawTable->columnCount();
    m_databaseRawTable->insertColumn(col);
    for (int row = 0; row < m_databaseRawTable->rowCount(); ++row) {
        m_databaseRawTable->setItem(row, col, new QTableWidgetItem(QStringLiteral("0")));
    }
}

void MainWindow::onDatabaseRemoveRawColumnClicked()
{
    if (!m_databaseRawTable) {
        return;
    }
    const int col = m_databaseRawTable->currentColumn() >= 0
        ? m_databaseRawTable->currentColumn()
        : m_databaseRawTable->columnCount() - 1;
    if (col >= 0) {
        m_databaseRawTable->removeColumn(col);
    }
}

QString MainWindow::selectedDatabasePlanFileName() const
{
    if (!m_databasePlanList || !m_databasePlanList->currentItem()) {
        return QString();
    }
    return m_databasePlanList->currentItem()->data(Qt::UserRole).toString();
}

QString MainWindow::databasePlanNameInput() const
{
    if (!m_databasePlanNameEdit) {
        return QString();
    }
    return palettePlanDisplayName(m_databasePlanNameEdit->text());
}

QString MainWindow::paletteStorageName(const QString& displayName)
{
    QString name = palettePlanDisplayName(displayName);
    if (name.isEmpty()) {
        return QString();
    }
    return name + QStringLiteral(".rob");
}

// === Public Slots ===

void MainWindow::updateRobotStatus()
{
    if (!m_robot) return;

    bool connected = m_robot->isConnected();
    if (m_state) {
        m_state->setRobotConnected(connected);
    }

    // Update UI based on robot status
    if (connected) {
        ui->lineEditURSerialNo->setText(m_robot->getSerialNumber());
        ui->lineEditURSoftwareVer->setText(m_robot->getSoftwareVersion());
        // Show the start of a long version string, not its end.
        ui->lineEditURSoftwareVer->setCursorPosition(0);
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

void MainWindow::setServerRunning(bool running, const QString& error)
{
    const bool wasRunning = m_serverRunning;
    m_serverRunning = running;
    updateEnabledStates();

    if (!error.isEmpty()) {
        showMessage("XML-RPC Fehler: " + error);
        return;
    }

    if (running && !wasRunning) {
        showMessage("XMLRPC Server gestartet");
    } else if (!running && wasRunning) {
        showMessage("XMLRPC Server gestoppt");
    }
}

void MainWindow::onStatusChanged(const QString& message, message::StatusType type)
{
    Theme::Tone tone = Theme::Tone::Neutral;
    switch (type) {
        case message::StatusType::Busy:
            tone = Theme::Tone::Info;
            break;
        case message::StatusType::Success:
            tone = Theme::Tone::Success;
            break;
        case message::StatusType::Warning:
            tone = Theme::Tone::Warning;
            break;
        case message::StatusType::Error:
            tone = Theme::Tone::Danger;
            break;
        default:
            break;
    }

    // Long messages wrap and, if still too long, get a smaller font instead of
    // being clipped (the status line is two lines high).
    QLabel* label = ui->label_GewichtInfo;
    label->setWordWrap(true);
    label->setText(message);
    QFont font = label->font();
    int pixelSize = 19;
    for (; pixelSize > 12; --pixelSize) {
        font.setPixelSize(pixelSize);
        const QRect needed = QFontMetrics(font).boundingRect(
            QRect(0, 0, label->width(), 10'000), Qt::TextWordWrap, message);
        if (needed.height() <= label->height()) {
            break;
        }
    }
    label->setStyleSheet(QStringLiteral("color: %1; font-size: %2px;")
                             .arg(Theme::toneColor(tone)).arg(pixelSize));
}

void MainWindow::setupPalettePlanCompleter()
{
    qDebug() << "Setting up palette plan auto-completion";
    
    if (!ui->EingabePallettenplan) {
        return;
    }

    m_palettePlanCompleterModel = new QStringListModel(this);
    m_palettePlanCompletionView = new QListView(m_centralWidget);
    m_palettePlanCompletionView->setObjectName(QStringLiteral("PalettePlanCompletionPopup"));
    m_palettePlanCompletionView->setModel(m_palettePlanCompleterModel);
    m_palettePlanCompletionView->setFocusPolicy(Qt::NoFocus);
    m_palettePlanCompletionView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_palettePlanCompletionView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_palettePlanCompletionView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_palettePlanCompletionView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_palettePlanCompletionView->setStyleSheet(QStringLiteral(
        "QListView#PalettePlanCompletionPopup {"
        "background: white; border: 2px solid #2f6fed; border-radius: 6px; font-size: 22px; }"
        "QListView#PalettePlanCompletionPopup::item { min-height: 48px; padding: 0 10px; }"));
    m_palettePlanCompletionView->hide();

    ui->EingabePallettenplan->setCompleter(nullptr);
    connect(ui->EingabePallettenplan, &QLineEdit::textChanged,
            this, &MainWindow::updatePalettePlanCompletionPopup);
    // The popup has no focus of its own: leaving the field closes it.
    connect(ui->EingabePallettenplan, &QLineEdit::editingFinished,
            this, &MainWindow::hidePalettePlanCompletionPopup);
    connect(m_palettePlanCompletionView, &QListView::clicked, this,
            [this](const QModelIndex& index) {
                const QString value = index.data(Qt::DisplayRole).toString();
                if (value.isEmpty()) {
                    return;
                }
                ui->EingabePallettenplan->setText(value);
                hidePalettePlanCompletionPopup();
                ui->EingabePallettenplan->setFocus(Qt::OtherFocusReason);
            });

    refreshPalettePlanCompleter();
    
    qDebug() << "Palette plan completer set up for Palettierplan input";
}

void MainWindow::appendConsoleLog(const QString& text)
{
    if (ui && ui->textEditConsole) {
        ui->textEditConsole->append(text);
    }
}

void MainWindow::refreshPalettePlanCompleter()
{
    if (!m_palettePlanCompleterModel) {
        return;
    }

    m_palettePlanCompletionWords.clear();
    if (m_database) {
        const auto files = m_database->listAvailableFiles();
        for (const auto& file : files) {
            const QString fileName = palettePlanDisplayName(file.fileName);
            if (!fileName.isEmpty()) {
                m_palettePlanCompletionWords.append(fileName);
            }
        }
    }

    m_palettePlanCompletionWords.removeDuplicates();
    m_palettePlanCompletionWords.sort(Qt::CaseInsensitive);
    updatePalettePlanCompletionPopup(ui->EingabePallettenplan ? ui->EingabePallettenplan->text() : QString());

    qDebug() << "Palette plan completer refreshed with" << m_palettePlanCompletionWords.size() << "entries";
}

void MainWindow::updatePalettePlanCompletionPopup(const QString& text)
{
    if (!m_palettePlanCompletionView || !m_palettePlanCompleterModel || !ui->EingabePallettenplan) {
        return;
    }

    // Suggestions belong to the field being edited. Without this a database
    // refresh re-opened the popup over whatever page was showing.
    const QString prefix = text.trimmed();
    if (prefix.isEmpty() || !ui->EingabePallettenplan->hasFocus()
        || ui->stackedWidget->currentIndex() != PAGE_MAIN_MENU) {
        hidePalettePlanCompletionPopup();
        return;
    }

    QStringList matches;
    for (const QString& word : m_palettePlanCompletionWords) {
        if (word.startsWith(prefix, Qt::CaseInsensitive)) {
            matches.append(word);
        }
    }

    if (matches.isEmpty()) {
        hidePalettePlanCompletionPopup();
        return;
    }

    m_palettePlanCompleterModel->setStringList(matches);

    const QPoint popupPos = ui->EingabePallettenplan->mapTo(m_centralWidget, QPoint(0, ui->EingabePallettenplan->height()));
    const int rowHeight = 50;
    const int visibleRows = qMin(matches.size(), 4);
    m_palettePlanCompletionView->setGeometry(
        popupPos.x(),
        popupPos.y(),
        ui->EingabePallettenplan->width(),
        visibleRows * rowHeight + 6);
    m_palettePlanCompletionView->raise();
    m_palettePlanCompletionView->show();
}

void MainWindow::hidePalettePlanCompletionPopup()
{
    if (m_palettePlanCompletionView) {
        m_palettePlanCompletionView->hide();
    }
}
} // namespace ui
} // namespace multipack
