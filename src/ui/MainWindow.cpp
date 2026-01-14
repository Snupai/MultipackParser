/**
 * @file MainWindow.cpp
 * @brief Full implementation of main application window
 */
#include "multipack/ui/MainWindow.h"
#include "multipack/config/SettingsManager.h"
#include "multipack/database/DatabaseManager.h"
#include "multipack/robot/RobotController.h"
#include "multipack/core/GlobalState.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFrame>
#include <QFont>
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
{
    qDebug() << "MainWindow - initializing";

    setupUi();
    setupConnections();
    applyStyleSheet();

    // Start on main menu
    m_stackedWidget->setCurrentIndex(PAGE_MAIN_MENU);

    qDebug() << "MainWindow - initialized";
}

MainWindow::~MainWindow()
{
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

void MainWindow::setupUi()
{
    // Window properties
    setWindowTitle("Palletierer");
    setFixedSize(1280, 720);

    // Create central widget
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    // Create stacked widget for pages
    m_stackedWidget = new QStackedWidget(centralWidget);
    m_stackedWidget->setGeometry(0, 0, 1280, 711);

    // Setup all pages
    setupMainMenuPage();
    setupRobotParametersPage();
    setupSettingsPage();
    setupExperimentalPage();
}

void MainWindow::setupMainMenuPage()
{
    m_mainMenuPage = new QWidget();

    QFont titleFont("Sans Serif", 18);
    QFont labelFont("Sans Serif", 16);
    QFont buttonFont("Sans Serif", 16);

    // Settings button (top-left)
    m_buttonSettings = new QPushButton(m_mainMenuPage);
    m_buttonSettings->setGeometry(0, 0, 61, 61);
    m_buttonSettings->setFlat(true);
    m_buttonSettings->setCursor(Qt::PointingHandCursor);
    m_buttonSettings->setToolTip("Einstellungen");
    m_buttonSettings->setIcon(QIcon(":/icons/settings.png"));
    m_buttonSettings->setIconSize(QSize(50, 50));

    // Volume button (top-right)
    m_buttonVolumeOnOff = new QPushButton(m_mainMenuPage);
    m_buttonVolumeOnOff->setGeometry(1221, -1, 60, 60);
    m_buttonVolumeOnOff->setFlat(true);
    m_buttonVolumeOnOff->setCursor(Qt::PointingHandCursor);
    m_buttonVolumeOnOff->setCheckable(false);
    m_buttonVolumeOnOff->setIcon(QIcon(":/icons/volume-on.png"));
    m_buttonVolumeOnOff->setIconSize(QSize(50, 50));

    // Palette info label (top center)
    m_labelPaletteInfo = new QLabel(m_mainMenuPage);
    m_labelPaletteInfo->setGeometry(60, 0, 1161, 51);
    m_labelPaletteInfo->setFont(titleFont);
    m_labelPaletteInfo->setAlignment(Qt::AlignCenter);

    // Horizontal line
    QFrame* line = new QFrame(m_mainMenuPage);
    line->setGeometry(0, 50, 1281, 16);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);

    // Palletierplan label and input
    QLabel* labelPlan = new QLabel("<html><body><p align=\"right\">Palletierplan: </p></body></html>", m_mainMenuPage);
    labelPlan->setGeometry(80, 90, 241, 41);
    labelPlan->setFont(labelFont);

    m_inputPalettePlan = new QLineEdit(m_mainMenuPage);
    m_inputPalettePlan->setGeometry(322, 90, 461, 39);
    m_inputPalettePlan->setFont(labelFont);
    m_inputPalettePlan->setPlaceholderText("z.B: 699-00120");

    m_buttonLoadPalette = new QPushButton(m_mainMenuPage);
    m_buttonLoadPalette->setGeometry(795, 80, 60, 60);
    m_buttonLoadPalette->setFlat(true);
    m_buttonLoadPalette->setCursor(Qt::PointingHandCursor);
    m_buttonLoadPalette->setToolTip("Pallettenplan laden");
    m_buttonLoadPalette->setIcon(QIcon(":/icons/load.png"));
    m_buttonLoadPalette->setIconSize(QSize(50, 50));

    // Startlage label and input
    QLabel* labelStartlage = new QLabel("<html><body><p align=\"right\">Startlage: </p></body></html>", m_mainMenuPage);
    labelStartlage->setGeometry(80, 140, 241, 41);
    labelStartlage->setFont(labelFont);
    labelStartlage->setEnabled(false);

    m_inputStartlage = new QSpinBox(m_mainMenuPage);
    m_inputStartlage->setGeometry(322, 144, 461, 39);
    m_inputStartlage->setFont(labelFont);
    m_inputStartlage->setMinimum(1);
    m_inputStartlage->setValue(1);
    m_inputStartlage->setEnabled(false);

    // Kartonhoehe label and input
    QLabel* labelKartonhoehe = new QLabel("<html><body><p align=\"right\">Kartonhoehe: </p></body></html>", m_mainMenuPage);
    labelKartonhoehe->setGeometry(80, 194, 241, 41);
    labelKartonhoehe->setFont(labelFont);
    labelKartonhoehe->setEnabled(false);

    m_inputKartonhoehe = new QLineEdit(m_mainMenuPage);
    m_inputKartonhoehe->setGeometry(322, 198, 461, 39);
    m_inputKartonhoehe->setFont(labelFont);
    m_inputKartonhoehe->setEnabled(false);

    QLabel* labelMm = new QLabel("mm", m_mainMenuPage);
    labelMm->setGeometry(790, 200, 121, 41);
    labelMm->setFont(labelFont);
    labelMm->setEnabled(false);

    // Gewicht label and input
    QLabel* labelGewicht = new QLabel("<html><body><p align=\"right\">Gewicht: </p></body></html>", m_mainMenuPage);
    labelGewicht->setGeometry(80, 248, 241, 41);
    labelGewicht->setFont(labelFont);
    labelGewicht->setEnabled(false);

    m_inputKartonGewicht = new QLineEdit(m_mainMenuPage);
    m_inputKartonGewicht->setGeometry(322, 252, 461, 39);
    m_inputKartonGewicht->setFont(labelFont);
    m_inputKartonGewicht->setEnabled(false);

    QLabel* labelKg = new QLabel("kg", m_mainMenuPage);
    labelKg->setGeometry(790, 250, 121, 41);
    labelKg->setFont(labelFont);
    labelKg->setEnabled(false);

    // Gewicht info label
    m_labelGewichtInfo = new QLabel(m_mainMenuPage);
    m_labelGewichtInfo->setGeometry(330, 290, 511, 61);
    m_labelGewichtInfo->setFont(QFont("Sans Serif", 16, QFont::Normal, false));

    // Checkboxes
    m_checkEinzelpaket = new QCheckBox("Einzelpaket laengs", m_mainMenuPage);
    m_checkEinzelpaket->setGeometry(70, 350, 251, 61);
    m_checkEinzelpaket->setFont(labelFont);
    m_checkEinzelpaket->setLayoutDirection(Qt::RightToLeft);
    m_checkEinzelpaket->setCursor(Qt::PointingHandCursor);
    m_checkEinzelpaket->setEnabled(false);

    m_checkLabelInvert = new QCheckBox("Invert Label", m_mainMenuPage);
    m_checkLabelInvert->setGeometry(70, 410, 251, 31);
    m_checkLabelInvert->setFont(labelFont);
    m_checkLabelInvert->setLayoutDirection(Qt::RightToLeft);
    m_checkLabelInvert->setCursor(Qt::PointingHandCursor);
    m_checkLabelInvert->setEnabled(false);

    // Action buttons
    m_buttonParameterRobot = new QPushButton("Parameter Roboter", m_mainMenuPage);
    m_buttonParameterRobot->setGeometry(330, 350, 261, 61);
    m_buttonParameterRobot->setFont(buttonFont);
    m_buttonParameterRobot->setCursor(Qt::PointingHandCursor);
    m_buttonParameterRobot->setEnabled(false);

    m_buttonStartServer = new QPushButton("Server starten", m_mainMenuPage);
    m_buttonStartServer->setGeometry(610, 350, 261, 61);
    m_buttonStartServer->setFont(buttonFont);
    m_buttonStartServer->setCursor(Qt::PointingHandCursor);
    m_buttonStartServer->setEnabled(false);

    // Szaidel logo
    m_labelLogo = new QLabel(m_mainMenuPage);
    m_labelLogo->setGeometry(940, 90, 271, 271);
    m_labelLogo->setScaledContents(true);
    m_labelLogo->setPixmap(QPixmap(":/images/logo.png"));

    // Experimental button
    m_buttonExperimental = new QPushButton("Experimental", m_mainMenuPage);
    m_buttonExperimental->setGeometry(1150, 660, 121, 41);

    m_stackedWidget->addWidget(m_mainMenuPage);
}

