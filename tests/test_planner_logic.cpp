#include "TestHelpers.h"

#include "multipack/config/SettingsManager.h"
#include "multipack/database/DatabaseManager.h"
#include "multipack/planner/PlannerConfig.h"
#include "multipack/planner/PlannerPlanList.h"
#include "multipack/planner/PlannerProtocol.h"
#include "multipack/planner/PlannerStore.h"
#include "multipack/planner/SyncBackoff.h"
#include "multipack/system/RobFileParser.h"
#include "multipack/ui/QrCodeWidget.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QtTest/QtTest>

using namespace multipack::planner;

namespace {

const QString kHmiId = QStringLiteral("2f7c1c0e-4c1d-4d8e-9a57-0e5d1c9b7a10");
const QString kToken = QStringLiteral("q9bXo3Y2vT7c1Lk0pWmZr8sNfHq2uYbEo4xJd6gKa_-");
const QString kFinalA = QStringLiteral("8d0a6f0e-4b52-4f8f-a1f4-5b9f6f0f2c11");
const QString kFinalB = QStringLiteral("9e1b7f1f-5c63-4a9f-b2a5-6caf7f1f3d22");

// A test CA certificate (public part only; its key was discarded).
const char* kCaPem = R"PEM(-----BEGIN CERTIFICATE-----
MIIBnDCCAUGgAwIBAgIUY9LOTQFUPU3BH3c61FlR9TR/jewwCgYIKoZIzj0EAwIw
IjEgMB4GA1UEAwwXTXVsdGlwYWNrUGFyc2VyIFRlc3QgQ0EwIBcNMjYxMTIwMTE0
MzUwWhgPMjEyNjEwMjcxMTQzNTBaMCIxIDAeBgNVBAMMF011bHRpcGFja1BhcnNl
ciBUZXN0IENBMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEP/i+EDLjVJAtsGZ7
Ay1HNU/+80COPUePIcfgLW3dGQt30fv74ymjXyVqHk9VRvZGBouJ689/zasuiKz7
SSt0C6NTMFEwHQYDVR0OBBYEFMLQSybN+pBW2j8R961V5SHRJ1sSMB8GA1UdIwQY
MBaAFMLQSybN+pBW2j8R961V5SHRJ1sSMA8GA1UdEwEB/wQFMAMBAf8wCgYIKoZI
zj0EAwIDSQAwRgIhAM8Fd1+KIkdbf88gWb65WxBlGt/PiPY6FtDp683XZv2QAiEA
xnovisYqfxv+14e98yYq00i+Ed4S71/9XRJLbf76m5s=
-----END CERTIFICATE-----
)PEM";

QByteArray sampleRobCrlf()
{
    QByteArray bytes = testhelpers::sampleRobFileContents().toUtf8();
    bytes.replace("\n", "\r\n");
    return bytes;
}

FinalEntry entryFor(const QString& id, const QString& recordId, const QString& name,
                    const QByteArray& bytes, const QDateTime& markedAt = QDateTime())
{
    FinalEntry entry;
    entry.id = id;
    entry.recordId = recordId;
    entry.name = name;
    entry.fileName = name + QStringLiteral(".rob");
    entry.sha256 = sha256Hex(bytes);
    entry.markedAt = markedAt.isValid() ? markedAt : QDateTime(QDate(2026, 11, 19), QTime(8, 30), QTimeZone::utc());
    return entry;
}

bool writeFile(const QString& path, const QByteArray& data)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}

} // namespace

class PlannerLogicTest : public QObject
{
    Q_OBJECT

private slots:
    // Configuration
    void configAcceptsHttpsAndNormalizes();
    void configRejectsPlainHttpToRemoteHosts();
    void configRejectsBadTokenAndHmiId();
    void configRoundTripsThroughSettings();
    void importCopiesCaAndRejectsBadPem();
    void importFindsOnlyHmiConfigFiles();

    // Wire format
    void parsesFinalsAndSkipsInvalidEntries();
    void parsesProposal();
    void buildsStatusBodyWithNulls();
    void sha256MatchesKnownVectorCaseInsensitive();
    void parsesRetryAfter();
    void classifiesResponses();

    // Backoff
    void backoffDoublesWithJitterAndCaps();
    void backoffHonoursRetryAfterAndNotAuthorized();

    // Parser + store
    void parserAcceptsBytesWithAnyLineEnding();
    void parserAcceptsAnyWhitespace();
    void storeRecordsAndClearsLoadErrors();
    void storeVerifiesShaAndParserBeforeStoring();
    void storeKeepsWithdrawnFinalsAndBytes();
    void plannerRowsInvisibleToUsbLookups();

    // HMI list + guard
    void planListHidesWithdrawnExceptActive();
    void planListFlagsNewVersionOfActivePlan();
    void switchGuardBlocksRunningOrUnknownRobot();

    // QR
    void qrCodeEncodesOnlyThePageUrl();
};

