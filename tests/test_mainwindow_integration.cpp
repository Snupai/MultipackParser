#include "TestHelpers.h"

#include "multipack/config/ConfigDefaults.h"
#include "multipack/config/SettingsManager.h"
#include "multipack/core/GlobalState.h"
#include "multipack/database/DatabaseManager.h"
#include "multipack/network/XmlRpcServer.h"
#include "multipack/ui/MainWindow.h"

#include <QApplication>
#include <QDir>
#include <QLibraryInfo>
#include <QLineEdit>
#include <QLabel>
#include <QSpinBox>
#include <QListView>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using multipack::config::SettingsManager;
using multipack::core::GlobalState;
using multipack::database::DatabaseManager;
using multipack::database::SaveResult;
using multipack::network::XmlRpcServer;
using multipack::ui::MainWindow;

class MainWindowIntegrationTest : public QObject
{
    Q_OBJECT

private slots:
    void startAndStopServerFromUi();
    void failedPaletteLoadKeepsSuggestionsVisible();
    void successfulPaletteLoadClosesSuggestions();
    void selectedStartLayerReachesRpcAfterRobotStart();
};

void MainWindowIntegrationTest::startAndStopServerFromUi()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    qputenv("MULTIPACK_DISABLE_ROBOT_STATUS_MONITOR", "1");

    GlobalState::instance().clear();

    DatabaseManager database;
    QVERIFY(database.open(tempDir.filePath("palettes.db")));
    QCOMPARE(database.savePaletteData(testhelpers::makeSamplePaletteData()), SaveResult::Inserted);

    SettingsManager settings;
    settings.resetToDefaults();
    settings.setValue(multipack::config::Keys::SERVER_PORT, static_cast<int>(testhelpers::findFreePort()));

    XmlRpcServer server;
    server.setDatabaseManager(&database);
    server.setGlobalState(&GlobalState::instance());
    server.registerStandardMethods();

    MainWindow window;
    window.setSettingsManager(&settings);
    window.setDatabaseManager(&database);
    window.setGlobalState(&GlobalState::instance());
    window.setXmlRpcServer(&server);
    window.show();

    auto* paletteInput = window.findChild<QLineEdit*>("EingabePallettenplan");
    auto* loadButton = window.findChild<QPushButton*>("LadePallettenplan");
    auto* startButton = window.findChild<QPushButton*>("ButtonDatenSenden");
    auto* stopButton = window.findChild<QPushButton*>("ButtonStopRPCServer");

    QVERIFY(paletteInput != nullptr);
    QVERIFY(loadButton != nullptr);
    QVERIFY(startButton != nullptr);
    QVERIFY(stopButton != nullptr);

    paletteInput->setText("sample");
    QTest::mouseClick(loadButton, Qt::LeftButton);
    QTRY_VERIFY(startButton->isEnabled());

    QTest::mouseClick(startButton, Qt::LeftButton);
    QTRY_VERIFY(server.isRunning());

    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, static_cast<quint16>(server.port()));
    QVERIFY(socket.waitForConnected(5000));
    socket.disconnectFromHost();

    QTest::mouseClick(stopButton, Qt::LeftButton);
    QTRY_VERIFY(!server.isRunning());

    window.close();
    QTRY_VERIFY(!window.isVisible());
}


void MainWindowIntegrationTest::failedPaletteLoadKeepsSuggestionsVisible()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    qputenv("MULTIPACK_DISABLE_ROBOT_STATUS_MONITOR", "1");

    DatabaseManager database;
    QVERIFY(database.open(tempDir.filePath("palettes.db")));
    QCOMPARE(database.savePaletteData(testhelpers::makeSamplePaletteData()), SaveResult::Inserted);

    MainWindow window;
    window.setDatabaseManager(&database);
    window.show();

    auto* paletteInput = window.findChild<QLineEdit*>("EingabePallettenplan");
    auto* loadButton = window.findChild<QPushButton*>("LadePallettenplan");
    auto* popup = window.findChild<QListView*>("PalettePlanCompletionPopup");

    QVERIFY(paletteInput != nullptr);
    QVERIFY(loadButton != nullptr);
    QVERIFY(popup != nullptr);

    paletteInput->setText("sam");
    QTRY_VERIFY(popup->isVisible());

    // Simulate the plan disappearing/failing to load after suggestions were
    // already presented. Block the database change signal so the popup model
    // remains unchanged and we specifically exercise the failed-load UI path.
    {
        const QSignalBlocker blocker(&database);
        QVERIFY(database.deletePalette("sample.rob"));
    }

    QTest::mouseClick(loadButton, Qt::LeftButton);
    QTRY_VERIFY(popup->isVisible());
}