void MainWindow::setupRobotParametersPage()
{
    m_robotParamsPage = new QWidget();

    m_robotTabWidget = new QTabWidget(m_robotParamsPage);
    m_robotTabWidget->setGeometry(0, 0, 1280, 711);
    m_robotTabWidget->setFont(QFont("Sans Serif", 16));

    QFont buttonFont("Sans Serif", 16);

    // === Roboter Tab ===
    QWidget* roboterTab = new QWidget();

    // Szaidel logo
    QLabel* logoLabel = new QLabel(roboterTab);
    logoLabel->setGeometry(360, -35, 600, 337);
    logoLabel->setScaledContents(true);

    m_buttonZurueck1 = new QPushButton(roboterTab);
    m_buttonZurueck1->setGeometry(0, 140, 81, 81);
    m_buttonZurueck1->setFlat(true);
    m_buttonZurueck1->setCursor(Qt::PointingHandCursor);
    m_buttonZurueck1->setToolTip("Zurueck");
    m_buttonZurueck1->setIcon(QIcon(":/icons/back.png"));
    m_buttonZurueck1->setIconSize(QSize(70, 70));

    m_buttonRobotStart = new QPushButton("Roboter Start", roboterTab);
    m_buttonRobotStart->setGeometry(140, 70, 341, 71);
    m_buttonRobotStart->setFont(buttonFont);
    m_buttonRobotStart->setCursor(Qt::PointingHandCursor);
    m_buttonRobotStart->setEnabled(false);

    m_buttonRobotStop = new QPushButton("Roboter Stop", roboterTab);
    m_buttonRobotStop->setGeometry(830, 70, 341, 71);
    m_buttonRobotStop->setFont(buttonFont);
    m_buttonRobotStop->setCursor(Qt::PointingHandCursor);
    m_buttonRobotStop->setEnabled(false);

    m_buttonRobotPause = new QPushButton("Roboter Pause", roboterTab);
    m_buttonRobotPause->setGeometry(140, 220, 341, 71);
    m_buttonRobotPause->setFont(buttonFont);
    m_buttonRobotPause->setCursor(Qt::PointingHandCursor);
    m_buttonRobotPause->setEnabled(false);

    m_buttonStopRpcServer = new QPushButton("Stop RPC Server", roboterTab);
    m_buttonStopRpcServer->setGeometry(830, 220, 341, 71);
    m_buttonStopRpcServer->setFont(buttonFont);
    m_buttonStopRpcServer->setCursor(Qt::PointingHandCursor);
    m_buttonStopRpcServer->setEnabled(false);

    m_robotTabWidget->addTab(roboterTab, "Roboter");

    // === Aufnahme Tab ===
    QWidget* aufnahmeTab = new QWidget();

    m_buttonZurueck2 = new QPushButton(aufnahmeTab);
    m_buttonZurueck2->setGeometry(0, 140, 81, 81);
    m_buttonZurueck2->setFlat(true);
    m_buttonZurueck2->setCursor(Qt::PointingHandCursor);
    m_buttonZurueck2->setIcon(QIcon(":/icons/back.png"));
    m_buttonZurueck2->setIconSize(QSize(70, 70));

    m_imageAufnahmePos = new QLabel(aufnahmeTab);
    m_imageAufnahmePos->setGeometry(110, 20, 390, 270);
    m_imageAufnahmePos->setFrameShape(QFrame::Box);
    m_imageAufnahmePos->setScaledContents(true);
    m_imageAufnahmePos->setPixmap(QPixmap(":/images/Aufnahmepos.png"));

    QLabel* labelVerschiebungX = new QLabel("verschieben in Richtung x von:", aufnahmeTab);
    labelVerschiebungX->setGeometry(550, 60, 411, 31);
    labelVerschiebungX->setFont(QFont("Sans Serif", 16));

    m_inputVerschiebungX = new QSpinBox(aufnahmeTab);
    m_inputVerschiebungX->setGeometry(970, 60, 170, 41);
    m_inputVerschiebungX->setSuffix(" mm");
    m_inputVerschiebungX->setMinimum(-20);
    m_inputVerschiebungX->setMaximum(20);

    QLabel* labelVerschiebungY = new QLabel("verschieben in Richtung y von:", aufnahmeTab);
    labelVerschiebungY->setGeometry(550, 160, 411, 31);
    labelVerschiebungY->setFont(QFont("Sans Serif", 16));

    m_inputVerschiebungY = new QSpinBox(aufnahmeTab);
    m_inputVerschiebungY->setGeometry(970, 160, 170, 41);
    m_inputVerschiebungY->setSuffix(" mm");
    m_inputVerschiebungY->setMinimum(-20);
    m_inputVerschiebungY->setMaximum(20);

    m_buttonAufnahmeServer = new QPushButton("Server starten", aufnahmeTab);
    m_buttonAufnahmeServer->setGeometry(550, 250, 590, 91);
    m_buttonAufnahmeServer->setCursor(Qt::PointingHandCursor);

    m_checkKlemmung = new QCheckBox("Klemmung aktiv", aufnahmeTab);
    m_checkKlemmung->setGeometry(200, 300, 221, 31);
    m_checkKlemmung->setLayoutDirection(Qt::RightToLeft);
    m_checkKlemmung->setChecked(true);
    m_checkKlemmung->setCursor(Qt::PointingHandCursor);

    m_robotTabWidget->addTab(aufnahmeTab, "Aufnahme");

    m_stackedWidget->addWidget(m_robotParamsPage);
}

