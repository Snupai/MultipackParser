#include "TestHelpers.h"

#include "multipack/config/ConfigDefaults.h"
#include "multipack/config/SettingsManager.h"
#include "multipack/database/DatabaseManager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using multipack::config::SettingsManager;
using multipack::database::DatabaseManager;
using multipack::database::FileInfo;
using multipack::database::SaveResult;

class SettingsAndDatabaseTest : public QObject
{
    Q_OBJECT

private slots:
    void nestedSettingsPersist();
    void adminPasswordDefaultsToLegacyValue();
    void savePaletteDataReportsOutcomes();
    void timestampFormattingUsesMilliseconds();
};

void SettingsAndDatabaseTest::nestedSettingsPersist()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString settingsPath = tempDir.filePath("settings.json");

    SettingsManager settings;
    settings.resetToDefaults();
    settings.setValue(multipack::config::Keys::DISPLAY_SPECS_MODEL, "Panel-X");
    settings.setValue(multipack::config::Keys::DISPLAY_SPECS_WIDTH, 1920);
    settings.setValue("aufnahme.verschiebung_x", 42);
    settings.setValue("aufnahme.verschiebung_y", -17);
    QVERIFY(settings.save(settingsPath));

    SettingsManager loaded;
    QVERIFY(loaded.load(settingsPath));
    QCOMPARE(loaded.value(multipack::config::Keys::DISPLAY_SPECS_MODEL).toString(), QString("Panel-X"));
    QCOMPARE(loaded.value(multipack::config::Keys::DISPLAY_SPECS_WIDTH).toInt(), 1920);
    QCOMPARE(loaded.value("aufnahme.verschiebung_x").toInt(), 42);
    QCOMPARE(loaded.value("aufnahme.verschiebung_y").toInt(), -17);
}

void SettingsAndDatabaseTest::adminPasswordDefaultsToLegacyValue()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString settingsPath = tempDir.filePath("settings.json");

    SettingsManager settings;
    settings.resetToDefaults();
    QVERIFY(settings.hasAdminPassword());
    QVERIFY(settings.verifyAdminPassword("666666"));
    QVERIFY(settings.save(settingsPath));

    SettingsManager loaded;
    QVERIFY(loaded.load(settingsPath));
    QVERIFY(loaded.hasAdminPassword());
    QVERIFY(loaded.verifyAdminPassword("666666"));
    QVERIFY(!loaded.value(multipack::config::Keys::ADMIN_PASSWORD_HASH).toString().isEmpty());
}

void SettingsAndDatabaseTest::savePaletteDataReportsOutcomes()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    DatabaseManager database;
    QVERIFY(database.open(tempDir.filePath("palettes.db")));

    auto sample = testhelpers::makeSamplePaletteData("article-100.rob", 1735689600000);
    QCOMPARE(database.savePaletteData(sample), SaveResult::Inserted);
    QCOMPARE(database.savePaletteData(sample), SaveResult::Unchanged);

    sample.metadata.fileTimestamp += 1000;
    QCOMPARE(database.savePaletteData(sample), SaveResult::Updated);
}

void SettingsAndDatabaseTest::timestampFormattingUsesMilliseconds()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    DatabaseManager database;
    QVERIFY(database.open(tempDir.filePath("palettes.db")));

    const qint64 timestampMs = 1735689600123;
    const auto sample = testhelpers::makeSamplePaletteData("timed.rob", timestampMs);
    QCOMPARE(database.savePaletteData(sample), SaveResult::Inserted);

    const QVector<FileInfo> files = database.listAvailableFiles();
    QCOMPARE(files.size(), 1);
    QCOMPARE(files.first().timestamp, timestampMs);
    QCOMPARE(files.first().timestampStr,
             QDateTime::fromMSecsSinceEpoch(timestampMs).toString("yyyy-MM-dd hh:mm:ss"));
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    SettingsAndDatabaseTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_settings_database.moc"
