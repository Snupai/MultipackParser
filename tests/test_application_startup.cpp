#include "TestHelpers.h"

#include "multipack/config/SettingsManager.h"
#include "multipack/core/AppInitializer.h"
#include "multipack/core/Application.h"
#include "multipack/network/XmlRpcServer.h"

#include <QCoreApplication>
#include <QDir>
#include <QLibraryInfo>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using multipack::config::SettingsManager;
using multipack::core::Application;

class ApplicationStartupTest : public QObject
{
    Q_OBJECT

private slots:
    void xmlRpcServerAutoStartsWithoutUiClick();
};

void ApplicationStartupTest::xmlRpcServerAutoStartsWithoutUiClick()
{
    auto* app = qobject_cast<Application*>(QCoreApplication::instance());
    QVERIFY(app != nullptr);

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QVERIFY(QDir(tempDir.path()).mkpath("usb"));

    SettingsManager settings;
    settings.resetToDefaults();
    const quint16 port = testhelpers::findFreePort();
    settings.setXmlRpcPort(static_cast<int>(port));
    settings.setXmlRpcAutoStart(true);
    settings.setRobotIp(QString());
    settings.setDatabasePath(tempDir.filePath("palettes.db"));
    settings.setUsbPath(tempDir.filePath("usb"));
    QVERIFY(settings.save(tempDir.filePath("settings.json")));

    const QString oldCurrentPath = QDir::currentPath();
    QVERIFY(QDir::setCurrent(tempDir.path()));

    QVERIFY(app->initialize());
    QVERIFY(app->initializer() != nullptr);
    auto* server = app->initializer()->xmlRpcServer();
    QVERIFY(server != nullptr);
    QVERIFY(server->isRunning());
    QCOMPARE(server->port(), static_cast<int>(port));

    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, port);
    QVERIFY(socket.waitForConnected(5000));
    socket.disconnectFromHost();

    QVERIFY(QDir::setCurrent(oldCurrentPath));
}

int main(int argc, char** argv)
{
    Q_INIT_RESOURCE(multipack);
    Q_INIT_RESOURCE(MainWindowResources);

#ifdef Q_OS_WIN
    qputenv("QT_QPA_PLATFORM", "windows");
#else
    qputenv("QT_QPA_PLATFORM", "offscreen");
#endif
    qputenv("QT_PLUGIN_PATH", QDir::toNativeSeparators(QLibraryInfo::path(QLibraryInfo::PluginsPath)).toUtf8());
    qputenv("MULTIPACK_DISABLE_ROBOT_STATUS_MONITOR", "1");
    qputenv("MULTIPACK_FULLSCREEN", "0");

    Application app(argc, argv);
    ApplicationStartupTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_application_startup.moc"