void MainWindow::setupSettingsPage()
{
    m_settingsPage = new QWidget();

    m_settingsTabWidget = new QTabWidget(m_settingsPage);
    m_settingsTabWidget->setGeometry(0, 0, 1280, 390);

    // === Info Tab ===
    QWidget* infoTab = new QWidget();

    m_buttonZurueck3 = new QPushButton(infoTab);
    m_buttonZurueck3->setGeometry(0, 130, 81, 81);
    m_buttonZurueck3->setFlat(true);
    m_buttonZurueck3->setCursor(Qt::PointingHandCursor);
    m_buttonZurueck3->setIcon(QIcon(":/icons/back.png"));
    m_buttonZurueck3->setIconSize(QSize(70, 70));

    m_buttonSaveSettings1 = new QPushButton("Speichern", infoTab);
    m_buttonSaveSettings1->setGeometry(4, 15, 91, 24);

    // Left form layout
    QWidget* leftFormWidget = new QWidget(infoTab);
    leftFormWidget->setGeometry(100, 10, 551, 341);
    QFormLayout* leftForm = new QFormLayout(leftFormWidget);

    m_comboUrModel = new QComboBox();
    m_comboUrModel->addItem("UR10");
    m_comboUrModel->addItem("UR20");
    leftForm->addRow("UR Model: ", m_comboUrModel);

    m_lineEditUrSerial = new QLineEdit();
    leftForm->addRow("UR Serial #: ", m_lineEditUrSerial);

    m_lineEditUrManufDate = new QLineEdit();
    leftForm->addRow("UR Manufacturing Date: ", m_lineEditUrManufDate);

    m_lineEditUrSoftwareVer = new QLineEdit();
    leftForm->addRow("UR Software Ver.: ", m_lineEditUrSoftwareVer);

    m_lineEditUrName = new QLineEdit();
    leftForm->addRow("Pallettierer Name: ", m_lineEditUrName);

    m_lineEditUrStandort = new QLineEdit();
    leftForm->addRow("Pallettierer Standort: ", m_lineEditUrStandort);

    m_lineEditNumPlans = new QLineEdit();
    m_lineEditNumPlans->setEnabled(false);
    leftForm->addRow("Number of Plans: ", m_lineEditNumPlans);

    m_lineEditNumCycles = new QLineEdit();
    m_lineEditNumCycles->setEnabled(false);
    leftForm->addRow("Number of use cycles: ", m_lineEditNumCycles);

    m_lineEditLastRestart = new QLineEdit();
    m_lineEditLastRestart->setEnabled(false);
    leftForm->addRow("last restart: ", m_lineEditLastRestart);

    m_lineEditCurrentVersion = new QLineEdit();
    m_lineEditCurrentVersion->setEnabled(false);
    leftForm->addRow("current version: ", m_lineEditCurrentVersion);

    // Right form layout
    QWidget* rightFormWidget = new QWidget(infoTab);
    rightFormWidget->setGeometry(690, 10, 551, 341);
    QFormLayout* rightForm = new QFormLayout(rightFormWidget);

    m_lineEditDisplayModel = new QLineEdit();
    rightForm->addRow("Display Model: ", m_lineEditDisplayModel);

    m_lineEditDisplayRefreshRate = new QLineEdit();
    m_lineEditDisplayRefreshRate->setEnabled(false);
    rightForm->addRow("Refresh rate: ", m_lineEditDisplayRefreshRate);

    m_lineEditDisplayWidth = new QLineEdit();
    m_lineEditDisplayWidth->setEnabled(false);
    rightForm->addRow("Display width: ", m_lineEditDisplayWidth);

    m_lineEditDisplayHeight = new QLineEdit();
    m_lineEditDisplayHeight->setEnabled(false);
    rightForm->addRow("Display height: ", m_lineEditDisplayHeight);

    m_buttonExitApp = new QPushButton("Restart\nApplication", infoTab);
    m_buttonExitApp->setGeometry(760, 220, 152, 81);

    m_buttonSearchUpdate = new QPushButton("Search Update", infoTab);
    m_buttonSearchUpdate->setGeometry(1010, 220, 152, 81);

    m_settingsTabWidget->addTab(infoTab, "Info");

    // === Admin Tab ===
    QWidget* adminTab = new QWidget();

    m_buttonZurueck4 = new QPushButton(adminTab);
    m_buttonZurueck4->setGeometry(0, 130, 81, 81);
    m_buttonZurueck4->setFlat(true);
    m_buttonZurueck4->setCursor(Qt::PointingHandCursor);
    m_buttonZurueck4->setIcon(QIcon(":/icons/back.png"));
    m_buttonZurueck4->setIconSize(QSize(70, 70));

    m_buttonSaveSettings2 = new QPushButton("Speichern", adminTab);
    m_buttonSaveSettings2->setGeometry(4, 15, 91, 24);

    QWidget* adminFormWidget = new QWidget(adminTab);
    adminFormWidget->setGeometry(100, 10, 551, 341);
    QFormLayout* adminForm = new QFormLayout(adminFormWidget);

    m_lineEditPassword = new QLineEdit();
    m_lineEditPassword->setEchoMode(QLineEdit::PasswordEchoOnEdit);
    m_lineEditPassword->setMaxLength(8);
    adminForm->addRow("Passwort: ", m_lineEditPassword);

    QHBoxLayout* robPathLayout = new QHBoxLayout();
    m_lineEditRobPath = new QLineEdit();
    m_buttonSelectRobPath = new QToolButton();
    m_buttonSelectRobPath->setText("...");
    robPathLayout->addWidget(m_lineEditRobPath);
    robPathLayout->addWidget(m_buttonSelectRobPath);
    adminForm->addRow("Pfad Palettenplaene", robPathLayout);

    QHBoxLayout* audioPathLayout = new QHBoxLayout();
    m_lineEditAudioPath = new QLineEdit();
    m_buttonSelectAudioPath = new QToolButton();
    m_buttonSelectAudioPath->setText("...");
    audioPathLayout->addWidget(m_lineEditAudioPath);
    audioPathLayout->addWidget(m_buttonSelectAudioPath);
    adminForm->addRow("Pfad Audio file", audioPathLayout);

    QHBoxLayout* scannerSoundLayout = new QHBoxLayout();
    m_lineEditScannerSoundPath = new QLineEdit();
    m_buttonSelectScannerSoundPath = new QToolButton();
    m_buttonSelectScannerSoundPath->setText("...");
    scannerSoundLayout->addWidget(m_lineEditScannerSoundPath);
    scannerSoundLayout->addWidget(m_buttonSelectScannerSoundPath);
    adminForm->addRow("Pfad Scanner Warning Sound", scannerSoundLayout);

    m_checkScanner1Overwrite = new QCheckBox("Overwrite Scanner 1");
    m_checkScanner1Overwrite->setLayoutDirection(Qt::RightToLeft);
    adminForm->addRow(m_checkScanner1Overwrite);

    m_checkScanner2Overwrite = new QCheckBox("Overwrite Scanner 2");
    m_checkScanner2Overwrite->setLayoutDirection(Qt::RightToLeft);
    adminForm->addRow(m_checkScanner2Overwrite);

    m_checkScanner3Overwrite = new QCheckBox("Overwrite Scanner 3");
    m_checkScanner3Overwrite->setLayoutDirection(Qt::RightToLeft);
    adminForm->addRow(m_checkScanner3Overwrite);

    // Remote command section
    QWidget* remoteFormWidget = new QWidget(adminTab);
    remoteFormWidget->setGeometry(690, 10, 551, 100);
    QFormLayout* remoteForm = new QFormLayout(remoteFormWidget);

    m_comboRemoteCommand = new QComboBox();
    m_comboRemoteCommand->addItem("power on");
    m_comboRemoteCommand->addItem("brake release");
    m_comboRemoteCommand->addItem("play");
    m_comboRemoteCommand->addItem("stopj(5.0)");

    m_buttonSendCommand = new QPushButton("Send");
    remoteForm->addRow(m_buttonSendCommand, m_comboRemoteCommand);

    m_settingsTabWidget->addTab(adminTab, "Admin");

    // === Editor Tab ===
    QWidget* editorTab = new QWidget();

    m_buttonZurueck5 = new QPushButton(editorTab);
    m_buttonZurueck5->setGeometry(0, 130, 81, 81);
    m_buttonZurueck5->setFlat(true);
    m_buttonZurueck5->setCursor(Qt::PointingHandCursor);
    m_buttonZurueck5->setIcon(QIcon(":/icons/back.png"));
    m_buttonZurueck5->setIconSize(QSize(70, 70));

    m_buttonSaveSettings3 = new QPushButton("Speichern", editorTab);
    m_buttonSaveSettings3->setGeometry(4, 15, 91, 24);

    m_lineEditFilePath = new QLineEdit(editorTab);
    m_lineEditFilePath->setGeometry(130, 9, 1041, 22);

    m_buttonOpenFile = new QPushButton("Open", editorTab);
    m_buttonOpenFile->setGeometry(1176, 8, 75, 24);

    m_textEditFile = new QTextEdit(editorTab);
    m_textEditFile->setGeometry(130, 30, 1121, 331);

    m_settingsTabWidget->addTab(editorTab, "Editor");

    // === Explorer Tab ===
    QWidget* explorerTab = new QWidget();

    m_buttonZurueck6 = new QPushButton(explorerTab);
    m_buttonZurueck6->setGeometry(0, 130, 81, 81);
    m_buttonZurueck6->setFlat(true);
    m_buttonZurueck6->setCursor(Qt::PointingHandCursor);
    m_buttonZurueck6->setIcon(QIcon(":/icons/back.png"));
    m_buttonZurueck6->setIconSize(QSize(70, 70));

    m_buttonSaveSettings4 = new QPushButton("Speichern", explorerTab);
    m_buttonSaveSettings4->setGeometry(4, 15, 91, 24);

    m_treeView = new QTreeView(explorerTab);
    m_treeView->setGeometry(110, 0, 1141, 361);

    // Setup file system model
    QFileSystemModel* fsModel = new QFileSystemModel(this);
    fsModel->setRootPath(QDir::homePath());
    m_treeView->setModel(fsModel);
    m_treeView->setRootIndex(fsModel->index(QDir::homePath()));

    m_settingsTabWidget->addTab(explorerTab, "Explorer");

    // === Console Tab ===
    QWidget* consoleTab = new QWidget();

    m_buttonZurueck7 = new QPushButton(consoleTab);
    m_buttonZurueck7->setGeometry(0, 130, 81, 81);
    m_buttonZurueck7->setFlat(true);
    m_buttonZurueck7->setCursor(Qt::PointingHandCursor);
    m_buttonZurueck7->setIcon(QIcon(":/icons/back.png"));
    m_buttonZurueck7->setIconSize(QSize(70, 70));

    m_textEditConsole = new QTextEdit(consoleTab);
    m_textEditConsole->setGeometry(90, 0, 1151, 331);
    m_textEditConsole->setReadOnly(true);

    m_lineEditCommand = new QLineEdit(consoleTab);
    m_lineEditCommand->setGeometry(90, 330, 1151, 22);
    m_lineEditCommand->setPlaceholderText("Enter command...");

    m_settingsTabWidget->addTab(consoleTab, "Console");

    m_stackedWidget->addWidget(m_settingsPage);
}

