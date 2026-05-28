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
#include <QPushButton>
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