void PlannerLogicTest::configAcceptsHttpsAndNormalizes()
{
    const QByteArray json = QJsonDocument(QJsonObject{
        {"serverUrl", "https://planer.werk.local/"},
        {"hmiId", kHmiId.toUpper()},
        {"token", kToken},
        {"extra", "ignored"}}).toJson();
    const ConfigParseResult result = parseConfigJson(json);
    QVERIFY2(result.ok, qPrintable(result.error));
    QCOMPARE(result.config.serverUrl, QStringLiteral("https://planer.werk.local"));
    QCOMPARE(result.config.hmiId, kHmiId);
    QCOMPARE(result.config.token, kToken);
    QCOMPARE(result.config.hmiPageUrl(), QStringLiteral("https://planer.werk.local/hmi/") + kHmiId);
    QCOMPARE(result.config.apiUrl(QString::fromLatin1(Api::FINALS)).toString(),
             QStringLiteral("https://planer.werk.local/api/hmi/v1/finals"));
}

void PlannerLogicTest::configRejectsPlainHttpToRemoteHosts()
{
    QString normalized;
    QString error;
    QVERIFY(!normalizeServerUrl(QStringLiteral("http://planer.werk.local"), &normalized, &error));
    QVERIFY(!normalizeServerUrl(QStringLiteral("http://192.168.0.10:3000"), &normalized, &error));
    QVERIFY(!normalizeServerUrl(QStringLiteral("ftp://planer.werk.local"), &normalized, &error));
    QVERIFY(!normalizeServerUrl(QStringLiteral("https://user:pw@planer.werk.local"), &normalized, &error));
    QVERIFY(!normalizeServerUrl(QStringLiteral("https://planer.werk.local/?x=1"), &normalized, &error));

    // Plain HTTP only for local development servers.
    QVERIFY(normalizeServerUrl(QStringLiteral("http://localhost:3000"), &normalized, &error));
    QCOMPARE(normalized, QStringLiteral("http://localhost:3000"));
    QVERIFY(normalizeServerUrl(QStringLiteral("http://127.0.0.1:3000/"), &normalized, &error));
    QVERIFY(normalizeServerUrl(QStringLiteral("http://[::1]:3000"), &normalized, &error));

    QVERIFY(isSecureTransport(QUrl(QStringLiteral("https://planer.werk.local/api"))));
    QVERIFY(!isSecureTransport(QUrl(QStringLiteral("http://planer.werk.local/api"))));
    QVERIFY(isSecureTransport(QUrl(QStringLiteral("http://127.0.0.1:8000/api"))));
}

void PlannerLogicTest::configRejectsBadTokenAndHmiId()
{
    auto parse = [](const QString& hmiId, const QString& token) {
        return parseConfigJson(QJsonDocument(QJsonObject{
            {"serverUrl", "https://planer.werk.local"}, {"hmiId", hmiId}, {"token", token}}).toJson());
    };
    QVERIFY(parse(kHmiId, kToken).ok);
    QVERIFY(!parse(QStringLiteral("not-a-uuid"), kToken).ok);
    QVERIFY(!parse(kHmiId, kToken.left(42)).ok);
    QVERIFY(!parse(kHmiId, kToken + QStringLiteral("x")).ok);
    QVERIFY(!parse(kHmiId, QString(kToken).replace(0, 1, QStringLiteral("+"))).ok);
    QVERIFY(!parseConfigJson("{\"serverUrl\":\"https://x\"}").ok);
    QVERIFY(!parseConfigJson("not json").ok);

    // Errors never echo the token.
    const ConfigParseResult bad = parse(kHmiId, kToken.left(42));
    QVERIFY(!bad.error.contains(kToken.left(10)));
}

void PlannerLogicTest::configRoundTripsThroughSettings()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("settings.json"));

    multipack::config::SettingsManager settings;
    settings.resetToDefaults();
    PlannerConfig config;
    config.serverUrl = QStringLiteral("https://planer.werk.local");
    config.hmiId = kHmiId;
    config.token = kToken;
    config.caCertPath = QStringLiteral("planner/planner-ca.pem");
    storeInSettings(settings, config);
    QVERIFY(settings.save(path));

    // The file holds a secret: owner-only permissions.
    const QFile::Permissions permissions = QFile::permissions(path);
    QVERIFY(permissions.testFlag(QFileDevice::ReadOwner));
    QVERIFY(!permissions.testFlag(QFileDevice::ReadOther));
    QVERIFY(!permissions.testFlag(QFileDevice::ReadGroup));

    multipack::config::SettingsManager loaded;
    QVERIFY(loaded.load(path));
    const PlannerConfig back = loadFromSettings(loaded);
    QCOMPARE(back.serverUrl, config.serverUrl);
    QCOMPARE(back.hmiId, config.hmiId);
    QCOMPARE(back.token, config.token);
    QCOMPARE(back.caCertPath, config.caCertPath);
    QCOMPARE(resolveCaCertPath(back, dir.path()), dir.filePath(QStringLiteral("planner/planner-ca.pem")));

    clearFromSettings(loaded);
    QVERIFY(!loadFromSettings(loaded).isComplete());
}