void MainWindow::setupExperimentalPage()
{
    m_experimentalPage = new QWidget();

    m_buttonZurueck8 = new QPushButton(m_experimentalPage);
    m_buttonZurueck8->setGeometry(0, 270, 81, 81);
    m_buttonZurueck8->setFlat(true);
    m_buttonZurueck8->setCursor(Qt::PointingHandCursor);
    m_buttonZurueck8->setIcon(QIcon(":/icons/back.png"));
    m_buttonZurueck8->setIconSize(QSize(70, 70));

    QLabel* labelAvailable = new QLabel("Available .rob files", m_experimentalPage);
    labelAvailable->setGeometry(210, 0, 201, 31);

    m_robFilesListWidget = new QListWidget(m_experimentalPage);
    m_robFilesListWidget->setGeometry(210, 30, 181, 641);

    m_buttonDeselectRobFile = new QPushButton("Deselect", m_experimentalPage);
    m_buttonDeselectRobFile->setGeometry(210, 680, 181, 24);

    // Filter group box
    QGroupBox* filterGroup = new QGroupBox(m_experimentalPage);
    filterGroup->setGeometry(10, 30, 194, 101);

    QWidget* filterWidget = new QWidget(filterGroup);
    filterWidget->setGeometry(0, 0, 191, 101);
    QGridLayout* filterLayout = new QGridLayout(filterWidget);

    filterLayout->addWidget(new QLabel("Laenge"), 0, 0);
    m_lineEditFilterLength = new QLineEdit();
    m_lineEditFilterLength->setPlaceholderText("0");
    filterLayout->addWidget(m_lineEditFilterLength, 0, 1);
    filterLayout->addWidget(new QLabel("mm"), 0, 2);

    filterLayout->addWidget(new QLabel("Breite"), 1, 0);
    m_lineEditFilterWidth = new QLineEdit();
    m_lineEditFilterWidth->setPlaceholderText("0");
    filterLayout->addWidget(m_lineEditFilterWidth, 1, 1);
    filterLayout->addWidget(new QLabel("mm"), 1, 2);

    filterLayout->addWidget(new QLabel("Hoehe"), 2, 0);
    m_lineEditFilterHeight = new QLineEdit();
    m_lineEditFilterHeight->setPlaceholderText("0");
    filterLayout->addWidget(m_lineEditFilterHeight, 2, 1);
    filterLayout->addWidget(new QLabel("mm"), 2, 2);

    m_buttonClearFilters = new QPushButton("Clear Filter", m_experimentalPage);
    m_buttonClearFilters->setGeometry(10, 140, 191, 41);

    m_buttonLoadRobFile = new QPushButton(m_experimentalPage);
    m_buttonLoadRobFile->setGeometry(120, 280, 60, 60);
    m_buttonLoadRobFile->setFlat(true);
    m_buttonLoadRobFile->setCursor(Qt::PointingHandCursor);
    m_buttonLoadRobFile->setIcon(QIcon(":/icons/load.png"));
    m_buttonLoadRobFile->setIconSize(QSize(50, 50));

    // 3D visualization frame (placeholder for matplotlib canvas)
    m_matplotlibFrame = new QFrame(m_experimentalPage);
    m_matplotlibFrame->setGeometry(410, 30, 851, 671);
    m_matplotlibFrame->setFrameShape(QFrame::StyledPanel);
    m_matplotlibFrame->setFrameShadow(QFrame::Raised);

    QLabel* vizLabel = new QLabel("3D Visualization", m_matplotlibFrame);
    vizLabel->setAlignment(Qt::AlignCenter);
    vizLabel->setGeometry(0, 0, 851, 671);
    vizLabel->setStyleSheet("color: #666; font-size: 24px;");

    m_stackedWidget->addWidget(m_experimentalPage);
}

