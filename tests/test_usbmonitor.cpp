#include "TestHelpers.h"

#include "multipack/database/DatabaseManager.h"
#include "multipack/system/UsbMonitor.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using multipack::database::DatabaseManager;
using multipack::system::UsbMonitor;

class UsbMonitorTest : public QObject
{
    Q_OBJECT

private slots:
    void unchangedFileIsNotMarkedFailed();
    void startMonitoringImportsExistingFiles();
    void folderCreatedAfterMonitoringStartsIsImported();
    void newFileAddedWhileMonitoringIsImported();
    void destroyingMonitorWaitsForAsyncUpdate();
};

void UsbMonitorTest::unchangedFileIsNotMarkedFailed()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString usbDir = tempDir.filePath("usb");
    const QString dbPath = tempDir.filePath("palettes.db");
    QVERIFY(testhelpers::writeSampleRobFile(usbDir, "sample.rob"));

    UsbMonitor monitor(usbDir, dbPath);
    QCOMPARE(monitor.updateDatabaseFromUsb(), 1);
    QCOMPARE(monitor.updateDatabaseFromUsb(), 0);
    QVERIFY(monitor.getFailedFiles().isEmpty());

    DatabaseManager database;
    QVERIFY(database.open(dbPath));
    QCOMPARE(database.listAvailableFiles().size(), 1);
}

void UsbMonitorTest::startMonitoringImportsExistingFiles()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString usbDir = tempDir.filePath("usb");
    const QString dbPath = tempDir.filePath("palettes.db");
    QVERIFY(testhelpers::writeSampleRobFile(usbDir, "startup.rob"));

    UsbMonitor monitor(usbDir, dbPath);
    QSignalSpy completedSpy(&monitor, &UsbMonitor::databaseUpdateCompleted);
    QVERIFY(monitor.startMonitoring());
    QTRY_VERIFY_WITH_TIMEOUT(completedSpy.count() > 0, 5000);

    DatabaseManager database;
    QVERIFY(database.open(dbPath));
    QCOMPARE(database.listAvailableFiles().size(), 1);
}

void UsbMonitorTest::folderCreatedAfterMonitoringStartsIsImported()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString usbDir = tempDir.filePath("usb");
    const QString dbPath = tempDir.filePath("palettes.db");

    UsbMonitor monitor(usbDir, dbPath);
    QSignalSpy completedSpy(&monitor, &UsbMonitor::databaseUpdateCompleted);
    QVERIFY(monitor.startMonitoring());

    QVERIFY(testhelpers::writeSampleRobFile(usbDir, "mounted-later.rob"));
    QVERIFY(QMetaObject::invokeMethod(&monitor, "processChanges", Qt::DirectConnection));
    QTRY_VERIFY_WITH_TIMEOUT(completedSpy.count() > 0, 5000);

    DatabaseManager database;
    QVERIFY(database.open(dbPath));
    QCOMPARE(database.listAvailableFiles().size(), 1);
}

void UsbMonitorTest::newFileAddedWhileMonitoringIsImported()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString usbDir = tempDir.filePath("usb");
    const QString dbPath = tempDir.filePath("palettes.db");
    QVERIFY(QDir().mkpath(usbDir));

    UsbMonitor monitor(usbDir, dbPath);
    QSignalSpy completedSpy(&monitor, &UsbMonitor::databaseUpdateCompleted);
    QVERIFY(monitor.startMonitoring());

    QVERIFY(testhelpers::writeSampleRobFile(usbDir, "added-later.rob"));
    QVERIFY(QMetaObject::invokeMethod(&monitor, "processChanges", Qt::DirectConnection));
    QTRY_VERIFY_WITH_TIMEOUT(completedSpy.count() > 0, 5000);

    DatabaseManager database;
    QVERIFY(database.open(dbPath));
    QCOMPARE(database.listAvailableFiles().size(), 1);
}

void UsbMonitorTest::destroyingMonitorWaitsForAsyncUpdate()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString usbDir = tempDir.filePath("usb");
    const QString dbPath = tempDir.filePath("palettes.db");
    for (int i = 0; i < 8; ++i) {
        QVERIFY(testhelpers::writeSampleRobFile(usbDir, QString("sample-%1.rob").arg(i)));
    }

    QElapsedTimer timer;
    timer.start();

    auto* monitor = new UsbMonitor(usbDir, dbPath);
    monitor->updateDatabaseFromUsbAsync();
    delete monitor;

    QVERIFY(timer.elapsed() < 10000);

    DatabaseManager database;
    QVERIFY(database.open(dbPath));
    QVERIFY(!database.listAvailableFiles().isEmpty());
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    UsbMonitorTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_usbmonitor.moc"