void PlannerLogicTest::importCopiesCaAndRejectsBadPem()
{
    QTemporaryDir usb;
    QTemporaryDir work;
    QVERIFY(usb.isValid() && work.isValid());

    const QByteArray json = QJsonDocument(QJsonObject{
        {"serverUrl", "https://planer.werk.local"}, {"hmiId", kHmiId}, {"token", kToken}}).toJson();
    const QString configPath = usb.filePath(QStringLiteral("hmi-linie-3.json"));
    QVERIFY(writeFile(configPath, json));

    // Without a CA: system trust store.
    ImportResult plain = importFromFile(configPath, work.path());
    QVERIFY2(plain.ok, qPrintable(plain.error));
    QVERIFY(!plain.caImported);
    QVERIFY(plain.config.caCertPath.isEmpty());

    // With a CA next to it (same base name).
    QVERIFY(writeFile(usb.filePath(QStringLiteral("hmi-linie-3.pem")), kCaPem));
    ImportResult withCa = importFromFile(configPath, work.path());
    QVERIFY2(withCa.ok, qPrintable(withCa.error));
    QVERIFY(withCa.caImported);
    QCOMPARE(withCa.config.caCertPath, QStringLiteral("planner/planner-ca.pem"));
    const QString copied = resolveCaCertPath(withCa.config, work.path());
    QCOMPARE(loadCaCertificates(copied).size(), 1);

    // A broken PEM fails the whole import.
    QVERIFY(writeFile(usb.filePath(QStringLiteral("hmi-linie-3.pem")), "-----BEGIN CERTIFICATE-----\nxx\n"));
    ImportResult broken = importFromFile(configPath, work.path());
    QVERIFY(!broken.ok);
    QVERIFY(!broken.error.isEmpty());
}

void PlannerLogicTest::importFindsOnlyHmiConfigFiles()
{
    QTemporaryDir usb;
    QVERIFY(usb.isValid());
    QVERIFY(writeFile(usb.filePath(QStringLiteral("hmi-a.json")), "{}"));
    QVERIFY(writeFile(usb.filePath(QStringLiteral("hmi-b.json")), "{}"));
    QVERIFY(writeFile(usb.filePath(QStringLiteral("other.json")), "{}"));
    QVERIFY(writeFile(usb.filePath(QStringLiteral("plan.rob")), "1"));
    const QStringList found = findConfigFiles(usb.path());
    QCOMPARE(found.size(), 2);
    QVERIFY(found.first().endsWith(QStringLiteral("hmi-a.json")));
    QVERIFY(findConfigFiles(usb.filePath(QStringLiteral("missing"))).isEmpty());
}

void PlannerLogicTest::parsesFinalsAndSkipsInvalidEntries()
{
    const QByteArray body = R"({
      "finals": [
        {"id": "8d0a6f0e-4b52-4f8f-a1f4-5b9f6f0f2c11", "recordId": "project-42", "name": "SKU 9 · P-12",
         "fileName": "line-7.rob", "sha256": "2BA87CD84FFC7F6F302C26707ADC128A889977BD5480F745AB58AAB0A4E4DB3E",
         "markedAt": "2026-11-19T08:30:00.000Z", "future": 1},
        {"id": "../../etc", "recordId": "x", "name": "bad id", "fileName": "a.rob",
         "sha256": "2ba87cd84ffc7f6f302c26707adc128a889977bd5480f745ab58aab0a4e4db3e"},
        {"id": "9e1b7f1f-5c63-4a9f-b2a5-6caf7f1f3d22", "recordId": "p", "name": "bad sha", "sha256": "abc"},
        "not an object"
      ]
    })";
    const auto finals = parseFinalsResponse(body);
    QVERIFY(finals.has_value());
    QCOMPARE(finals->size(), 1);
    const FinalEntry& entry = finals->first();
    QCOMPARE(entry.id, kFinalA);
    QCOMPARE(entry.recordId, QStringLiteral("project-42"));
    QCOMPARE(entry.name, QStringLiteral("SKU 9 · P-12"));
    QCOMPARE(entry.sha256, QStringLiteral("2ba87cd84ffc7f6f302c26707adc128a889977bd5480f745ab58aab0a4e4db3e"));
    QCOMPARE(entry.markedAt, QDateTime(QDate(2026, 11, 19), QTime(8, 30), QTimeZone::utc()));

    QVERIFY(!parseFinalsResponse("{}").has_value());
    QVERIFY(!parseFinalsResponse("[]").has_value());
    QVERIFY(!parseFinalsResponse("garbage").has_value());
    QVERIFY(parseFinalsResponse(R"({"finals": []})")->isEmpty());
}

void PlannerLogicTest::parsesProposal()
{
    const QByteArray body = R"({"proposal": {
        "id": "c3a1d7b2-0f8e-4e57-9c1a-3f2b6d5e4a90", "finalId": "8d0a6f0e-4b52-4f8f-a1f4-5b9f6f0f2c11",
        "name": "SKU 9 · P-12", "sha256": "2ba87cd84ffc7f6f302c26707adc128a889977bd5480f745ab58aab0a4e4db3e",
        "proposedBy": null, "createdAt": "2026-11-19T08:31:12.000Z"}})";
    const auto proposal = parseProposalResponse(body);
    QVERIFY(proposal.has_value());
    QCOMPARE(proposal->id, QStringLiteral("c3a1d7b2-0f8e-4e57-9c1a-3f2b6d5e4a90"));
    QCOMPARE(proposal->finalId, kFinalA);
    QVERIFY(proposal->proposedBy.isEmpty());
    QVERIFY(proposal->createdAt.isValid());

    QVERIFY(!parseProposalResponse(R"({"proposal": {"id": "x"}})").has_value());
    QVERIFY(!parseProposalResponse("{}").has_value());
}