void MainWindow::setupConnections()
{
    // Main menu navigation
    connect(m_buttonSettings, &QPushButton::clicked, this, &MainWindow::onSettingsClicked);
    connect(m_buttonVolumeOnOff, &QPushButton::clicked, this, &MainWindow::onVolumeToggleClicked);
    connect(m_buttonExperimental, &QPushButton::clicked, this, &MainWindow::onExperimentalClicked);
    connect(m_buttonParameterRobot, &QPushButton::clicked, this, &MainWindow::onParameterRobotClicked);

    // Main menu actions
    connect(m_buttonLoadPalette, &QPushButton::clicked, this, &MainWindow::onLoadPaletteClicked);
    connect(m_buttonStartServer, &QPushButton::clicked, this, &MainWindow::onStartServerClicked);
    connect(m_inputPalettePlan, &QLineEdit::returnPressed, this, &MainWindow::onLoadPaletteClicked);
    connect(m_checkEinzelpaket, &QCheckBox::stateChanged, this, &MainWindow::onEinzelpaketChanged);
    connect(m_checkLabelInvert, &QCheckBox::stateChanged, this, &MainWindow::onLabelInvertChanged);
    connect(m_inputKartonhoehe, &QLineEdit::editingFinished, this, &MainWindow::onKartonhoeheChanged);
    connect(m_inputKartonGewicht, &QLineEdit::editingFinished, this, &MainWindow::onGewichtChanged);
    connect(m_inputStartlage, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onStartlageChanged);

    // Robot parameters - back buttons
    connect(m_buttonZurueck1, &QPushButton::clicked, this, &MainWindow::showMainMenu);
    connect(m_buttonZurueck2, &QPushButton::clicked, this, &MainWindow::showMainMenu);

    // Robot control buttons
    connect(m_buttonRobotStart, &QPushButton::clicked, this, &MainWindow::onRobotStartClicked);
    connect(m_buttonRobotStop, &QPushButton::clicked, this, &MainWindow::onRobotStopClicked);
    connect(m_buttonRobotPause, &QPushButton::clicked, this, &MainWindow::onRobotPauseClicked);
    connect(m_buttonStopRpcServer, &QPushButton::clicked, this, &MainWindow::onStopRpcServerClicked);

    // Aufnahme tab
    connect(m_inputVerschiebungX, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onVerschiebungXChanged);
    connect(m_inputVerschiebungY, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onVerschiebungYChanged);
    connect(m_checkKlemmung, &QCheckBox::stateChanged, this, &MainWindow::onKlemmungChanged);
    connect(m_buttonAufnahmeServer, &QPushButton::clicked, this, &MainWindow::onAufnahmeServerStart);

    // Settings - back buttons
    connect(m_buttonZurueck3, &QPushButton::clicked, this, &MainWindow::showMainMenu);
    connect(m_buttonZurueck4, &QPushButton::clicked, this, &MainWindow::showMainMenu);
    connect(m_buttonZurueck5, &QPushButton::clicked, this, &MainWindow::showMainMenu);
    connect(m_buttonZurueck6, &QPushButton::clicked, this, &MainWindow::showMainMenu);
    connect(m_buttonZurueck7, &QPushButton::clicked, this, &MainWindow::showMainMenu);

    // Settings actions
    connect(m_buttonSaveSettings1, &QPushButton::clicked, this, &MainWindow::onSaveSettingsClicked);
    connect(m_buttonSaveSettings2, &QPushButton::clicked, this, &MainWindow::onSaveSettingsClicked);
    connect(m_buttonSaveSettings3, &QPushButton::clicked, this, &MainWindow::onSaveSettingsClicked);
    connect(m_buttonSaveSettings4, &QPushButton::clicked, this, &MainWindow::onSaveSettingsClicked);
    connect(m_comboUrModel, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onUrModelChanged);
    connect(m_buttonExitApp, &QPushButton::clicked, this, &MainWindow::onExitAppClicked);
    connect(m_buttonSearchUpdate, &QPushButton::clicked, this, &MainWindow::onSearchUpdateClicked);
    connect(m_buttonSendCommand, &QPushButton::clicked, this, &MainWindow::onSendCommandClicked);
    connect(m_buttonSelectRobPath, &QToolButton::clicked, this, &MainWindow::onSelectRobPathClicked);
    connect(m_buttonSelectAudioPath, &QToolButton::clicked, this, &MainWindow::onSelectAudioPathClicked);
    connect(m_buttonSelectScannerSoundPath, &QToolButton::clicked, this, &MainWindow::onSelectScannerSoundPathClicked);
    connect(m_buttonOpenFile, &QPushButton::clicked, this, &MainWindow::onOpenFileClicked);
    connect(m_lineEditCommand, &QLineEdit::returnPressed, this, &MainWindow::onConsoleCommandEntered);

    // Experimental - back button and actions
    connect(m_buttonZurueck8, &QPushButton::clicked, this, &MainWindow::showMainMenu);
    connect(m_robFilesListWidget, &QListWidget::itemClicked, this, &MainWindow::onRobFileSelected);
    connect(m_buttonDeselectRobFile, &QPushButton::clicked, this, &MainWindow::onDeselectRobFile);
    connect(m_buttonClearFilters, &QPushButton::clicked, this, &MainWindow::onClearFiltersClicked);
    connect(m_buttonLoadRobFile, &QPushButton::clicked, this, &MainWindow::onLoadSelectedRobFile);
    connect(m_lineEditFilterLength, &QLineEdit::textChanged, this, &MainWindow::onFilterChanged);
    connect(m_lineEditFilterWidth, &QLineEdit::textChanged, this, &MainWindow::onFilterChanged);
    connect(m_lineEditFilterHeight, &QLineEdit::textChanged, this, &MainWindow::onFilterChanged);
}