void MainWindowIntegrationTest::successfulPaletteLoadClosesSuggestions()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    qputenv("MULTIPACK_DISABLE_ROBOT_STATUS_MONITOR", "1");

    GlobalState::instance().clear();

    DatabaseManager database;
    QVERIFY(database.open(tempDir.filePath("palettes.db")));
    QCOMPARE(database.savePaletteData(testhelpers::makeSamplePaletteData()), SaveResult::Inserted);

    MainWindow window;
    window.setDatabaseManager(&database);
    window.setGlobalState(&GlobalState::instance());
    window.show();

    auto* paletteInput = window.findChild<QLineEdit*>("EingabePallettenplan");
    auto* loadButton = window.findChild<QPushButton*>("LadePallettenplan");
    auto* popup = window.findChild<QListView*>("PalettePlanCompletionPopup");

    QVERIFY(paletteInput != nullptr);
    QVERIFY(loadButton != nullptr);
    QVERIFY(popup != nullptr);

    paletteInput->setText("sample");
    QTRY_VERIFY(popup->isVisible());

    QTest::mouseClick(loadButton, Qt::LeftButton);
    QTRY_VERIFY(!popup->isVisible());
}

void MainWindowIntegrationTest::selectedStartLayerReachesRpcAfterRobotStart()
{
    QTemporaryDir tempDir;
    qputenv("MULTIPACK_DISABLE_ROBOT_STATUS_MONITOR", "1");
    auto& state = GlobalState::instance();
    state.clear();
    DatabaseManager database;
    QVERIFY(database.open(tempDir.filePath("palettes.db")));
    auto palette = testhelpers::makeSamplePaletteData();
    palette.metadata.anzLagen = 2;
    palette.layerAssignments = {1, 1};
    palette.intermediaryLayers = {0, 0};
    QCOMPARE(database.savePaletteData(palette), SaveResult::Inserted);
    MainWindow window;
    window.setDatabaseManager(&database);
    window.setGlobalState(&state);
    auto* input = window.findChild<QLineEdit*>("EingabePallettenplan");
    auto* load = window.findChild<QPushButton*>("LadePallettenplan");
    auto* startLayer = window.findChild<QSpinBox*>("EingabeStartlage");
    QVERIFY(input && load && startLayer);
    const QStringList labels = {"label_Startlage", "label_Kartonhoehe", "label_Kartonhoehe_mm",
                                "label_Gewicht", "label_Gewicht_kg"};
    for (const auto& name : labels) {
        auto* label = window.findChild<QLabel*>(name);
        QVERIFY(label);
        QVERIFY(!label->isEnabled());
    }
    input->setText("sample");
    load->click();
    for (const auto& name : labels) QVERIFY(window.findChild<QLabel*>(name)->isEnabled());
    startLayer->setValue(2);
    QCOMPARE(state.startLayer(), 2);
    // Reproduce a stale state write after valueChanged. The robot-start slot must
    // commit the visible value again even without an available physical robot.
    state.setStartLayer(1);
    QVERIFY(QMetaObject::invokeMethod(&window, "onRobotStartClicked", Qt::DirectConnection));
    QCOMPARE(state.startLayer(), 2);
    XmlRpcServer server;
    server.setGlobalState(&state);
    server.registerStandardMethods();
    QVERIFY(server.start(0));
    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, server.port());
    QVERIFY(socket.waitForConnected(1000));
    const QByteArray body = "<methodCall><methodName>UR_Startlage</methodName></methodCall>";
    socket.write("POST /RPC2 HTTP/1.1\r\nHost: localhost\r\nContent-Length: "
                 + QByteArray::number(body.size()) + "\r\n\r\n" + body);
    QTRY_VERIFY_WITH_TIMEOUT(socket.bytesAvailable() > 0, 3000);
    const auto reply = socket.readAll();
    QVERIFY2(reply.contains("<int>2</int>"), reply.constData());
    QVERIFY(!reply.contains("<fault>"));
}

int main(int argc, char** argv)
{
#ifdef Q_OS_WIN
    qputenv("QT_QPA_PLATFORM", "windows");
#else
    qputenv("QT_QPA_PLATFORM", "offscreen");
#endif
    qputenv("QT_PLUGIN_PATH", QDir::toNativeSeparators(QLibraryInfo::path(QLibraryInfo::PluginsPath)).toUtf8());

    QApplication app(argc, argv);
    MainWindowIntegrationTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_mainwindow_integration.moc"
