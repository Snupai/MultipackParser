#include "multipack/system/SystemUpdaterClient.h"
#include "multipack/system/WifiSsidProbe.h"
#include "multipack/system/AutoUpdater.h"
#include <QLocalServer>
#include <QLocalSocket>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QtTest/QtTest>
using namespace multipack::system;
class SystemClientsTest : public QObject {
    Q_OBJECT
private slots:
    void versionBoundary() {
        QVERIFY(SystemUpdaterClient::validVersion("v2.0.0-rc_1"));
        QVERIFY(SystemUpdaterClient::validVersion(QString(80, 'a')));
        for (const QString& v : {QString(), QString("../root"), QString("https://host"),
                                QString("x\n"), QString("x;reboot"), QString(81, 'a')})
            QVERIFY(!SystemUpdaterClient::validVersion(v));
    }
    void fakeSocketInstallAndFragmentedStatus() {
        QTemporaryDir dir;
        QLocalServer server;
        QVERIFY(server.listen(dir.filePath("control.sock")));
        SystemUpdaterClient client(nullptr, server.fullServerName());
        QSignalSpy replies(&client, &SystemUpdaterClient::response);
        QSignalSpy errors(&client, &SystemUpdaterClient::error);
        QVERIFY(client.install("v2.0"));
        QVERIFY(!client.status()); // Never overlap requests.
        QTRY_VERIFY(server.hasPendingConnections());
        auto* peer = server.nextPendingConnection();
        QTRY_VERIFY(peer->canReadLine());
        const auto request = QJsonDocument::fromJson(peer->readLine()).object();
        QCOMPARE(request, (QJsonObject{{"op", "install"}, {"version", "v2.0"}}));
        peer->write("{\"accepted\":true}\n"); peer->flush();
        QTRY_COMPARE(replies.count(), 1);
        QVERIFY(client.status());
        QTRY_VERIFY(server.hasPendingConnections());
        auto* statusPeer = server.nextPendingConnection();
        QTRY_VERIFY(statusPeer->canReadLine());
        QCOMPARE(QJsonDocument::fromJson(statusPeer->readLine()).object(), (QJsonObject{{"op", "status"}}));
        statusPeer->write("{\"phase\":"); statusPeer->flush();
        QTest::qWait(20);
        QCOMPARE(replies.count(), 1);
        statusPeer->write("\"installing\",\"percent\":42}\n"); statusPeer->flush();
        QTRY_COMPARE(replies.count(), 2);
        QCOMPARE(errors.count(), 0);
    }
    void failures_data() {
        QTest::addColumn<QByteArray>("reply");
        QTest::newRow("daemon error") << QByteArray("{\"error\":\"update already running\"}\n");
        QTest::newRow("malformed") << QByteArray("bad json\n");
        QTest::newRow("schema") << QByteArray("{}\n");
        QTest::newRow("oversized") << QByteArray(65537, 'x');
        QTest::newRow("timeout") << QByteArray();
    }
    void failures() {
        QFETCH(QByteArray, reply);
        QTemporaryDir dir;
        QLocalServer server;
        QVERIFY(server.listen(dir.filePath("control.sock")));
        SystemUpdaterClient client(nullptr, server.fullServerName(), 200);
        QSignalSpy errors(&client, &SystemUpdaterClient::error);
        QVERIFY(client.status());
        QTRY_VERIFY(server.hasPendingConnections());
        auto* peer = server.nextPendingConnection();
        QTRY_VERIFY(peer->canReadLine()); peer->readLine();
        peer->write(reply); peer->flush();
        QTRY_COMPARE(errors.count(), 1);
        QVERIFY(client.status()); // Retry is safe after failure.
        client.cancel();
    }
    void missingSocketAndClose() {
        QTemporaryDir dir;
        SystemUpdaterClient client(nullptr, dir.filePath("absent"));
        QSignalSpy errors(&client, &SystemUpdaterClient::error);
        QVERIFY(client.status());
        QTRY_COMPARE(errors.count(), 1);
        QLocalServer server;
        QVERIFY(server.listen(dir.filePath("control.sock")));
        auto* closing = new SystemUpdaterClient(nullptr, server.fullServerName());
        QVERIFY(closing->status());
        QTRY_VERIFY(server.hasPendingConnections());
        delete closing; // Pending socket is owned and cancelled on destruction.
    }
    void wifiParsingAndMissingUtility() {
        QCOMPARE(WifiSsidProbe::parse("no:other\nyes: A:B\\C \n"), QString(" A:B\\C "));
        WifiSsidProbe probe(nullptr, "/missing/nmcli");
        QSignalSpy result(&probe, &WifiSsidProbe::result);
        probe.refresh();
        QTRY_COMPARE(result.count(), 1);
        QVERIFY(result.first().first().toString().contains("unavailable"));
    }
#ifndef Q_OS_WIN
    void slowToolDoesNotBlockEventLoop() {
        QTemporaryDir dir;
        QFile script(dir.filePath("slow-nmcli"));
        QVERIFY(script.open(QIODevice::WriteOnly));
        script.write("#!/bin/sh\nexec sleep 10\n"); script.close();
        QVERIFY(script.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        WifiSsidProbe probe(nullptr, script.fileName(), 100);
        QSignalSpy result(&probe, &WifiSsidProbe::result);
        bool heartbeat = false;
        QTimer::singleShot(10, &probe, [&] { heartbeat = true; });
        probe.refresh();
        QTRY_VERIFY_WITH_TIMEOUT(heartbeat, 90);
        QTRY_COMPARE(result.count(), 1);
        QVERIFY(result.first().first().toString().contains("timeout"));
    }
#endif
    void immutableRejectsLegacyUsbPath() {
        qputenv("MULTIPACK_IMMUTABLE", "1");
        AutoUpdater updater;
        QSignalSpy errors(&updater, &AutoUpdater::checkFailed);
        QVERIFY(!updater.checkForUsbUpdates("/any/path"));
        QCOMPARE(errors.count(), 1);
        qunsetenv("MULTIPACK_IMMUTABLE");
    }
};
QTEST_GUILESS_MAIN(SystemClientsTest)
#include "test_system_clients.moc"