void MainWindow::applyStyleSheet()
{
    // Basic styling
    setStyleSheet(R"(
        QMainWindow {
            background-color: #f0f0f0;
        }
        QPushButton {
            background-color: #e0e0e0;
            border: 1px solid #c0c0c0;
            border-radius: 4px;
            padding: 8px;
        }
        QPushButton:hover {
            background-color: #d0d0d0;
        }
        QPushButton:pressed {
            background-color: #c0c0c0;
        }
        QPushButton:disabled {
            background-color: #f0f0f0;
            color: #a0a0a0;
        }
        QLineEdit, QSpinBox {
            border: 1px solid #c0c0c0;
            border-radius: 4px;
            padding: 4px;
            background-color: white;
        }
        QLineEdit:disabled, QSpinBox:disabled {
            background-color: #f0f0f0;
        }
        QTabWidget::pane {
            border: 1px solid #c0c0c0;
            background-color: white;
        }
        QTabBar::tab {
            background-color: #e0e0e0;
            border: 1px solid #c0c0c0;
            padding: 8px 16px;
        }
        QTabBar::tab:selected {
            background-color: white;
            border-bottom-color: white;
        }
        QCheckBox {
            spacing: 8px;
        }
        QListWidget {
            border: 1px solid #c0c0c0;
            background-color: white;
        }
        QTextEdit {
            border: 1px solid #c0c0c0;
            background-color: white;
        }
    )");
}