void PlannerLogicTest::buildsStatusBodyWithNulls()
{
    const QJsonObject none = QJsonDocument::fromJson(buildStatusBody(QString(), QString(), "MultipackParser 2.0.0")).object();
    QVERIFY(none.contains("activeFinalId") && none.value("activeFinalId").isNull());
    QVERIFY(none.contains("activeSha256") && none.value("activeSha256").isNull());
    QCOMPARE(none.value("appVersion").toString(), QStringLiteral("MultipackParser 2.0.0"));

    const QJsonObject active = QJsonDocument::fromJson(buildStatusBody(kFinalA, "ab", "v")).object();
    QCOMPARE(active.value("activeFinalId").toString(), kFinalA);
    QCOMPARE(active.value("activeSha256").toString(), QStringLiteral("ab"));

    QCOMPARE(QJsonDocument::fromJson(buildProposalAnswerBody(true)).object().value("result").toString(),
             QStringLiteral("accepted"));
    QCOMPARE(QJsonDocument::fromJson(buildProposalAnswerBody(false)).object().value("result").toString(),
             QStringLiteral("rejected"));
}

void PlannerLogicTest::sha256MatchesKnownVectorCaseInsensitive()
{
    const QString abc = QStringLiteral("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    QCOMPARE(sha256Hex("abc"), abc);
    QVERIFY(sha256Matches(abc, "abc"));
    QVERIFY(sha256Matches(abc.toUpper(), "abc"));
    QVERIFY(!sha256Matches(abc, "abd"));
    QVERIFY(!sha256Matches(QStringLiteral("ba78"), "abc"));
    // Line endings are part of the bytes.
    QVERIFY(sha256Hex("a\r\nb") != sha256Hex("a\nb"));
}

void PlannerLogicTest::parsesRetryAfter()
{
    const QDateTime now(QDate(2015, 10, 21), QTime(7, 27, 0), QTimeZone::utc());
    QCOMPARE(parseRetryAfterMs("7", now).value_or(-1), qint64(7000));
    QCOMPARE(parseRetryAfterMs(" 0 ", now).value_or(-1), qint64(0));
    QCOMPARE(parseRetryAfterMs("Wed, 21 Oct 2015 07:28:00 GMT", now).value_or(-1), qint64(60000));
    QCOMPARE(parseRetryAfterMs("Wed, 21 Oct 2015 07:20:00 GMT", now).value_or(-1), qint64(0));
    QVERIFY(!parseRetryAfterMs("", now).has_value());
    QVERIFY(!parseRetryAfterMs("-5", now).has_value());
    QVERIFY(!parseRetryAfterMs("soon", now).has_value());
}

void PlannerLogicTest::classifiesResponses()
{
    QCOMPARE(classifyResponse(200, QString()), ResponseClass::Ok);
    QCOMPARE(classifyResponse(204, QString()), ResponseClass::Ok);
    QCOMPARE(classifyResponse(304, QString()), ResponseClass::Ok);
    QCOMPARE(classifyResponse(401, "unauthenticated"), ResponseClass::NotAuthorized);
    QCOMPARE(classifyResponse(404, "server_storage_disabled"), ResponseClass::NotAuthorized);
    QCOMPARE(classifyResponse(404, "final_not_found"), ResponseClass::NotFound);
    QCOMPARE(classifyResponse(404, "proposal_not_found"), ResponseClass::NotFound);
    QCOMPARE(classifyResponse(409, "proposal_closed"), ResponseClass::Conflict);
    QCOMPARE(classifyResponse(429, "rate_limited"), ResponseClass::RateLimited);
    QCOMPARE(classifyResponse(400, "invalid_request"), ResponseClass::ClientError);
    QCOMPARE(classifyResponse(500, QString()), ResponseClass::TransientError);
    QCOMPARE(classifyResponse(503, QString()), ResponseClass::TransientError);
    QCOMPARE(classifyResponse(0, QString()), ResponseClass::TransientError);
    QCOMPARE(classifyResponse(302, QString()), ResponseClass::TransientError);
}

void PlannerLogicTest::backoffDoublesWithJitterAndCaps()
{
    double factor = 1.0;
    SyncBackoff backoff([&factor]() { return factor; });
    QCOMPARE(backoff.currentDelayMs(), qint64(10'000));
    QCOMPARE(backoff.onTransientFailure(), qint64(10'000));
    QCOMPARE(backoff.onTransientFailure(), qint64(20'000));
    QCOMPARE(backoff.onTransientFailure(), qint64(40'000));
    QCOMPARE(backoff.onTransientFailure(), qint64(80'000));
    QCOMPARE(backoff.onTransientFailure(), qint64(160'000));
    QCOMPARE(backoff.onTransientFailure(), qint64(300'000));
    for (int i = 0; i < 100; ++i) {
        QCOMPARE(backoff.onTransientFailure(), qint64(300'000));  // no overflow in long outages
    }
    QCOMPARE(backoff.failureCount(), 106);

    QCOMPARE(backoff.onSuccess(), qint64(10'000));
    QCOMPARE(backoff.failureCount(), 0);

    factor = 0.5;
    QCOMPARE(backoff.onTransientFailure(), qint64(5'000));
    factor = 1.5;
    QCOMPARE(backoff.onTransientFailure(), qint64(30'000));
    for (int i = 0; i < 10; ++i) {
        backoff.onTransientFailure();
    }
    QCOMPARE(backoff.currentDelayMs(), qint64(300'000));  // jitter never exceeds the 5 min cap
    factor = 7.0;  // out-of-range jitter is clamped
    backoff.onSuccess();
    QCOMPARE(backoff.onTransientFailure(), qint64(15'000));

    // Default jitter stays in [0.5, 1.5].
    SyncBackoff random;
    for (int i = 0; i < 200; ++i) {
        random.onSuccess();
        const qint64 delay = random.onTransientFailure();
        QVERIFY(delay >= 5'000 && delay <= 15'000);
    }
}

void PlannerLogicTest::backoffHonoursRetryAfterAndNotAuthorized()
{
    SyncBackoff backoff([]() { return 1.0; });
    QCOMPARE(backoff.onRateLimited(qint64(7'000)), qint64(7'000));
    QCOMPARE(backoff.onRateLimited(qint64(600'000)), qint64(600'000));  // server knows best
    QCOMPARE(backoff.onRateLimited(qint64(0)), qint64(1'000));
    backoff.onSuccess();
    QCOMPARE(backoff.onRateLimited(std::nullopt), qint64(10'000));
    QCOMPARE(backoff.onNotAuthorized(), qint64(300'000));
    QCOMPARE(backoff.onNotAuthorized(), qint64(300'000));
    QCOMPARE(backoff.onSuccess(), qint64(10'000));
}

void PlannerLogicTest::parserAcceptsBytesWithAnyLineEnding()
{
    multipack::RobFileParser parser;
    const QByteArray lf = testhelpers::sampleRobFileContents().toUtf8();
    const auto fromLf = parser.parseBytes(lf, QStringLiteral("lf.rob"));
    const auto fromCrlf = parser.parseBytes(sampleRobCrlf(), QStringLiteral("crlf.rob"));
    QVERIFY2(fromLf.isValid, qPrintable(fromLf.errorMessage));
    QVERIFY2(fromCrlf.isValid, qPrintable(fromCrlf.errorMessage));
    QCOMPARE(fromCrlf.g_Daten, fromLf.g_Daten);
    QCOMPARE(fromCrlf.g_PalettenDim, (QVector<int>{1200, 800, 150}));
    QVERIFY(!fromCrlf.fileTimestamp.isValid());

    QVERIFY(!parser.parseBytes(QByteArray(), QStringLiteral("empty.rob")).isValid);
    QVERIFY(!parser.parseBytes("1200\t800\n<html>", QStringLiteral("html.rob")).isValid);
    QVERIFY(!parser.parseBytes("1200\t800\t150\n200\t100\t50\t0\n", QStringLiteral("short.rob")).isValid);
}

void PlannerLogicTest::parserAcceptsAnyWhitespace()
{
    multipack::RobFileParser parser;

    // The planner's own fixture: spaces + CRLF (was rejected by the HMI).
    const auto planner = parser.parseBytes(testhelpers::plannerFixtureRobCrlf(), QStringLiteral("line-7.rob"));
    QVERIFY2(planner.isValid, qPrintable(planner.errorMessage));
    QCOMPARE(planner.g_PalettenDim, (QVector<int>{1200, 800, 144}));
    QCOMPARE(planner.g_PaketDim, (QVector<int>{200, 300, 150, 1}));
    QCOMPARE(planner.g_LageArten, 2);
    QCOMPARE(planner.g_AnzLagen, 3);
    QCOMPARE(planner.g_LageZuordnung, (QVector<int>{1, 2, 1}));
    QCOMPARE(planner.g_PaketeZuordnung, (QVector<int>{1, 2}));
    QCOMPARE(planner.g_PaketPos.size(), 3);
    QCOMPARE(planner.g_PaketPos[1], (QVector<int>{100, 50, 0, 500, 300, 90, 2, 0, -1}));

    // Same plan with tabs (Multipack/USB) and with mixed separators: same data.
    QByteArray tabs = testhelpers::plannerFixtureRobCrlf();
    tabs.replace(' ', '\t');
    const auto fromTabs = parser.parseBytes(tabs, QStringLiteral("tabs.rob"));
    QVERIFY(fromTabs.isValid);
    QCOMPARE(fromTabs.g_Daten, planner.g_Daten);
    QByteArray mixed = testhelpers::plannerFixtureRobCrlf();
    mixed.replace("100 50 0 400", "100\t 50 \t0   400");
    mixed.replace("\r\n", "\n");
    const auto fromMixed = parser.parseBytes(mixed, QStringLiteral("mixed.rob"));
    QVERIFY(fromMixed.isValid);
    QCOMPARE(fromMixed.g_Daten, planner.g_Daten);

    // Files on a USB stick go through the same tokenizer.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(writeFile(dir.filePath(QStringLiteral("spaces.rob")), testhelpers::plannerFixtureRobCrlf()));
    multipack::RobFileParser usbParser(dir.path());
    QVERIFY(usbParser.parseFile(QStringLiteral("spaces.rob")).isValid);

    // Still rejected: non-numbers.
    QVERIFY(!parser.parseBytes("1200 800 abc\n200 300 150 1\n1\n1\n0\n1 0\n1\n0 0 0 1 1 0 1 0 0\n",
                               QStringLiteral("bad.rob")).isValid);
}

void PlannerLogicTest::storeRecordsAndClearsLoadErrors()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    PlannerStore store;
    QVERIFY(store.open(dir.filePath(QStringLiteral("paletten.db"))));

    const QByteArray garbage = "<html>not a plan</html>";
    FinalEntry a = entryFor(kFinalA, QStringLiteral("project-42"), QStringLiteral("Alpha"), garbage);
    QVERIFY(store.applyList({a}));
    QVERIFY(store.find(kFinalA)->loadError.isEmpty());

    QCOMPARE(store.storeRob(a, garbage), StoreOutcome::InvalidRob);
    QCOMPARE(store.find(kFinalA)->loadError, QString::fromLatin1(LOAD_ERROR_INVALID_ROB));
    QVERIFY(!store.find(kFinalA)->downloaded);
    QCOMPARE(store.finals().first().loadError, QString::fromLatin1(LOAD_ERROR_INVALID_ROB));

    // Same list again: the error stays (same bytes, still not loadable).
    QVERIFY(store.applyList({a}));
    QCOMPARE(store.find(kFinalA)->loadError, QString::fromLatin1(LOAD_ERROR_INVALID_ROB));

    // Wrong checksum is recorded as such.
    QCOMPARE(store.storeRob(a, "other bytes"), StoreOutcome::ShaMismatch);
    QCOMPARE(store.find(kFinalA)->loadError, QString::fromLatin1(LOAD_ERROR_CHECKSUM));

    // New checksum in the list: error cleared, download retried.
    const QByteArray rob = sampleRobCrlf();
    a.sha256 = sha256Hex(rob);
    QVERIFY(store.applyList({a}));
    QVERIFY(store.find(kFinalA)->loadError.isEmpty());

    // Error, then a good download: cleared.
    QCOMPARE(store.storeRob(a, rob + "x"), StoreOutcome::ShaMismatch);
    QVERIFY(!store.find(kFinalA)->loadError.isEmpty());
    QCOMPARE(store.storeRob(a, rob), StoreOutcome::Stored);
    QVERIFY(store.find(kFinalA)->loadError.isEmpty());

    // A proposal-only Fertigstand that fails is remembered (not listed).
    const QString idC = QStringLiteral("0c2c8a2a-6d74-4b0a-83b6-7dbf8f2f4e33");
    FinalEntry c = entryFor(idC, QString(), QStringLiteral("gamma"), garbage);
    QCOMPARE(store.storeRob(c, garbage), StoreOutcome::InvalidRob);
    QVERIFY(store.find(idC).has_value());
    QVERIFY(!store.find(idC)->inList);
    QCOMPARE(store.find(idC)->loadError, QString::fromLatin1(LOAD_ERROR_INVALID_ROB));

    // A database from before load_error existed is upgraded on open.
    store.close();
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("legacy"));
        db.setDatabaseName(dir.filePath(QStringLiteral("legacy.db")));
        QVERIFY(db.open());
        QSqlQuery q(db);
        QVERIFY(q.exec("CREATE TABLE planner_finals (final_id TEXT PRIMARY KEY, record_id TEXT NOT NULL, "
                       "name TEXT NOT NULL, file_name TEXT, sha256 TEXT NOT NULL, marked_at INTEGER, "
                       "in_list INTEGER NOT NULL DEFAULT 1, rob BLOB, stored_at INTEGER)"));
        db.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("legacy"));
    PlannerStore legacy;
    QVERIFY(legacy.open(dir.filePath(QStringLiteral("legacy.db"))));
    QVERIFY(legacy.applyList({a}));
    QCOMPARE(legacy.storeRob(c, garbage), StoreOutcome::InvalidRob);
    QCOMPARE(legacy.find(idC)->loadError, QString::fromLatin1(LOAD_ERROR_INVALID_ROB));
}

void PlannerLogicTest::storeVerifiesShaAndParserBeforeStoring()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPath = dir.filePath(QStringLiteral("paletten.db"));
    PlannerStore store;
    QVERIFY(store.open(dbPath));

    const QByteArray rob = sampleRobCrlf();
    const FinalEntry a = entryFor(kFinalA, QStringLiteral("project-42"), QStringLiteral("SKU 9"), rob);
    QVERIFY(store.applyList({a}));
    QCOMPARE(store.pendingDownloads().size(), 1);

    multipack::database::DatabaseManager db;
    QVERIFY(db.open(dbPath));

    // Wrong bytes: rejected by the checksum, nothing stored.
    QString error;
    QCOMPARE(store.storeRob(a, rob + "x", &error), StoreOutcome::ShaMismatch);
    QVERIFY(!db.loadPaletteData(plannerPaletteFileName(kFinalA)).has_value());
    QCOMPARE(store.pendingDownloads().size(), 1);

    // Matching checksum but not a .rob: rejected by the parser, nothing stored.
    const QByteArray garbage = "<html>not a plan</html>";
    FinalEntry bad = a;
    bad.sha256 = sha256Hex(garbage);
    QCOMPARE(store.storeRob(bad, garbage, &error), StoreOutcome::InvalidRob);
    QVERIFY(!db.loadPaletteData(plannerPaletteFileName(kFinalA)).has_value());
    QVERIFY(store.robBytes(kFinalA).isEmpty());

    // Valid: stored byte-exact (CRLF kept) and loadable as a palette.
    QCOMPARE(store.storeRob(a, rob, &error), StoreOutcome::Stored);
    QCOMPARE(store.robBytes(kFinalA), rob);
    QVERIFY(store.pendingDownloads().isEmpty());
    const auto loaded = db.loadPaletteData(plannerPaletteFileName(kFinalA));
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->paletteDimensions.length, 1200);
    QCOMPARE(loaded->metadata.anzLagen, 1);
    QCOMPARE(finalIdFromPaletteFileName(loaded->metadata.fileName), kFinalA);
    QCOMPARE(loaded->metadata.fileTimestamp, a.markedAt.toMSecsSinceEpoch());

    // Survives reopening (offline: local copies).
    store.close();
    PlannerStore reopened;
    QVERIFY(reopened.open(dbPath));
    QVERIFY(reopened.find(kFinalA)->downloaded);
    QCOMPARE(reopened.robBytes(kFinalA), rob);

    reopened.setFinalsEtag(QStringLiteral("\"v1\""));
    QCOMPARE(reopened.finalsEtag(), QStringLiteral("\"v1\""));
}

void PlannerLogicTest::storeKeepsWithdrawnFinalsAndBytes()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    PlannerStore store;
    QVERIFY(store.open(dir.filePath(QStringLiteral("paletten.db"))));

    const QByteArray rob = sampleRobCrlf();
    const FinalEntry a = entryFor(kFinalA, QStringLiteral("project-42"), QStringLiteral("Alpha"), rob);
    const FinalEntry b = entryFor(kFinalB, QStringLiteral("project-43"), QStringLiteral("beta"), rob);
    QVERIFY(store.applyList({a, b}));
    QCOMPARE(store.storeRob(a, rob), StoreOutcome::Stored);

    QVERIFY(store.applyList({b}));  // a withdrawn
    const auto storedA = store.find(kFinalA);
    QVERIFY(storedA.has_value());
    QVERIFY(!storedA->inList);
    QVERIFY(storedA->downloaded);  // never deleted
    QCOMPARE(store.robBytes(kFinalA), rob);
    QVERIFY(store.find(kFinalB)->inList);

    // Listed first, case-insensitive by name.
    const auto all = store.finals();
    QCOMPARE(all.size(), 2);
    QCOMPARE(all.first().entry.id, kFinalB);

    // Back in the list later: listed again, bytes kept.
    QVERIFY(store.applyList({a, b}));
    QVERIFY(store.find(kFinalA)->inList);
    QVERIFY(store.find(kFinalA)->downloaded);

    // A Fertigstand from a proposal that is not in the list yet.
    const QString idC = QStringLiteral("0c2c8a2a-6d74-4b0a-c3b6-7dbf8f2f4e33");
    FinalEntry c = entryFor(idC, QString(), QStringLiteral("gamma"), rob);
    QCOMPARE(store.storeRob(c, rob), StoreOutcome::Stored);
    QVERIFY(store.find(idC).has_value());
    QVERIFY(!store.find(idC)->inList);
}

void PlannerLogicTest::plannerRowsInvisibleToUsbLookups()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPath = dir.filePath(QStringLiteral("paletten.db"));

    multipack::database::DatabaseManager db;
    QVERIFY(db.open(dbPath));
    QCOMPARE(db.savePaletteData(testhelpers::makeSamplePaletteData(QStringLiteral("4711.rob"))),
             multipack::database::SaveResult::Inserted);

    PlannerStore store;
    QVERIFY(store.open(dbPath));
    const QByteArray rob = sampleRobCrlf();
    const FinalEntry a = entryFor(kFinalA, QStringLiteral("p"), QStringLiteral("SKU"), rob);
    QCOMPARE(store.storeRob(a, rob), StoreOutcome::Stored);

    const auto files = db.listAvailableFiles();
    QCOMPARE(files.size(), 1);
    QCOMPARE(files.first().fileName, QStringLiteral("4711.rob"));

    // "6f0e" is a fragment of the Fertigstand id: must not load it.
    QVERIFY(!db.loadPaletteData(QStringLiteral("6f0e")).has_value());
    QVERIFY(!db.findFile(QStringLiteral("6f0e")).has_value());
    QVERIFY(db.findByPackageDimensions(200, 100, 50) == QStringList{QStringLiteral("4711")});
    QCOMPARE(db.loadPaletteData(QStringLiteral("4711"))->metadata.fileName, QStringLiteral("4711.rob"));
    // The exact planner name (what GlobalState/XML-RPC use) still works.
    QVERIFY(db.loadPaletteData(plannerPaletteFileName(kFinalA)).has_value());
    QVERIFY(db.updateBoxDimensions(plannerPaletteFileName(kFinalA), -1, 2.5, -1));
    QCOMPARE(db.getBoxWeight(plannerPaletteFileName(kFinalA)).value_or(0.0), 2.5);
}

void PlannerLogicTest::planListHidesWithdrawnExceptActive()
{
    auto stored = [](const QString& id, const QString& record, bool inList, bool downloaded) {
        StoredFinal s;
        s.entry.id = id;
        s.entry.recordId = record;
        s.entry.name = id.left(4);
        s.inList = inList;
        s.downloaded = downloaded;
        return s;
    };
    const QVector<StoredFinal> finals = {
        stored(kFinalA, QStringLiteral("r1"), false, true),   // withdrawn
        stored(kFinalB, QStringLiteral("r2"), true, true),
    };

    PlanListView none = buildPlanList(finals, QString());
    QCOMPARE(none.rows.size(), 1);
    QCOMPARE(none.rows.first().final.entry.id, kFinalB);

    PlanListView active = buildPlanList(finals, kFinalA);
    QCOMPARE(active.rows.size(), 2);
    bool sawWithdrawnActive = false;
    for (const PlanListRow& row : active.rows) {
        if (row.final.entry.id == kFinalA) {
            QVERIFY(row.isActive);
            QVERIFY(row.isWithdrawn);
            sawWithdrawnActive = true;
        }
    }
    QVERIFY(sawWithdrawnActive);
    QVERIFY(!active.newVersion.has_value());
}

void PlannerLogicTest::planListFlagsNewVersionOfActivePlan()
{
    auto stored = [](const QString& id, const QString& record, bool inList, const QDateTime& markedAt) {
        StoredFinal s;
        s.entry.id = id;
        s.entry.recordId = record;
        s.entry.name = QStringLiteral("Plan ") + record;
        s.entry.markedAt = markedAt;
        s.inList = inList;
        s.downloaded = true;
        return s;
    };
    const QDateTime t0(QDate(2026, 11, 19), QTime(8, 0), QTimeZone::utc());
    const QString idNew = QStringLiteral("0c2c8a2a-6d74-4b0a-c3b6-7dbf8f2f4e33");
    const QVector<StoredFinal> finals = {
        stored(kFinalA, QStringLiteral("r1"), false, t0),               // running, re-marked -> withdrawn
        stored(idNew, QStringLiteral("r1"), true, t0.addSecs(3600)),    // new version of r1
        stored(kFinalB, QStringLiteral("r2"), true, t0),                // unrelated
    };

    const PlanListView view = buildPlanList(finals, kFinalA);
    QVERIFY(view.newVersion.has_value());
    QCOMPARE(view.newVersion->id, idNew);
    for (const PlanListRow& row : view.rows) {
        QCOMPARE(row.isNewVersionOfActive, row.final.entry.id == idNew);
    }

    // Running the newest one: no hint.
    QVERIFY(!buildPlanList(finals, idNew).newVersion.has_value());
    // Not a planner plan: no hint.
    QVERIFY(!buildPlanList(finals, QString()).newVersion.has_value());
}

void PlannerLogicTest::switchGuardBlocksRunningOrUnknownRobot()
{
    using multipack::robot::ProgramState;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    QCOMPARE(canSwitchPlan(true, ProgramState::Stopped, now.addSecs(-2), now), SwitchPermission::Allowed);
    QCOMPARE(canSwitchPlan(true, ProgramState::Playing, now, now), SwitchPermission::ProgramRunning);
    QCOMPARE(canSwitchPlan(true, ProgramState::Paused, now, now), SwitchPermission::ProgramRunning);
    QCOMPARE(canSwitchPlan(true, ProgramState::Unknown, now, now), SwitchPermission::RobotUnknown);
    QCOMPARE(canSwitchPlan(false, ProgramState::Stopped, now, now), SwitchPermission::RobotUnknown);
    QCOMPARE(canSwitchPlan(true, ProgramState::Stopped, QDateTime(), now), SwitchPermission::RobotUnknown);
    QCOMPARE(canSwitchPlan(true, ProgramState::Stopped, now.addSecs(-30), now), SwitchPermission::RobotUnknown);
}

void PlannerLogicTest::qrCodeEncodesOnlyThePageUrl()
{
    PlannerConfig config;
    config.serverUrl = QStringLiteral("https://planer.werk.local");
    config.hmiId = kHmiId;
    config.token = kToken;
    const QString url = config.hmiPageUrl();
    QVERIFY(!url.contains(kToken));
    QCOMPARE(url, QStringLiteral("https://planer.werk.local/hmi/") + kHmiId);

    const QImage image = multipack::ui::renderQrCode(url);
    QVERIFY(!image.isNull());
    QCOMPARE(image.width(), image.height());
    QVERIFY(image.width() >= 21 + 8);
    // Quiet zone is white, finder pattern corner is black.
    QCOMPARE(image.pixelIndex(0, 0), 0);
    QCOMPARE(image.pixelIndex(4, 4), 1);
    QVERIFY(multipack::ui::renderQrCode(QString()).isNull());
}

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    PlannerLogicTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_planner_logic.moc"
