#include "TestHelpers.h"
#include "FakePlannerServer.h"

#include "multipack/config/SettingsManager.h"
#include "multipack/core/GlobalState.h"
#include "multipack/database/DatabaseManager.h"
#include "multipack/planner/PlannerConfig.h"
#include "multipack/planner/PlannerPlanList.h"
#include "multipack/planner/PlannerProtocol.h"
#include "multipack/planner/PlannerStore.h"
#include "multipack/planner/PlannerSyncService.h"
#include "multipack/robot/RobotStatusMonitor.h"
#include "multipack/ui/MainWindow.h"
#include "multipack/ui/PlanProposalDialog.h"

#include <QApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QElapsedTimer>
#include <QFontMetrics>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using multipack::planner::FinalEntry;
using multipack::planner::PlannerConfig;
using multipack::planner::PlannerSyncService;
using testhelpers::FakePlannerServer;

namespace {

const QString kHmiId = QStringLiteral("2f7c1c0e-4c1d-4d8e-9a57-0e5d1c9b7a10");
const QString kToken = QStringLiteral("q9bXo3Y2vT7c1Lk0pWmZr8sNfHq2uYbEo4xJd6gKa_-");
const QString kFinalA = QStringLiteral("8d0a6f0e-4b52-4f8f-a1f4-5b9f6f0f2c11");
const QString kFinalB = QStringLiteral("9e1b7f1f-5c63-4a9f-b2a5-6caf7f1f3d22");
const QString kProposal = QStringLiteral("c3a1d7b2-0f8e-4e57-9c1a-3f2b6d5e4a90");

QByteArray robA()
{
    QByteArray bytes = testhelpers::sampleRobFileContents().toUtf8();
    bytes.replace("\n", "\r\n");
    return bytes;
}

QByteArray robB()
{
    QByteArray bytes = testhelpers::sampleRobFileContents().toUtf8();
    bytes.replace("1200\t800\t150", "1000\t800\t150");
    return bytes;
}

QJsonObject finalJson(const QString& id, const QString& recordId, const QString& name, const QByteArray& bytes)
{
    return QJsonObject{
        {"id", id}, {"recordId", recordId}, {"name", name}, {"fileName", name + ".rob"},
        {"sha256", multipack::planner::sha256Hex(bytes)}, {"markedAt", "2026-11-19T08:30:00.000Z"}};
}

QByteArray finalsBody(const QJsonArray& finals)
{
    return QJsonDocument(QJsonObject{{"finals", finals}}).toJson(QJsonDocument::Compact);
}

QByteArray proposalBody(const QString& finalId, const QByteArray& bytes)
{
    return QJsonDocument(QJsonObject{{"proposal", QJsonObject{
        {"id", kProposal}, {"finalId", finalId}, {"name", "SKU 9 · P-12"},
        {"sha256", multipack::planner::sha256Hex(bytes)}, {"proposedBy", "planer"},
        {"createdAt", "2026-11-19T08:31:12.000Z"}}}}).toJson(QJsonDocument::Compact);
}

PlannerConfig configFor(quint16 port, const QString& scheme = QStringLiteral("http"))
{
    PlannerConfig config;
    config.serverUrl = QStringLiteral("%1://127.0.0.1:%2").arg(scheme).arg(port);
    config.hmiId = kHmiId;
    config.token = kToken;
    return config;
}

/// Run one cycle (the first one starts from setConfig) and wait for it.
bool runCycle(PlannerSyncService& service, bool start = true)
{
    QSignalSpy spy(&service, &PlannerSyncService::cycleFinished);
    if (start) {
        service.pollNow();
    }
    return spy.wait(10000);
}

} // namespace

class PlannerSyncTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void fullCycleDownloadsVerifiesAndReportsStatus();
    void secondPollSendsEtagAndHandles304();
    void withdrawnFinalsStayLocal();
    void notAuthorizedStopsForFiveMinutes();
    void storageDisabledCountsAsNotAuthorized();
    void rateLimitHonoursRetryAfter();
    void serverErrorsBackOffAndRecover();
    void offlineKeepsLocalCopies();
    void proposalDownloadsUnknownFinalFirst();
    void proposalAnswerOutcomes();
    void statusReportsActivePlan();
    void insecureRemoteHttpIsNeverContacted();
    void tlsRequiresTrustedCertificate();

    // MainWindow: takeover only after confirmation, never while running
    void proposalNeverSwitchesWithoutOperator();
    void proposalAcceptSwitchesOnlyAfterConfirmation();
    void proposalClosedDoesNotSwitch();
    void proposalConfirmedWhileRunningDoesNotSwitch();
    void manualLoadBlockedWhileProgramRuns();

    // e2e findings
    void plannerFixtureWithSpacesIsStored();
    void unloadableFinalShowsErrorAndProposalIsRejected();
    void proposalForUnknownUnloadableFinalIsRejected();
    void unloadableNoticeBeforeWindowIsShown();
    void answerAgainstHangingServerGivesUpQuickly();
    void refusalMessageIsNotClipped();
    void proposalDialogShowsRealPackageCount();

private:
    QTemporaryDir* m_dir = nullptr;
    std::unique_ptr<QTemporaryDir> m_dirHolder;
    std::unique_ptr<FakePlannerServer> m_server;
    std::unique_ptr<PlannerSyncService> m_service;

    void startService(const PlannerConfig& config);
};

void PlannerSyncTest::init()
{
    m_service.reset();
    m_server = std::make_unique<FakePlannerServer>();
    QVERIFY(m_server->listen(QHostAddress::LocalHost, 0));
    m_dirHolder = std::make_unique<QTemporaryDir>();
    m_dir = m_dirHolder.get();
    QVERIFY(m_dir->isValid());
}

void PlannerSyncTest::startService(const PlannerConfig& config)
{
    m_service = std::make_unique<PlannerSyncService>();
    m_service->setAutoSchedule(false);
    m_service->setJitterSource([]() { return 1.0; });
    m_service->setAppVersion(QStringLiteral("MultipackParser test"));
    QVERIFY(m_service->openStore(m_dir->filePath(QStringLiteral("paletten.db"))));
    QSignalSpy spy(m_service.get(), &PlannerSyncService::cycleFinished);
    m_service->setConfig(config, m_dir->path());
    if (m_service->isConfigured()) {
        QVERIFY(spy.wait(10000));
    }
}