void MainWindow::loadSettings()
{
    if (!m_settings) return;

    // Load settings into UI fields
    m_lineEditCurrentVersion->setText("1.0.0");  // TODO: Get from GlobalState
    m_lineEditLastRestart->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));

    // Display info
    QScreen* screen = QApplication::primaryScreen();
    if (screen) {
        m_lineEditDisplayWidth->setText(QString::number(screen->size().width()));
        m_lineEditDisplayHeight->setText(QString::number(screen->size().height()));
        m_lineEditDisplayRefreshRate->setText(QString::number(screen->refreshRate()) + " Hz");
    }

    qDebug() << "MainWindow - settings loaded";
}

void MainWindow::loadRobFileList()
{
    if (!m_database) return;

    m_robFilesListWidget->clear();

    auto files = m_database->listAvailableFiles();
    for (const auto& file : files) {
        m_robFilesListWidget->addItem(file.fileName);
    }

    m_lineEditNumPlans->setText(QString::number(files.size()));

    qDebug() << "MainWindow - loaded" << files.size() << "rob files";
}

void MainWindow::updateEnabledStates()
{
    bool paletteLoaded = m_paletteLoaded;

    // Enable/disable controls based on palette loaded state
    m_inputStartlage->setEnabled(paletteLoaded);
    m_inputKartonhoehe->setEnabled(paletteLoaded);
    m_inputKartonGewicht->setEnabled(paletteLoaded);
    m_checkEinzelpaket->setEnabled(paletteLoaded);
    m_checkLabelInvert->setEnabled(paletteLoaded);
    m_buttonParameterRobot->setEnabled(paletteLoaded);
    m_buttonStartServer->setEnabled(paletteLoaded && !m_serverRunning);

    // Robot controls
    bool serverRunning = m_serverRunning;
    m_buttonRobotStart->setEnabled(serverRunning);
    m_buttonRobotStop->setEnabled(serverRunning);
    m_buttonRobotPause->setEnabled(serverRunning);
    m_buttonStopRpcServer->setEnabled(serverRunning);
}

// === Navigation Slots ===

void MainWindow::showMainMenu()
{
    m_stackedWidget->setCurrentIndex(PAGE_MAIN_MENU);
}

void MainWindow::showRobotParameters()
{
    m_stackedWidget->setCurrentIndex(PAGE_ROBOT_PARAMS);
}

void MainWindow::showSettings()
{
    m_stackedWidget->setCurrentIndex(PAGE_SETTINGS);
}

void MainWindow::showExperimental()
{
    m_stackedWidget->setCurrentIndex(PAGE_EXPERIMENTAL);
}

// === Main Menu Slots ===

