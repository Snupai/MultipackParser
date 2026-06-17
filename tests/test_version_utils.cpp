#include "multipack/utils/VersionUtils.h"

#include <QCoreApplication>
#include <QtTest/QtTest>

using multipack::utils::VersionUtils::compare;
using multipack::utils::VersionUtils::normalize;

class VersionUtilsTest : public QObject
{
    Q_OBJECT

private slots:
    void normalizesReleaseTags();
    void comparesSemanticVersions_data();
    void comparesSemanticVersions();
};

void VersionUtilsTest::normalizesReleaseTags()
{
    QCOMPARE(normalize("v2.0.0-beta"), QString("2.0.0-beta"));
    QCOMPARE(normalize(" V1.1.0 "), QString("1.1.0"));
}

void VersionUtilsTest::comparesSemanticVersions_data()
{
    QTest::addColumn<QString>("left");
    QTest::addColumn<QString>("right");
    QTest::addColumn<int>("expected");

    QTest::newRow("beta after alpha") << "2.0.0-beta" << "2.0.0-alpha" << 1;
    QTest::newRow("rc after beta") << "2.0.0-rc.1" << "2.0.0-beta" << 1;
    QTest::newRow("final after beta") << "2.0.0" << "2.0.0-beta" << 1;
    QTest::newRow("final after rc") << "2.0.0" << "2.0.0-rc.1" << 1;
    QTest::newRow("patch after final") << "2.0.1" << "2.0.0" << 1;
    QTest::newRow("numeric prerelease order") << "2.0.0-beta.2" << "2.0.0-beta.11" << -1;
    QTest::newRow("longer prerelease order") << "2.0.0-beta" << "2.0.0-beta.1" << -1;
    QTest::newRow("build metadata ignored") << "2.0.0+build.5" << "2.0.0" << 0;
    QTest::newRow("tag prefix ignored") << "v2.0.0-beta" << "2.0.0-beta" << 0;
}

void VersionUtilsTest::comparesSemanticVersions()
{
    QFETCH(QString, left);
    QFETCH(QString, right);
    QFETCH(int, expected);

    QCOMPARE(compare(left, right), expected);
    QCOMPARE(compare(right, left), -expected);
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    VersionUtilsTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_version_utils.moc"
