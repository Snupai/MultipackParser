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
#include "multipack/config/ConfigDefaults.h"

#include <QApplication>
#include <QScreen>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QFileSystemModel>
#include <QDebug>
#include <QIcon>
#include <QPixmap>

namespace multipack {
namespace ui {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::Form)
{
    qDebug() << "MainWindow - initializing with UI file";

    // Create central widget and setup UI
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    ui->setupUi(centralWidget);

    // Setup connections
    setupConnections();

    // Start on main menu
    ui->stackedWidget->setCurrentIndex(PAGE_MAIN_MENU);

    // Set window properties
    setWindowTitle("Palletierer");
    setFixedSize(1280, 720);

    qDebug() << "MainWindow - initialized";
}

MainWindow::~MainWindow()
{
    delete ui;
    qDebug() << "MainWindow - destroyed";
}

void MainWindow::setSettingsManager(config::SettingsManager* settings)
{
    m_settings = settings;
    loadSettings();
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
}

void MainWindow::setGlobalState(core::GlobalState* state)
{
    m_state = state;
}

void MainWindow::setAudioManager(audio::AudioManager* audio)
{
    m_audio = audio;
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
    connect(ui->EingabeKartonhoehe, &QLineEdit::editingFinished, this, &MainWindow::onKartonhoeheChanged);
    connect(ui->EingabeKartonGewicht, &QLineEdit::editingFinished, this, &MainWindow::onGewichtChanged);
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
    connect(ui->pushButtonOpenFile, &QPushButton::clicked, this, &MainWindow::onOpenFileClicked);
    connect(ui->lineEditCommand, &QLineEdit::returnPressed, this, &MainWindow::onConsoleCommandEntered);

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

            // Update UI with loaded data
            ui->EingabeKartonhoehe->setText(QString::number(data->packageDimensions.height));
            ui->EingabeKartonGewicht->setText(QString::number(data->packageDimensions.weight, 'f', 2));
            ui->EingabeStartlage->setMaximum(data->metadata.anzLagen);
            ui->checkBoxEinzelpaket->setChecked(data->packageDimensions.einzelpaketLaengs);

            ui->LabelPalletenplanInfo->setText(QString("Geladen: %1 - %2 Lagen, %3 Pakete")
                .arg(fileName)
                .arg(data->metadata.anzLagen)
                .arg(data->metadata.anzahlPakete));

            updateEnabledStates();
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
    showSettings();
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

    // Update global state
    if (m_state) {
        m_state->setAudioMuted(!m_volumeOn);
    }
}

void MainWindow::onExperimentalClicked()
{
    showExperimental();
}

void MainWindow::onEinzelpaketChanged(Qt::CheckState state)
{
    bool checked = (state == Qt::CheckState::Checked);
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

void MainWindow::onLabelInvertChanged(Qt::CheckState state)
{
    bool checked = (state == Qt::CheckState::Checked);
    qDebug() << "Label invert changed:" << checked;

    // Update global state - label invert affects paket orientation
    if (m_state) {
        m_state->setPaketQuer(checked ? 2 : 1);
    }
}

void MainWindow::onKartonhoeheChanged()
{
    QString text = ui->EingabeKartonhoehe->text();
    int height = text.toInt();
    qDebug() << "Kartonhoehe changed:" << height;

    if (m_database && !m_currentPaletteFile.isEmpty()) {
        if (!m_database->updateBoxDimensions(m_currentPaletteFile, height, -1.0, -1)) {
            qWarning() << "Failed to update box height in database";
        }
    }
}

void MainWindow::onGewichtChanged()
{
    QString text = ui->EingabeKartonGewicht->text();
    double weight = text.toDouble();
    qDebug() << "Gewicht changed:" << weight;

    if (m_database && !m_currentPaletteFile.isEmpty()) {
        if (!m_database->updateBoxDimensions(m_currentPaletteFile, -1, weight, -1)) {
            qWarning() << "Failed to update box weight in database";
        }
    }
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
}

void MainWindow::onVerschiebungYChanged(int value)
{
    qDebug() << "Verschiebung Y changed:" << value;

    // Store in settings for RPC server access
    if (m_settings) {
        m_settings->setValue("aufnahme.verschiebung_y", value);
    }
}

void MainWindow::onKlemmungChanged(Qt::CheckState state)
{
    bool checked = (state == Qt::CheckState::Checked);
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
    showMessage("Update-Suche nicht implementiert");
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
    }
}

void MainWindow::onSelectScannerSoundPathClicked()
{
    QString file = QFileDialog::getOpenFileName(this, "Scanner Warning Sound waehlen",
        QString(), "Audio Files (*.wav *.mp3 *.ogg)");
    if (!file.isEmpty()) {
        ui->scannerWarningSoundPathEdit->setText(file);
    }
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

    ui->EingabePallettenplan->setText(fileName);
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
    ui->label_GewichtInfo->setText(message);
    qDebug() << "Message:" << message;
}

void MainWindow::appendConsoleLog(const QString& text)
{
    ui->textEditConsole->append(text);
}

} // namespace ui
} // namespace multipack