void MainWindow::onLoadPaletteClicked()
{
    QString fileName = m_inputPalettePlan->text().trimmed();
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
            m_inputKartonhoehe->setText(QString::number(data->packageDimensions.height));
            m_inputKartonGewicht->setText(QString::number(data->packageDimensions.weight, 'f', 2));
            m_inputStartlage->setMaximum(data->metadata.anzLagen);
            m_checkEinzelpaket->setChecked(data->packageDimensions.einzelpaketLaengs);

            m_labelPaletteInfo->setText(QString("Geladen: %1 - %2 Lagen, %3 Pakete")
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
    m_buttonStartServer->setText("Server laeuft...");
    m_buttonStartServer->setEnabled(false);
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

    // Update icon
    if (m_volumeOn) {
        m_buttonVolumeOnOff->setIcon(QIcon(":/icons/volume-on.png"));
    } else {
        m_buttonVolumeOnOff->setIcon(QIcon(":/icons/volume-off.png"));
    }

    // TODO: Update audio manager
}

void MainWindow::onExperimentalClicked()
{
    showExperimental();
}

void MainWindow::onEinzelpaketChanged(int state)
{
    qDebug() << "Einzelpaket changed:" << (state == Qt::Checked);
    // TODO: Update database
}

void MainWindow::onLabelInvertChanged(int state)
{
    qDebug() << "Label invert changed:" << (state == Qt::Checked);
    // TODO: Update global state
}

void MainWindow::onKartonhoeheChanged()
{
    QString text = m_inputKartonhoehe->text();
    int height = text.toInt();
    qDebug() << "Kartonhoehe changed:" << height;

    if (m_database && !m_currentPaletteFile.isEmpty()) {
        m_database->updateBoxDimensions(m_currentPaletteFile, height, -1.0, -1);
    }
}

void MainWindow::onGewichtChanged()
{
    QString text = m_inputKartonGewicht->text();
    double weight = text.toDouble();
    qDebug() << "Gewicht changed:" << weight;

    if (m_database && !m_currentPaletteFile.isEmpty()) {
        m_database->updateBoxDimensions(m_currentPaletteFile, -1, weight, -1);
    }
}

void MainWindow::onStartlageChanged(int value)
{
    qDebug() << "Startlage changed:" << value;
    // TODO: Update global state
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
    m_buttonStartServer->setText("Server starten");
    updateEnabledStates();

    emit serverStopRequested();
    qDebug() << "Server stop requested";
}

void MainWindow::onVerschiebungXChanged(int value)
{
    qDebug() << "Verschiebung X changed:" << value;
    // TODO: Update global state
}

void MainWindow::onVerschiebungYChanged(int value)
{
    qDebug() << "Verschiebung Y changed:" << value;
    // TODO: Update global state
}

void MainWindow::onKlemmungChanged(int state)
{
    qDebug() << "Klemmung changed:" << (state == Qt::Checked);
    // TODO: Update global state
}

void MainWindow::onAufnahmeServerStart()
{
    onStartServerClicked();
}

// === Settings Slots ===

void MainWindow::onSaveSettingsClicked()
{
    qDebug() << "Save settings clicked";
    // TODO: Save all settings to SettingsManager
    showMessage("Einstellungen gespeichert");
}

void MainWindow::onUrModelChanged(int index)
{
    QString model = m_comboUrModel->currentText();
    qDebug() << "UR Model changed:" << model;
    // TODO: Update settings
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
    QString command = m_comboRemoteCommand->currentText();
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
        m_lineEditRobPath->setText(dir);
    }
}

void MainWindow::onSelectAudioPathClicked()
{
    QString file = QFileDialog::getOpenFileName(this, "Audio-Datei waehlen",
        QString(), "Audio Files (*.wav *.mp3 *.ogg)");
    if (!file.isEmpty()) {
        m_lineEditAudioPath->setText(file);
    }
}

void MainWindow::onSelectScannerSoundPathClicked()
{
    QString file = QFileDialog::getOpenFileName(this, "Scanner Warning Sound waehlen",
        QString(), "Audio Files (*.wav *.mp3 *.ogg)");
    if (!file.isEmpty()) {
        m_lineEditScannerSoundPath->setText(file);
    }
}

void MainWindow::onOpenFileClicked()
{
    QString path = m_lineEditFilePath->text();
    if (path.isEmpty()) {
        path = QFileDialog::getOpenFileName(this, "Datei oeffnen");
        if (path.isEmpty()) return;
        m_lineEditFilePath->setText(path);
    }

    QFile file(path);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_textEditFile->setText(QString::fromUtf8(file.readAll()));
        file.close();
    } else {
        showMessage("Datei konnte nicht geoeffnet werden");
    }
}

void MainWindow::onConsoleCommandEntered()
{
    QString command = m_lineEditCommand->text().trimmed();
    if (command.isEmpty()) return;

    appendConsoleLog("$ " + command);
    m_lineEditCommand->clear();

    // Process console command
    if (command == "clear") {
        m_textEditConsole->clear();
    } else if (command == "help") {
        appendConsoleLog("Available commands: clear, help, status, version");
    } else if (command == "status") {
        appendConsoleLog("Server: " + QString(m_serverRunning ? "running" : "stopped"));
        appendConsoleLog("Palette: " + QString(m_paletteLoaded ? m_currentPaletteFile : "none"));
    } else if (command == "version") {
        appendConsoleLog("MultipackParser C++ v1.0.0");
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

    m_inputPalettePlan->setText(fileName);
}

void MainWindow::onDeselectRobFile()
{
    m_robFilesListWidget->clearSelection();
}

void MainWindow::onFilterChanged()
{
    if (!m_database) return;

    int length = m_lineEditFilterLength->text().toInt();
    int width = m_lineEditFilterWidth->text().toInt();
    int height = m_lineEditFilterHeight->text().toInt();

    if (length == 0 && width == 0 && height == 0) {
        loadRobFileList();
        return;
    }

    auto matching = m_database->findByPackageDimensions(length, width, height);

    m_robFilesListWidget->clear();
    for (const auto& name : matching) {
        m_robFilesListWidget->addItem(name);
    }
}

void MainWindow::onClearFiltersClicked()
{
    m_lineEditFilterLength->clear();
    m_lineEditFilterWidth->clear();
    m_lineEditFilterHeight->clear();
    loadRobFileList();
}

void MainWindow::onLoadSelectedRobFile()
{
    auto items = m_robFilesListWidget->selectedItems();
    if (items.isEmpty()) {
        showMessage("Bitte Datei auswaehlen");
        return;
    }

    m_inputPalettePlan->setText(items.first()->text());
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
        m_lineEditUrSerial->setText(m_robot->getSerialNumber());
        m_lineEditUrSoftwareVer->setText(m_robot->getSoftwareVersion());
    }
}

void MainWindow::updatePaletteInfo()
{
    // Called when palette data changes
    loadRobFileList();
}

void MainWindow::showMessage(const QString& message)
{
    m_labelGewichtInfo->setText(message);
    qDebug() << "Message:" << message;
}

void MainWindow::appendConsoleLog(const QString& text)
{
    m_textEditConsole->append(text);
}

} // namespace ui
} // namespace multipack