void PlannerSyncTest::fullCycleDownloadsVerifiesAndReportsStatus()
{
    m_server->setHandler([](const FakePlannerServer::Request& r) {
        if (r.path == "/api/hmi/v1/finals") {
            auto reply = FakePlannerServer::Reply::json(200, finalsBody({
                finalJson(kFinalA, "project-42", "Alpha", robA()),
                finalJson(kFinalB, "project-43", "Beta", robB())}));
            reply.header("ETag", "\"v1\"");
            return reply;
        }
        if (r.path == "/api/hmi/v1/finals/" + kFinalA.toLatin1() + "/rob") {
            FakePlannerServer::Reply reply;
            reply.body = robA();
            return reply;
        }
        if (r.path == "/api/hmi/v1/finals/" + kFinalB.toLatin1() + "/rob") {
            FakePlannerServer::Reply reply;
            reply.body = robB() + "tampered";  // checksum mismatch
            return reply;
        }
        if (r.path == "/api/hmi/v1/proposal") {
            return FakePlannerServer::Reply::empty(204);
        }
        if (r.method == "POST" && r.path == "/api/hmi/v1/status") {
            return FakePlannerServer::Reply::empty(204);
        }
        return FakePlannerServer::Reply::json(404, "{\"error\":\"not_found\"}");
    });

    startService(configFor(m_server->serverPort()));
    QCOMPARE(m_service->state(), PlannerSyncService::ConnectionState::Online);
    QCOMPARE(m_service->nextDelayMs(), qint64(10'000));

    // Every request carries the device token.
    QVERIFY(!m_server->requests.isEmpty());
    for (const auto& request : m_server->requests) {
        QCOMPARE(request.headers.value("authorization"), QByteArray("Bearer ") + kToken.toLatin1());
    }

    // A: verified, parsed, stored byte-exact. B: discarded (checksum).
    const auto a = m_service->store().find(kFinalA);
    QVERIFY(a.has_value() && a->downloaded);
    QCOMPARE(m_service->store().robBytes(kFinalA), robA());
    const auto b = m_service->store().find(kFinalB);
    QVERIFY(b.has_value() && !b->downloaded);

    // Status report after the first successful cycle: nothing from the planner is running.
    QTRY_VERIFY_WITH_TIMEOUT(!m_server->requestsFor("POST", "/api/hmi/v1/status").isEmpty(), 5000);
    const QJsonObject body = QJsonDocument::fromJson(
        m_server->requestsFor("POST", "/api/hmi/v1/status").first().body).object();
    QVERIFY(body.value("activeFinalId").isNull());
    QVERIFY(body.value("activeSha256").isNull());
    QCOMPARE(body.value("appVersion").toString(), QStringLiteral("MultipackParser test"));

    // Order within a cycle: list, downloads, proposal.
    QCOMPARE(m_server->requests.first().path, QByteArray("/api/hmi/v1/finals"));
}

void PlannerSyncTest::secondPollSendsEtagAndHandles304()
{
    int robBRequests = 0;
    m_server->setHandler([&robBRequests](const FakePlannerServer::Request& r) {
        if (r.path == "/api/hmi/v1/finals") {
            if (r.headers.value("if-none-match") == "\"v1\"") {
                return FakePlannerServer::Reply::empty(304);
            }
            auto reply = FakePlannerServer::Reply::json(200, finalsBody({
                finalJson(kFinalA, "project-42", "Alpha", robA()),
                finalJson(kFinalB, "project-43", "Beta", robB())}));
            reply.header("ETag", "\"v1\"");
            return reply;
        }
        if (r.path.endsWith(kFinalA.toLatin1() + "/rob")) {
            FakePlannerServer::Reply reply;
            reply.body = robA();
            return reply;
        }
        if (r.path.endsWith(kFinalB.toLatin1() + "/rob")) {
            ++robBRequests;
            FakePlannerServer::Reply reply;
            reply.body = "broken";
            return reply;
        }
        return FakePlannerServer::Reply::empty(204);
    });

    startService(configFor(m_server->serverPort()));
    QCOMPARE(robBRequests, 1);
    const int before = m_server->requestsFor("GET", "/api/hmi/v1/finals").size();

    QVERIFY(runCycle(*m_service));
    const auto listRequests = m_server->requestsFor("GET", "/api/hmi/v1/finals");
    QCOMPARE(listRequests.size(), before + 1);
    QCOMPARE(listRequests.last().headers.value("if-none-match"), QByteArray("\"v1\""));
    QCOMPARE(m_service->state(), PlannerSyncService::ConnectionState::Online);
    // A mismatching download is retried in a later cycle, not hammered within 10 s.
    QCOMPARE(robBRequests, 1);
    QVERIFY(m_service->store().find(kFinalA)->downloaded);
}

void PlannerSyncTest::withdrawnFinalsStayLocal()
{
    bool withdrawn = false;
    m_server->setHandler([&withdrawn](const FakePlannerServer::Request& r) {
        if (r.path == "/api/hmi/v1/finals") {
            auto reply = FakePlannerServer::Reply::json(
                200, withdrawn ? finalsBody({}) : finalsBody({finalJson(kFinalA, "project-42", "Alpha", robA())}));
            reply.header("ETag", withdrawn ? "\"v2\"" : "\"v1\"");
            return reply;
        }
        if (r.path.endsWith("/rob")) {
            FakePlannerServer::Reply reply;
            reply.body = robA();
            return reply;
        }
        return FakePlannerServer::Reply::empty(204);
    });

    startService(configFor(m_server->serverPort()));
    QVERIFY(m_service->store().find(kFinalA)->downloaded);

    withdrawn = true;
    QSignalSpy changed(m_service.get(), &PlannerSyncService::finalsChanged);
    QVERIFY(runCycle(*m_service));
    QVERIFY(changed.count() >= 1);
    const auto a = m_service->store().find(kFinalA);
    QVERIFY(!a->inList);
    QVERIFY(a->downloaded);
    QCOMPARE(m_service->store().robBytes(kFinalA), robA());

    QVERIFY(multipack::planner::buildPlanList(m_service->store().finals(), QString()).rows.isEmpty());
    QCOMPARE(multipack::planner::buildPlanList(m_service->store().finals(), kFinalA).rows.size(), 1);
}

void PlannerSyncTest::notAuthorizedStopsForFiveMinutes()
{
    m_server->setHandler([](const FakePlannerServer::Request&) {
        auto reply = FakePlannerServer::Reply::json(401, "{\"error\":\"unauthenticated\"}");
        reply.header("WWW-Authenticate", "Bearer realm=\"hmi\"");
        return reply;
    });
    startService(configFor(m_server->serverPort()));
    QCOMPARE(m_service->state(), PlannerSyncService::ConnectionState::NotAuthorized);
    QCOMPARE(m_service->stateText(), QStringLiteral("HMI nicht freigeschaltet"));
    QCOMPARE(m_service->nextDelayMs(), qint64(300'000));
    // Only the list was requested; no further calls in that cycle.
    QCOMPARE(m_server->requests.size(), 1);

    // Status reports are not attempted while not authorized.
    m_service->setActivePlan(QString());
    QTest::qWait(200);
    QVERIFY(m_server->requestsFor("POST", "/api/hmi/v1/status").isEmpty());
}

void PlannerSyncTest::storageDisabledCountsAsNotAuthorized()
{
    m_server->setHandler([](const FakePlannerServer::Request&) {
        return FakePlannerServer::Reply::json(404, "{\"error\":\"server_storage_disabled\"}");
    });
    startService(configFor(m_server->serverPort()));
    QCOMPARE(m_service->state(), PlannerSyncService::ConnectionState::NotAuthorized);
    QCOMPARE(m_service->nextDelayMs(), qint64(300'000));
}

void PlannerSyncTest::rateLimitHonoursRetryAfter()
{
    m_server->setHandler([](const FakePlannerServer::Request&) {
        auto reply = FakePlannerServer::Reply::json(429, "{\"error\":\"rate_limited\"}");
        reply.header("Retry-After", "7");
        return reply;
    });
    startService(configFor(m_server->serverPort()));
    QCOMPARE(m_service->nextDelayMs(), qint64(7'000));
    QVERIFY(m_service->state() != PlannerSyncService::ConnectionState::Online);
}

void PlannerSyncTest::serverErrorsBackOffAndRecover()
{
    bool healthy = false;
    m_server->setHandler([&healthy](const FakePlannerServer::Request& r) {
        if (!healthy) {
            return FakePlannerServer::Reply::empty(503);
        }
        if (r.path == "/api/hmi/v1/finals") {
            return FakePlannerServer::Reply::json(200, finalsBody({}));
        }
        return FakePlannerServer::Reply::empty(204);
    });
    startService(configFor(m_server->serverPort()));
    QCOMPARE(m_service->state(), PlannerSyncService::ConnectionState::Offline);
    QCOMPARE(m_service->nextDelayMs(), qint64(10'000));
    QVERIFY(runCycle(*m_service));
    QCOMPARE(m_service->nextDelayMs(), qint64(20'000));
    QVERIFY(runCycle(*m_service));
    QCOMPARE(m_service->nextDelayMs(), qint64(40'000));

    healthy = true;
    QVERIFY(runCycle(*m_service));
    QCOMPARE(m_service->state(), PlannerSyncService::ConnectionState::Online);
    QCOMPARE(m_service->nextDelayMs(), qint64(10'000));
    QVERIFY(m_service->lastSuccess().isValid());
}

void PlannerSyncTest::offlineKeepsLocalCopies()
{
    m_server->setHandler([](const FakePlannerServer::Request& r) {
        if (r.path == "/api/hmi/v1/finals") {
            return FakePlannerServer::Reply::json(200, finalsBody({finalJson(kFinalA, "p", "Alpha", robA())}));
        }
        if (r.path.endsWith("/rob")) {
            FakePlannerServer::Reply reply;
            reply.body = robA();
            return reply;
        }
        return FakePlannerServer::Reply::empty(204);
    });
    startService(configFor(m_server->serverPort()));
    QVERIFY(m_service->store().find(kFinalA)->downloaded);

    // Planner disappears (mesh outage).
    const quint16 port = m_server->serverPort();
    m_server->close();
    QVERIFY(runCycle(*m_service));
    QCOMPARE(m_service->state(), PlannerSyncService::ConnectionState::Offline);
    QVERIFY(m_service->store().find(kFinalA)->inList);
    QVERIFY(m_service->store().find(kFinalA)->downloaded);

    // App restart while offline: the local copy is still there and loadable.
    m_service.reset();
    multipack::database::DatabaseManager db;
    QVERIFY(db.open(m_dir->filePath(QStringLiteral("paletten.db"))));
    QVERIFY(db.loadPaletteData(multipack::planner::plannerPaletteFileName(kFinalA)).has_value());
    Q_UNUSED(port);
}

void PlannerSyncTest::proposalDownloadsUnknownFinalFirst()
{
    m_server->setHandler([](const FakePlannerServer::Request& r) {
        if (r.path == "/api/hmi/v1/finals") {
            return FakePlannerServer::Reply::json(200, finalsBody({}));  // not listed yet
        }
        if (r.path == "/api/hmi/v1/proposal") {
            return FakePlannerServer::Reply::json(200, proposalBody(kFinalA, robA()));
        }
        if (r.path.endsWith(kFinalA.toLatin1() + "/rob")) {
            FakePlannerServer::Reply reply;
            reply.body = robA();
            return reply;
        }
        return FakePlannerServer::Reply::empty(204);
    });

    m_service = std::make_unique<PlannerSyncService>();
    m_service->setAutoSchedule(false);
    QVERIFY(m_service->openStore(m_dir->filePath(QStringLiteral("paletten.db"))));
    QSignalSpy available(m_service.get(), &PlannerSyncService::proposalAvailable);
    QSignalSpy cycle(m_service.get(), &PlannerSyncService::cycleFinished);
    m_service->setConfig(configFor(m_server->serverPort()), m_dir->path());
    QVERIFY(cycle.wait(10000));

    QCOMPARE(available.count(), 1);
    const auto proposal = available.first().first().value<multipack::planner::Proposal>();
    QCOMPARE(proposal.id, kProposal);
    QCOMPARE(proposal.finalId, kFinalA);
    QVERIFY(m_service->store().find(kFinalA)->downloaded);
    QVERIFY(m_service->currentProposal().has_value());

    // Same proposal again: not re-announced.
    QVERIFY(runCycle(*m_service));
    QCOMPARE(available.count(), 1);
}

void PlannerSyncTest::proposalAnswerOutcomes()
{
    int answerStatus = 204;
    m_server->setHandler([&answerStatus](const FakePlannerServer::Request& r) {
        if (r.method == "POST" && r.path.startsWith("/api/hmi/v1/proposal/")) {
            if (answerStatus == 409) {
                return FakePlannerServer::Reply::json(409, "{\"error\":\"proposal_closed\",\"state\":\"superseded\"}");
            }
            return FakePlannerServer::Reply::empty(answerStatus);
        }
        if (r.path == "/api/hmi/v1/finals") {
            return FakePlannerServer::Reply::json(200, finalsBody({}));
        }
        return FakePlannerServer::Reply::empty(204);
    });
    startService(configFor(m_server->serverPort()));

    using Outcome = PlannerSyncService::AnswerOutcome;
    auto answer = [this](bool accepted) {
        QSignalSpy spy(m_service.get(), &PlannerSyncService::proposalAnswered);
        m_service->answerProposal(kProposal, accepted);
        if (!spy.wait(10000)) {
            return Outcome::NotDelivered;
        }
        return spy.first().at(2).value<Outcome>();
    };

    answerStatus = 500;
    QCOMPARE(answer(true), Outcome::NotDelivered);
    answerStatus = 204;
    QCOMPARE(answer(true), Outcome::Confirmed);
    const auto posts = m_server->requestsFor("POST", "/api/hmi/v1/proposal/");
    QCOMPARE(posts.last().path, QByteArray("/api/hmi/v1/proposal/") + kProposal.toLatin1());
    QCOMPARE(QJsonDocument::fromJson(posts.last().body).object().value("result").toString(),
             QStringLiteral("accepted"));
    QCOMPARE(posts.last().headers.value("content-type"), QByteArray("application/json"));

    answerStatus = 409;
    QCOMPARE(answer(false), Outcome::Closed);
    QCOMPARE(QJsonDocument::fromJson(m_server->requestsFor("POST", "/api/hmi/v1/proposal/").last().body)
                 .object().value("result").toString(),
             QStringLiteral("rejected"));
    answerStatus = 404;
    QCOMPARE(answer(false), Outcome::Closed);
}

void PlannerSyncTest::statusReportsActivePlan()
{
    m_server->setHandler([](const FakePlannerServer::Request& r) {
        if (r.path == "/api/hmi/v1/finals") {
            return FakePlannerServer::Reply::json(200, finalsBody({finalJson(kFinalA, "p", "Alpha", robA())}));
        }
        if (r.path.endsWith("/rob")) {
            FakePlannerServer::Reply reply;
            reply.body = robA();
            return reply;
        }
        return FakePlannerServer::Reply::empty(204);
    });
    startService(configFor(m_server->serverPort()));
    QTRY_VERIFY_WITH_TIMEOUT(!m_server->requestsFor("POST", "/api/hmi/v1/status").isEmpty(), 5000);
    const int before = m_server->requestsFor("POST", "/api/hmi/v1/status").size();

    QSignalSpy reported(m_service.get(), &PlannerSyncService::statusReported);
    m_service->setActivePlan(kFinalA);
    QVERIFY(reported.wait(5000));
    QCOMPARE(reported.last().first().toBool(), true);
    const auto posts = m_server->requestsFor("POST", "/api/hmi/v1/status");
    QCOMPARE(posts.size(), before + 1);
    const QJsonObject body = QJsonDocument::fromJson(posts.last().body).object();
    QCOMPARE(body.value("activeFinalId").toString(), kFinalA);
    QCOMPARE(body.value("activeSha256").toString(), multipack::planner::sha256Hex(robA()));

    // Switching to a USB plan reports "no planner plan".
    m_service->setActivePlan(QString());
    QVERIFY(reported.wait(5000));
    const QJsonObject usb = QJsonDocument::fromJson(
        m_server->requestsFor("POST", "/api/hmi/v1/status").last().body).object();
    QVERIFY(usb.value("activeFinalId").isNull());
}

void PlannerSyncTest::insecureRemoteHttpIsNeverContacted()
{
    m_service = std::make_unique<PlannerSyncService>();
    QVERIFY(m_service->openStore(m_dir->filePath(QStringLiteral("paletten.db"))));
    PlannerConfig config;
    config.serverUrl = QStringLiteral("http://planer.werk.local");
    config.hmiId = kHmiId;
    config.token = kToken;
    m_service->setConfig(config, m_dir->path());
    QVERIFY(!m_service->isConfigured());
    QCOMPARE(m_service->state(), PlannerSyncService::ConnectionState::NotConfigured);
    m_service->pollNow();
    m_service->setActivePlan(QString());
    QSignalSpy answered(m_service.get(), &PlannerSyncService::proposalAnswered);
    m_service->answerProposal(kProposal, true);
    QVERIFY(answered.wait(2000));
    QCOMPARE(answered.first().at(2).value<PlannerSyncService::AnswerOutcome>(),
             PlannerSyncService::AnswerOutcome::NotDelivered);
}

void PlannerSyncTest::tlsRequiresTrustedCertificate()
{
    if (!QSslSocket::supportsSsl()) {
        QSKIP("No TLS backend available");
    }
    const QString openssl = QStandardPaths::findExecutable(QStringLiteral("openssl"));
    if (openssl.isEmpty()) {
        QSKIP("openssl CLI not available to create test certificates");
    }

    // Throwaway CA + server certificate for 127.0.0.1, created per test run.
    const QString d = m_dir->path();
    auto run = [&openssl, &d](const QStringList& args) {
        QProcess process;
        process.setWorkingDirectory(d);
        process.start(openssl, args);
        return process.waitForFinished(30000) && process.exitCode() == 0;
    };
    QFile ext(m_dir->filePath(QStringLiteral("ext.cnf")));
    QVERIFY(ext.open(QIODevice::WriteOnly));
    ext.write("subjectAltName=IP:127.0.0.1,DNS:localhost\nbasicConstraints=CA:FALSE\n"
              "keyUsage=critical,digitalSignature\nextendedKeyUsage=serverAuth\n");
    ext.close();
    QVERIFY(run({"req", "-x509", "-newkey", "ec", "-pkeyopt", "ec_paramgen_curve:P-256", "-nodes",
                 "-keyout", "ca.key", "-out", "ca.pem", "-days", "2", "-subj", "/CN=Test CA",
                 "-addext", "basicConstraints=critical,CA:TRUE",
                 "-addext", "keyUsage=critical,keyCertSign,cRLSign"}));
    QVERIFY(run({"req", "-newkey", "ec", "-pkeyopt", "ec_paramgen_curve:P-256", "-nodes",
                 "-keyout", "server.key", "-out", "server.csr", "-subj", "/CN=127.0.0.1"}));
    QVERIFY(run({"x509", "-req", "-in", "server.csr", "-CA", "ca.pem", "-CAkey", "ca.key",
                 "-CAcreateserial", "-out", "server.pem", "-days", "2", "-extfile", "ext.cnf"}));

    QFile certFile(m_dir->filePath(QStringLiteral("server.pem")));
    QFile keyFile(m_dir->filePath(QStringLiteral("server.key")));
    QVERIFY(certFile.open(QIODevice::ReadOnly) && keyFile.open(QIODevice::ReadOnly));
    const QSslCertificate cert(certFile.readAll(), QSsl::Pem);
    const QSslKey key(keyFile.readAll(), QSsl::Ec, QSsl::Pem);
    QVERIFY(!cert.isNull() && !key.isNull());

    m_server->enableTls(cert, key);
    m_server->setHandler([](const FakePlannerServer::Request& r) {
        if (r.path == "/api/hmi/v1/finals") {
            return FakePlannerServer::Reply::json(200, finalsBody({}));
        }
        return FakePlannerServer::Reply::empty(204);
    });

    // 1) Without the factory CA: the certificate is untrusted -> no request is served.
    startService(configFor(m_server->serverPort(), QStringLiteral("https")));
    QCOMPARE(m_service->state(), PlannerSyncService::ConnectionState::Offline);
    QVERIFY(m_server->requests.isEmpty());

    // 2) With the factory CA (QSslConfiguration::addCaCertificates): trusted.
    PlannerConfig withCa = configFor(m_server->serverPort(), QStringLiteral("https"));
    withCa.caCertPath = QStringLiteral("ca.pem");  // relative to the working dir
    startService(withCa);
    QCOMPARE(m_service->state(), PlannerSyncService::ConnectionState::Online);
    QVERIFY(!m_server->requestsFor("GET", "/api/hmi/v1/finals").isEmpty());
    QCOMPARE(m_server->requests.first().headers.value("authorization"),
             QByteArray("Bearer ") + kToken.toLatin1());
}

// === MainWindow ===

namespace {

struct WindowFixture {
    QTemporaryDir dir;
    multipack::database::DatabaseManager database;
    multipack::config::SettingsManager settings;
    PlannerSyncService service;
    std::unique_ptr<multipack::ui::MainWindow> window;

    bool start(quint16 port)
    {
        if (!dir.isValid() || !database.open(dir.filePath(QStringLiteral("paletten.db")))) {
            return false;
        }
        settings.resetToDefaults();
        service.setAutoSchedule(false);
        if (!service.openStore(dir.filePath(QStringLiteral("paletten.db")))) {
            return false;
        }
        multipack::core::GlobalState::instance().clear();
        window = std::make_unique<multipack::ui::MainWindow>();
        window->setSettingsManager(&settings);
        window->setDatabaseManager(&database);
        window->setGlobalState(&multipack::core::GlobalState::instance());
        window->setPlannerSync(&service);

        QSignalSpy cycle(&service, &PlannerSyncService::cycleFinished);
        service.setConfig(configFor(port), dir.path());
        return cycle.wait(10000);
    }

    void robot(multipack::robot::ProgramState state, bool connected = true)
    {
        multipack::robot::RobotStatus status;
        status.isConnected = connected;
        status.programState = state;
        status.lastUpdate = QDateTime::currentDateTime();
        window->updateStatusTab(status);
    }

    multipack::ui::PlanProposalDialog* dialog() const
    {
        auto* d = window->findChild<multipack::ui::PlanProposalDialog*>();
        return d && d->isVisible() ? d : nullptr;
    }

    QString activeFile() const { return multipack::core::GlobalState::instance().currentFileName(); }
};

FakePlannerServer::Handler proposalHandler(int* answerStatus)
{
    return [answerStatus](const FakePlannerServer::Request& r) {
        if (r.path == "/api/hmi/v1/finals") {
            return FakePlannerServer::Reply::json(200, finalsBody({finalJson(kFinalA, "p", "Alpha", robA())}));
        }
        if (r.path.endsWith("/rob")) {
            FakePlannerServer::Reply reply;
            reply.body = robA();
            return reply;
        }
        if (r.method == "GET" && r.path == "/api/hmi/v1/proposal") {
            return FakePlannerServer::Reply::json(200, proposalBody(kFinalA, robA()));
        }
        if (r.method == "POST" && r.path.startsWith("/api/hmi/v1/proposal/")) {
            if (*answerStatus == 409) {
                return FakePlannerServer::Reply::json(409, "{\"error\":\"proposal_closed\",\"state\":\"cancelled\"}");
            }
            return FakePlannerServer::Reply::empty(*answerStatus);
        }
        return FakePlannerServer::Reply::empty(204);
    };
}

} // namespace

void PlannerSyncTest::proposalNeverSwitchesWithoutOperator()
{
    qputenv("MULTIPACK_DISABLE_ROBOT_STATUS_MONITOR", "1");
    int answerStatus = 204;
    m_server->setHandler(proposalHandler(&answerStatus));

    WindowFixture f;
    // Robot is running before the proposal arrives.
    QVERIFY(f.start(m_server->serverPort()));
    f.robot(multipack::robot::ProgramState::Playing);
    QVERIFY(f.service.currentProposal().has_value());

    // Running: no dialog, no switch, button shows it is waiting.
    QVERIFY(!f.dialog());
    QVERIFY(f.activeFile().isEmpty());
    auto* button = f.window->findChild<QPushButton*>(QStringLiteral("plannerProposalButton"));
    QVERIFY(button);
    QVERIFY(!button->isEnabled());

    // Robot state unknown (dashboard lost): still no automatic dialog.
    f.robot(multipack::robot::ProgramState::Stopped, false);
    QVERIFY(!f.dialog());

    // Robot stopped: the dialog asks the operator - nothing switches by itself.
    f.robot(multipack::robot::ProgramState::Stopped);
    QVERIFY(f.dialog());
    QVERIFY(runCycle(f.service));
    QTest::qWait(100);
    QVERIFY(f.activeFile().isEmpty());
    QVERIFY(m_server->requestsFor("POST", "/api/hmi/v1/proposal/").isEmpty());

    // Program starts while the dialog is open: takeover disabled.
    f.robot(multipack::robot::ProgramState::Playing);
    auto* accept = f.dialog()->findChild<QPushButton*>(QStringLiteral("proposalAccept"));
    QVERIFY(accept && !accept->isEnabled());

    // "Spaeter": dialog closes, proposal stays open, no answer sent.
    f.robot(multipack::robot::ProgramState::Stopped);
    auto* later = f.dialog()->findChild<QPushButton*>(QStringLiteral("proposalLater"));
    QTest::mouseClick(later, Qt::LeftButton);
    QTRY_VERIFY(!f.dialog());
    QVERIFY(m_server->requestsFor("POST", "/api/hmi/v1/proposal/").isEmpty());
    QVERIFY(f.activeFile().isEmpty());
    f.window.reset();
}

void PlannerSyncTest::proposalAcceptSwitchesOnlyAfterConfirmation()
{
    qputenv("MULTIPACK_DISABLE_ROBOT_STATUS_MONITOR", "1");
    int answerStatus = 500;
    m_server->setHandler(proposalHandler(&answerStatus));

    WindowFixture f;
    QVERIFY(f.start(m_server->serverPort()));
    f.robot(multipack::robot::ProgramState::Stopped);
    QVERIFY(f.dialog());
    auto* accept = f.dialog()->findChild<QPushButton*>(QStringLiteral("proposalAccept"));
    QVERIFY(accept && accept->isEnabled());

    // Answer not delivered (5xx): no switch, dialog stays for a retry.
    QSignalSpy answered(&f.service, &PlannerSyncService::proposalAnswered);
    QTest::mouseClick(accept, Qt::LeftButton);
    QVERIFY(answered.wait(10000));
    QVERIFY(f.dialog());
    QVERIFY(f.activeFile().isEmpty());

    // Delivered (204): switch, then report the new active plan.
    answerStatus = 204;
    const int statusBefore = m_server->requestsFor("POST", "/api/hmi/v1/status").size();
    QTest::mouseClick(accept, Qt::LeftButton);
    QVERIFY(answered.wait(10000));
    QTRY_COMPARE(f.activeFile(), multipack::planner::plannerPaletteFileName(kFinalA));
    QVERIFY(!f.dialog());

    QTRY_VERIFY(m_server->requestsFor("POST", "/api/hmi/v1/status").size() > statusBefore);
    const QJsonObject status = QJsonDocument::fromJson(
        m_server->requestsFor("POST", "/api/hmi/v1/status").last().body).object();
    QCOMPARE(status.value("activeFinalId").toString(), kFinalA);

    // Order: the answer was sent before the status that reports the switch.
    int answerIndex = -1;
    int statusIndex = -1;
    for (int i = 0; i < m_server->requests.size(); ++i) {
        const auto& r = m_server->requests[i];
        if (r.method == "POST" && r.path.startsWith("/api/hmi/v1/proposal/") && answerIndex < 0) {
            answerIndex = i;
        }
        if (r.method == "POST" && r.path == "/api/hmi/v1/status"
            && QJsonDocument::fromJson(r.body).object().value("activeFinalId").toString() == kFinalA) {
            statusIndex = i;
        }
    }
    QVERIFY(answerIndex >= 0 && statusIndex > answerIndex);
    f.window.reset();
}

void PlannerSyncTest::proposalClosedDoesNotSwitch()
{
    qputenv("MULTIPACK_DISABLE_ROBOT_STATUS_MONITOR", "1");
    int answerStatus = 409;
    m_server->setHandler(proposalHandler(&answerStatus));

    WindowFixture f;
    QVERIFY(f.start(m_server->serverPort()));
    f.robot(multipack::robot::ProgramState::Stopped);
    QVERIFY(f.dialog());

    QSignalSpy answered(&f.service, &PlannerSyncService::proposalAnswered);
    QTest::mouseClick(f.dialog()->findChild<QPushButton*>(QStringLiteral("proposalAccept")), Qt::LeftButton);
    QVERIFY(answered.wait(10000));
    QTRY_VERIFY(!f.dialog());
    QVERIFY(f.activeFile().isEmpty());
    f.window.reset();
}

void PlannerSyncTest::proposalConfirmedWhileRunningDoesNotSwitch()
{
    qputenv("MULTIPACK_DISABLE_ROBOT_STATUS_MONITOR", "1");
    int answerStatus = 204;
    m_server->setHandler(proposalHandler(&answerStatus));

    WindowFixture f;
    QVERIFY(f.start(m_server->serverPort()));
    f.robot(multipack::robot::ProgramState::Stopped);
    QVERIFY(f.dialog());

    // The operator accepts, then the program starts before the planner confirms.
    QSignalSpy answered(&f.service, &PlannerSyncService::proposalAnswered);
    QTest::mouseClick(f.dialog()->findChild<QPushButton*>(QStringLiteral("proposalAccept")), Qt::LeftButton);
    f.robot(multipack::robot::ProgramState::Playing);
    QVERIFY(answered.wait(10000));
    QCOMPARE(answered.first().at(2).value<PlannerSyncService::AnswerOutcome>(),
             PlannerSyncService::AnswerOutcome::Confirmed);
    QVERIFY(f.activeFile().isEmpty());
    QTRY_VERIFY(f.window->findChild<QMessageBox*>(QStringLiteral("proposalNotSwitchedNotice")));
    f.window.reset();
}

void PlannerSyncTest::manualLoadBlockedWhileProgramRuns()
{
    qputenv("MULTIPACK_DISABLE_ROBOT_STATUS_MONITOR", "1");
    int answerStatus = 204;
    auto handler = proposalHandler(&answerStatus);
    m_server->setHandler([handler](const FakePlannerServer::Request& r) {
        if (r.method == "GET" && r.path == "/api/hmi/v1/proposal") {
            return FakePlannerServer::Reply::empty(204);  // no proposal, only the list
        }
        return handler(r);
    });

    WindowFixture f;
    QVERIFY(f.start(m_server->serverPort()));
    auto* list = f.window->findChild<QListWidget*>(QStringLiteral("plannerFinalsList"));
    auto* load = f.window->findChild<QPushButton*>(QStringLiteral("plannerLoadButton"));
    QVERIFY(list && load);
    QCOMPARE(list->count(), 1);
    list->setCurrentRow(0);
    QVERIFY(load->isEnabled());

    // Program running (or paused): neither the button nor a double-click switches.
    f.robot(multipack::robot::ProgramState::Playing);
    QTest::mouseClick(load, Qt::LeftButton);
    emit list->itemDoubleClicked(list->item(0));
    QVERIFY(f.activeFile().isEmpty());
    f.robot(multipack::robot::ProgramState::Paused);
    QTest::mouseClick(load, Qt::LeftButton);
    QVERIFY(f.activeFile().isEmpty());

    // Stopped: the operator's load switches.
    f.robot(multipack::robot::ProgramState::Stopped);
    QTest::mouseClick(load, Qt::LeftButton);
    QCOMPARE(f.activeFile(), multipack::planner::plannerPaletteFileName(kFinalA));
    f.window.reset();
}

namespace {

/// One Fertigstand (kFinalA) served with `bytes`; listed or only proposed.
FakePlannerServer::Handler planHandler(QByteArray bytes, bool listed, bool propose, int* answerStatus)
{
    return [bytes, listed, propose, answerStatus](const FakePlannerServer::Request& r) {
        if (r.path == "/api/hmi/v1/finals") {
            return FakePlannerServer::Reply::json(
                200, finalsBody(listed ? QJsonArray{finalJson(kFinalA, "p", "Alpha", bytes)} : QJsonArray{}));
        }
        if (r.path.endsWith("/rob")) {
            FakePlannerServer::Reply reply;
            reply.body = bytes;
            return reply;
        }
        if (r.method == "GET" && r.path == "/api/hmi/v1/proposal") {
            return propose ? FakePlannerServer::Reply::json(200, proposalBody(kFinalA, bytes))
                           : FakePlannerServer::Reply::empty(204);
        }
        if (r.method == "POST" && r.path.startsWith("/api/hmi/v1/proposal/")) {
            if (*answerStatus == 0) {
                return FakePlannerServer::Reply::hang();
            }
            return FakePlannerServer::Reply::empty(*answerStatus);
        }
        return FakePlannerServer::Reply::empty(204);
    };
}

QStringList answersSent(const FakePlannerServer& server)
{
    QStringList results;
    for (const auto& request : server.requestsFor("POST", "/api/hmi/v1/proposal/")) {
        results.append(QJsonDocument::fromJson(request.body).object().value("result").toString());
    }
    return results;
}

QStringList g_messages;
QtMessageHandler g_previousHandler = nullptr;
void captureMessages(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    g_messages.append(message);
    if (g_previousHandler) {
        g_previousHandler(type, context, message);
    }
}

} // namespace

void PlannerSyncTest::plannerFixtureWithSpacesIsStored()
{
    int answerStatus = 204;
    const QByteArray fixture = testhelpers::plannerFixtureRobCrlf();
    m_server->setHandler(planHandler(fixture, true, false, &answerStatus));
    startService(configFor(m_server->serverPort()));
    const auto a = m_service->store().find(kFinalA);
    QVERIFY(a.has_value());
    QVERIFY(a->downloaded);
    QVERIFY(a->loadError.isEmpty());
    QCOMPARE(m_service->store().robBytes(kFinalA), fixture);
}

void PlannerSyncTest::unloadableFinalShowsErrorAndProposalIsRejected()
{
    qputenv("MULTIPACK_DISABLE_ROBOT_STATUS_MONITOR", "1");
    int answerStatus = 500;  // first answer gets lost
    const QByteArray garbage = "<html>not a plan</html>";
    m_server->setHandler(planHandler(garbage, true, true, &answerStatus));

    WindowFixture f;
    QSignalSpy unloadable(&f.service, &PlannerSyncService::proposalUnloadable);
    QVERIFY(f.start(m_server->serverPort()));
    f.robot(multipack::robot::ProgramState::Stopped);

    // The list shows an error, not "loading" forever.
    auto* list = f.window->findChild<QListWidget*>(QStringLiteral("plannerFinalsList"));
    QVERIFY(list);
    QCOMPARE(list->count(), 1);
    QVERIFY2(list->item(0)->text().contains(QStringLiteral("Fehler")), qPrintable(list->item(0)->text()));
    QVERIFY(!list->item(0)->text().contains(QStringLiteral("wird geladen")));

    // The proposal is answered "rejected" and the operator is told; no dialog, no switch.
    QTRY_COMPARE(answersSent(*m_server), QStringList{QStringLiteral("rejected")});
    QCOMPARE(unloadable.count(), 1);
    QCOMPARE(unloadable.first().at(1).toString(), QString::fromLatin1(multipack::planner::LOAD_ERROR_INVALID_ROB));
    QTRY_VERIFY(f.window->findChild<QMessageBox*>(QStringLiteral("proposalUnloadableNotice")));
    QVERIFY(!f.dialog());
    QVERIFY(f.activeFile().isEmpty());

    // The answer was lost (500): the next cycle answers again, without a second notice.
    answerStatus = 204;
    QVERIFY(runCycle(f.service));
    QTRY_COMPARE(answersSent(*m_server).size(), 2);
    QCOMPARE(answersSent(*m_server).last(), QStringLiteral("rejected"));
    QCOMPARE(unloadable.count(), 1);
    QCOMPARE(f.window->findChildren<QMessageBox*>(QStringLiteral("proposalUnloadableNotice")).size(), 1);

    // Answered: no further answers.
    QVERIFY(runCycle(f.service));
    QTest::qWait(100);
    QCOMPARE(answersSent(*m_server).size(), 2);
    f.window.reset();
}

void PlannerSyncTest::proposalForUnknownUnloadableFinalIsRejected()
{
    int answerStatus = 204;
    const QByteArray garbage = "1 2 3\nnot a plan";
    m_server->setHandler(planHandler(garbage, false, true, &answerStatus));

    m_service = std::make_unique<PlannerSyncService>();
    m_service->setAutoSchedule(false);
    QVERIFY(m_service->openStore(m_dir->filePath(QStringLiteral("paletten.db"))));
    QSignalSpy unloadable(m_service.get(), &PlannerSyncService::proposalUnloadable);
    QSignalSpy available(m_service.get(), &PlannerSyncService::proposalAvailable);
    QSignalSpy answered(m_service.get(), &PlannerSyncService::proposalAnswered);
    QSignalSpy cycle(m_service.get(), &PlannerSyncService::cycleFinished);
    m_service->setConfig(configFor(m_server->serverPort()), m_dir->path());
    QVERIFY(cycle.wait(10000));

    QVERIFY(answered.count() > 0 || answered.wait(10000));
    QCOMPARE(answersSent(*m_server), QStringList{QStringLiteral("rejected")});
    QCOMPARE(unloadable.count(), 1);
    QCOMPARE(available.count(), 0);
    QVERIFY(!m_service->currentProposal().has_value());
    QCOMPARE(m_service->store().find(kFinalA)->loadError,
             QString::fromLatin1(multipack::planner::LOAD_ERROR_INVALID_ROB));
}

void PlannerSyncTest::unloadableNoticeBeforeWindowIsShown()
{
    // At start-up the first cycle runs while the splash screen processes
    // events, before MainWindow is connected to the service.
    qputenv("MULTIPACK_DISABLE_ROBOT_STATUS_MONITOR", "1");
    int answerStatus = 204;
    m_server->setHandler(planHandler("<html>not a plan</html>", true, true, &answerStatus));
    startService(configFor(m_server->serverPort()));
    QTRY_COMPARE(answersSent(*m_server), QStringList{QStringLiteral("rejected")});

    multipack::config::SettingsManager settings;
    settings.resetToDefaults();
    multipack::database::DatabaseManager database;
    QVERIFY(database.open(m_dir->filePath(QStringLiteral("paletten.db"))));
    auto window = std::make_unique<multipack::ui::MainWindow>();
    window->setSettingsManager(&settings);
    window->setDatabaseManager(&database);
    window->setPlannerSync(m_service.get());
    QCOMPARE(window->findChildren<QMessageBox*>(QStringLiteral("proposalUnloadableNotice")).size(), 1);
    // Shown once: a second window (or reconnect) gets nothing new.
    window->setPlannerSync(nullptr);
    window->setPlannerSync(m_service.get());
    QCOMPARE(window->findChildren<QMessageBox*>(QStringLiteral("proposalUnloadableNotice")).size(), 1);
}

void PlannerSyncTest::answerAgainstHangingServerGivesUpQuickly()
{
    int answerStatus = 0;  // POST answer hangs
    m_server->setHandler(planHandler(robA(), true, false, &answerStatus));
    startService(configFor(m_server->serverPort()));

    g_messages.clear();
    g_previousHandler = qInstallMessageHandler(captureMessages);
    QSignalSpy answered(m_service.get(), &PlannerSyncService::proposalAnswered);
    QElapsedTimer timer;
    timer.start();
    m_service->answerProposal(kProposal, true);
    QVERIFY(answered.wait(15000));
    const qint64 elapsed = timer.elapsed();
    qInstallMessageHandler(g_previousHandler);

    QCOMPARE(answered.first().at(2).value<PlannerSyncService::AnswerOutcome>(),
             PlannerSyncService::AnswerOutcome::NotDelivered);
    QVERIFY2(elapsed < 12000, qPrintable(QString::number(elapsed)));
    // The aborted reply is not read ("QIODevice::read: device not open").
    for (const QString& message : g_messages) {
        QVERIFY2(!message.contains(QStringLiteral("device not open")), qPrintable(message));
    }
}

void PlannerSyncTest::refusalMessageIsNotClipped()
{
    qputenv("MULTIPACK_DISABLE_ROBOT_STATUS_MONITOR", "1");
    int answerStatus = 204;
    m_server->setHandler(planHandler(robA(), true, false, &answerStatus));

    WindowFixture f;
    QVERIFY(f.start(m_server->serverPort()));
    auto* list = f.window->findChild<QListWidget*>(QStringLiteral("plannerFinalsList"));
    list->setCurrentRow(0);
    f.robot(multipack::robot::ProgramState::Playing);
    QTest::mouseClick(f.window->findChild<QPushButton*>(QStringLiteral("plannerLoadButton")), Qt::LeftButton);

    auto* label = f.window->findChild<QLabel*>(QStringLiteral("label_GewichtInfo"));
    QVERIFY(label);
    QVERIFY(label->text().endsWith(QStringLiteral("möglich.")));
    const QRect needed = QFontMetrics(label->font()).boundingRect(
        QRect(0, 0, label->width(), 10'000), Qt::TextWordWrap, label->text());
    QVERIFY2(needed.height() <= label->height(),
             qPrintable(QStringLiteral("%1 > %2").arg(needed.height()).arg(label->height())));
    QVERIFY(label->wordWrap());
    f.window.reset();
}

void PlannerSyncTest::proposalDialogShowsRealPackageCount()
{
    qputenv("MULTIPACK_DISABLE_ROBOT_STATUS_MONITOR", "1");
    int answerStatus = 204;
    m_server->setHandler(planHandler(testhelpers::plannerFixtureRobCrlf(), true, true, &answerStatus));

    WindowFixture f;
    QVERIFY(f.start(m_server->serverPort()));
    f.robot(multipack::robot::ProgramState::Stopped);
    QVERIFY(f.dialog());

    QLabel* count = nullptr;
    for (QLabel* label : f.dialog()->findChildren<QLabel*>()) {
        if (label->text() == QStringLiteral("Pakete:")) {
            count = qobject_cast<QLabel*>(label->buddy());
        }
    }
    QVERIFY(count);
    // The planner's golden data for this fixture: 5 packages (the .rob header says 1).
    QCOMPARE(count->text(), QStringLiteral("5"));
    f.window.reset();
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    qRegisterMetaType<multipack::planner::Proposal>("multipack::planner::Proposal");
    qRegisterMetaType<multipack::planner::Proposal>("Proposal");
    qRegisterMetaType<PlannerSyncService::AnswerOutcome>("multipack::planner::PlannerSyncService::AnswerOutcome");
    PlannerSyncTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_planner_sync.moc"
