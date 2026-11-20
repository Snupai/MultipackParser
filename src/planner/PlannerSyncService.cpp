/**
 * @file PlannerSyncService.cpp
 * @brief HMI-Abgleich: pulls Fertigstände and Planvorschläge from the planner
 */
#include "multipack/planner/PlannerSyncService.h"

#include <QDebug>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

#include <algorithm>
#include <limits>

namespace multipack {
namespace planner {

Q_LOGGING_CATEGORY(plannerLog, "multipack.planner")

namespace {

QString describeFailure(int httpStatus, const QString& errorString, const QString& errorCode)
{
    if (httpStatus == 0) {
        return errorString.isEmpty() ? QStringLiteral("network error") : errorString;
    }
    return errorCode.isEmpty() ? QStringLiteral("HTTP %1").arg(httpStatus)
                               : QStringLiteral("HTTP %1 (%2)").arg(httpStatus).arg(errorCode);
}

} // namespace

ResponseClass PlannerSyncService::Response::classify() const
{
    if (networkError) {
        return ResponseClass::TransientError;
    }
    return classifyResponse(httpStatus, errorCode());
}

PlannerSyncService::PlannerSyncService(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_timer(new QTimer(this))
    , m_sslConfig(QSslConfiguration::defaultConfiguration())
{
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &PlannerSyncService::startCycle);
}

PlannerSyncService::~PlannerSyncService()
{
    // Drop callbacks before aborting so nothing runs into a half-destroyed object.
    m_timer->stop();
    ++m_generation;
    const QSet<QNetworkReply*> replies = m_inFlight;
    m_inFlight.clear();
    for (QNetworkReply* reply : replies) {
        QObject::disconnect(reply, nullptr, this, nullptr);
        reply->abort();
    }
}

bool PlannerSyncService::openStore(const QString& databasePath)
{
    return m_store.open(databasePath);
}

void PlannerSyncService::setJitterSource(SyncBackoff::JitterSource jitter)
{
    m_backoff = SyncBackoff(std::move(jitter));
}

void PlannerSyncService::setConfig(const PlannerConfig& config, const QString& workingDir)
{
    stop();

    ConfigParseResult validated;
    if (config.isComplete()) {
        validated = validateConfig(config);
    }

    if (!validated.ok) {
        if (config.isComplete()) {
            qCWarning(plannerLog) << "Planner configuration rejected:" << validated.error;
        }
        m_configured = false;
        m_config = PlannerConfig();
        clearProposal();
        setState(ConnectionState::NotConfigured);
        return;
    }

    const bool targetChanged = validated.config.serverUrl != m_config.serverUrl
        || validated.config.hmiId != m_config.hmiId;

    m_config = validated.config;
    m_configured = true;

    m_sslConfig = QSslConfiguration::defaultConfiguration();
    const QString caPath = resolveCaCertPath(m_config, workingDir);
    if (!caPath.isEmpty()) {
        const QList<QSslCertificate> certs = loadCaCertificates(caPath);
        if (certs.isEmpty()) {
            // Never fall back to skipping verification: without the CA the
            // system trust store decides, and an untrusted server fails.
            qCWarning(plannerLog) << "Planner CA certificate not loadable:" << caPath;
        } else {
            m_sslConfig.addCaCertificates(certs);
            qCInfo(plannerLog) << "Planner CA certificate loaded:" << certs.size() << "certificate(s)";
        }
    }

    if (targetChanged) {
        m_store.setFinalsEtag(QString());
        m_answeredProposals.clear();
        m_retryAfter.clear();
        clearProposal();
    }

    qCInfo(plannerLog) << "Planner sync configured for" << m_config.serverUrl << "hmi" << m_config.hmiId;
    m_backoff.onSuccess();
    m_statusDirty = true;
    setState(ConnectionState::Connecting);
    scheduleNext(0);
}

void PlannerSyncService::stop()
{
    m_timer->stop();
    abortAll();
    m_cycleRunning = false;
    m_statusInFlight = false;
}

void PlannerSyncService::pollNow()
{
    if (!m_configured || m_cycleRunning) {
        return;
    }
    m_timer->stop();
    startCycle();
}

void PlannerSyncService::abortAll()
{
    ++m_generation;
    const QSet<QNetworkReply*> replies = m_inFlight;
    m_inFlight.clear();
    for (QNetworkReply* reply : replies) {
        reply->abort();
    }
}

QString PlannerSyncService::stateText() const
{
    switch (m_state) {
        case ConnectionState::NotConfigured: return QStringLiteral("Planer nicht konfiguriert");
        case ConnectionState::Connecting: return QStringLiteral("Verbinde mit Planer ...");
        case ConnectionState::Online: return QStringLiteral("Online");
        case ConnectionState::Offline: return QStringLiteral("Offline - lokale Kopien");
        case ConnectionState::NotAuthorized: return QStringLiteral("HMI nicht freigeschaltet");
    }
    return QString();
}

void PlannerSyncService::setState(ConnectionState state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    qCInfo(plannerLog) << "Planner sync state:" << stateText();
    emit stateChanged(state);
}

void PlannerSyncService::scheduleNext(qint64 delayMs)
{
    if (!m_configured) {
        return;
    }
    m_timer->start(static_cast<int>(std::clamp<qint64>(delayMs, 0, std::numeric_limits<int>::max())));
}

void PlannerSyncService::sendRequest(const QByteArray& verb, const QString& path, const QByteArray& body,
                                     const QByteArray& ifNoneMatch, qint64 maxBytes, Callback callback)
{
    const QUrl url = m_config.apiUrl(path);
    if (!m_configured || !isSecureTransport(url)) {
        // The token must never leave over plain HTTP to a remote host.
        Response refused;
        refused.networkError = true;
        refused.errorString = QStringLiteral("refused: insecure or missing planner URL");
        QTimer::singleShot(0, this, [callback, refused]() { callback(refused); });
        return;
    }

    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + m_config.token.toLatin1());
    request.setRawHeader("Accept", "application/json, text/plain");
    if (!ifNoneMatch.isEmpty()) {
        request.setRawHeader("If-None-Match", ifNoneMatch);
    }
    if (!body.isEmpty()) {
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    }
    // Redirects could carry the bearer token elsewhere: never follow them.
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
    request.setTransferTimeout(TRANSFER_TIMEOUT_MS);
    if (url.scheme().compare(QLatin1String("https"), Qt::CaseInsensitive) == 0) {
        request.setSslConfiguration(m_sslConfig);
    }

    QNetworkReply* reply = verb == "GET" ? m_network->get(request)
                                         : m_network->sendCustomRequest(request, verb, body);
    m_inFlight.insert(reply);
    const quint64 generation = m_generation;

    connect(reply, &QNetworkReply::downloadProgress, reply, [reply, maxBytes](qint64 received, qint64 total) {
        if (received > maxBytes || total > maxBytes) {
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::sslErrors, reply, [path](const QList<QSslError>& errors) {
        // Logged only; the connection fails (no ignoreSslErrors anywhere).
        for (const QSslError& error : errors) {
            qCWarning(plannerLog) << "Planner TLS error on" << path << ":" << error.errorString();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation, maxBytes, callback]() {
        m_inFlight.remove(reply);
        reply->deleteLater();

        Response response;
        response.httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        response.body = reply->readAll();
        response.etag = reply->rawHeader("ETag");
        response.retryAfter = reply->rawHeader("Retry-After");
        if (response.httpStatus == 0 || response.body.size() > maxBytes) {
            response.networkError = true;
            response.errorString = response.body.size() > maxBytes ? QStringLiteral("response too large")
                                                                   : reply->errorString();
        }

        if (generation != m_generation) {
            // Configuration changed or service stopped meanwhile.
            response.networkError = true;
            response.errorString = QStringLiteral("cancelled");
            response.httpStatus = 0;
        }
        callback(response);
    });
}

void PlannerSyncService::startCycle()
{
    if (!m_configured || m_cycleRunning) {
        return;
    }
    m_cycleRunning = true;
    const quint64 generation = m_generation;
    sendRequest("GET", QString::fromLatin1(Api::FINALS), QByteArray(), m_store.finalsEtag().toLatin1(),
                MAX_JSON_BYTES, [this, generation](const Response& response) {
                    if (generation != m_generation) {
                        return;
                    }
                    onFinalsResponse(response);
                });
}

void PlannerSyncService::onFinalsResponse(const Response& response)
{
    if (response.classify() != ResponseClass::Ok) {
        finishCycle(false, &response);
        return;
    }

    if (response.httpStatus == 304) {
        downloadNext();
        return;
    }

    const auto entries = parseFinalsResponse(response.body);
    if (!entries.has_value()) {
        qCWarning(plannerLog) << "Planner finals list malformed";
        finishCycle(false);
        return;
    }
    if (!m_store.applyList(*entries)) {
        qCWarning(plannerLog) << "Planner finals list could not be stored";
        finishCycle(false);
        return;
    }
    m_store.setFinalsEtag(QString::fromLatin1(response.etag));
    emit finalsChanged();
    downloadNext();
}

bool PlannerSyncService::retryAllowed(const QString& finalId) const
{
    const auto it = m_retryAfter.constFind(finalId);
    return it == m_retryAfter.constEnd() || QDateTime::currentDateTimeUtc() >= it.value();
}

void PlannerSyncService::downloadNext()
{
    const QVector<FinalEntry> pending = m_store.pendingDownloads();
    const FinalEntry* next = nullptr;
    for (const FinalEntry& entry : pending) {
        if (retryAllowed(entry.id)) {
            next = &entry;
            break;
        }
    }
    if (!next) {
        requestProposal();
        return;
    }

    const FinalEntry entry = *next;
    const quint64 generation = m_generation;
    sendRequest("GET", Api::robPath(entry.id), QByteArray(), QByteArray(), MAX_ROB_BYTES,
                [this, generation, entry](const Response& response) {
                    if (generation != m_generation) {
                        return;
                    }
                    const QDateTime now = QDateTime::currentDateTimeUtc();
                    const ResponseClass cls = response.classify();
                    if (cls == ResponseClass::NotFound) {
                        qCWarning(plannerLog) << "Planner final not found:" << entry.id;
                        m_retryAfter.insert(entry.id, now.addMSecs(SyncBackoff::POLL_INTERVAL_MS));
                        downloadNext();
                        return;
                    }
                    if (cls != ResponseClass::Ok || response.httpStatus != 200) {
                        finishCycle(false, &response);
                        return;
                    }

                    QString error;
                    switch (m_store.storeRob(entry, response.body, &error)) {
                        case StoreOutcome::Stored:
                            m_retryAfter.remove(entry.id);
                            qCInfo(plannerLog) << "Planner final stored:" << entry.id << entry.name;
                            emit finalsChanged();
                            break;
                        case StoreOutcome::ShaMismatch:
                            qCWarning(plannerLog) << "Planner final checksum mismatch, discarded:" << entry.id;
                            m_retryAfter.insert(entry.id, now.addMSecs(SyncBackoff::POLL_INTERVAL_MS));
                            break;
                        case StoreOutcome::InvalidRob:
                            qCWarning(plannerLog) << "Planner final rejected by .rob parser:" << entry.id << error;
                            m_retryAfter.insert(entry.id, now.addMSecs(INVALID_ROB_RETRY_MS));
                            break;
                        case StoreOutcome::DatabaseError:
                            qCWarning(plannerLog) << "Planner final could not be stored:" << entry.id << error;
                            m_retryAfter.insert(entry.id, now.addMSecs(SyncBackoff::POLL_INTERVAL_MS));
                            break;
                    }
                    downloadNext();
                });
}

void PlannerSyncService::requestProposal()
{
    const quint64 generation = m_generation;
    sendRequest("GET", QString::fromLatin1(Api::PROPOSAL), QByteArray(), QByteArray(), MAX_JSON_BYTES,
                [this, generation](const Response& response) {
                    if (generation != m_generation) {
                        return;
                    }
                    onProposalResponse(response);
                });
}

void PlannerSyncService::onProposalResponse(const Response& response)
{
    if (response.classify() != ResponseClass::Ok) {
        finishCycle(false, &response);
        return;
    }
    if (response.httpStatus == 204) {
        clearProposal();
        finishCycle(true);
        return;
    }

    const auto proposal = parseProposalResponse(response.body);
    if (!proposal.has_value()) {
        qCWarning(plannerLog) << "Planner proposal malformed";
        finishCycle(false);
        return;
    }
    if (m_answeredProposals.contains(proposal->id)) {
        finishCycle(true);
        return;
    }

    auto publish = [this](const Proposal& p) {
        if (m_proposal && m_proposal->id == p.id) {
            return;
        }
        if (m_proposal) {
            const QString previous = m_proposal->id;
            m_proposal.reset();
            emit proposalCleared(previous);
        }
        m_proposal = p;
        qCInfo(plannerLog) << "Planner proposal open:" << p.id << "final" << p.finalId;
        emit proposalAvailable(p);
    };

    const auto stored = m_store.find(proposal->finalId);
    if (stored && stored->downloaded
        && stored->entry.sha256.compare(proposal->sha256, Qt::CaseInsensitive) == 0) {
        publish(*proposal);
        finishCycle(true);
        return;
    }

    // The proposed Fertigstand is not here yet: fetch and verify it first.
    FinalEntry entry;
    if (stored) {
        entry = stored->entry;
    }
    entry.id = proposal->finalId;
    entry.sha256 = proposal->sha256;
    if (entry.name.isEmpty()) {
        entry.name = proposal->name;
    }
    if (!retryAllowed(entry.id)) {
        finishCycle(true);
        return;
    }

    const quint64 generation = m_generation;
    const Proposal p = *proposal;
    sendRequest("GET", Api::robPath(entry.id), QByteArray(), QByteArray(), MAX_ROB_BYTES,
                [this, generation, entry, p, publish](const Response& robResponse) {
                    if (generation != m_generation) {
                        return;
                    }
                    const ResponseClass cls = robResponse.classify();
                    if (cls == ResponseClass::NotFound) {
                        m_retryAfter.insert(entry.id, QDateTime::currentDateTimeUtc().addMSecs(SyncBackoff::POLL_INTERVAL_MS));
                        finishCycle(true);
                        return;
                    }
                    if (cls != ResponseClass::Ok || robResponse.httpStatus != 200) {
                        finishCycle(false, &robResponse);
                        return;
                    }
                    QString error;
                    const StoreOutcome outcome = m_store.storeRob(entry, robResponse.body, &error);
                    if (outcome == StoreOutcome::Stored) {
                        emit finalsChanged();
                        publish(p);
                    } else {
                        qCWarning(plannerLog) << "Proposed final not stored:" << entry.id << error;
                        m_retryAfter.insert(entry.id, QDateTime::currentDateTimeUtc().addMSecs(
                            outcome == StoreOutcome::InvalidRob ? INVALID_ROB_RETRY_MS : SyncBackoff::POLL_INTERVAL_MS));
                    }
                    finishCycle(true);
                });
}

void PlannerSyncService::finishCycle(bool success, const Response* failure)
{
    m_cycleRunning = false;
    qint64 delay = SyncBackoff::POLL_INTERVAL_MS;

    if (success) {
        m_lastSuccess = QDateTime::currentDateTimeUtc();
        delay = m_backoff.onSuccess();
        setState(ConnectionState::Online);
        maybeSendStatus();
    } else {
        const ResponseClass cls = failure ? failure->classify() : ResponseClass::TransientError;
        const QString reason = failure
            ? describeFailure(failure->httpStatus, failure->errorString, failure->errorCode())
            : QStringLiteral("invalid response");
        switch (cls) {
            case ResponseClass::NotAuthorized:
                delay = m_backoff.onNotAuthorized();
                clearProposal();
                setState(ConnectionState::NotAuthorized);
                break;
            case ResponseClass::RateLimited:
                delay = m_backoff.onRateLimited(
                    parseRetryAfterMs(failure->retryAfter, QDateTime::currentDateTimeUtc()));
                break;
            default:
                delay = m_backoff.onTransientFailure();
                setState(ConnectionState::Offline);
                break;
        }
        qCWarning(plannerLog) << "Planner sync failed:" << reason << "- next attempt in" << delay / 1000 << "s";
    }

    emit cycleFinished(success);
    if (m_autoSchedule) {
        scheduleNext(delay);
    }
}

void PlannerSyncService::clearProposal()
{
    if (!m_proposal) {
        return;
    }
    const QString id = m_proposal->id;
    m_proposal.reset();
    emit proposalCleared(id);
}

void PlannerSyncService::setActivePlan(const QString& finalId)
{
    // Report even when the id is unchanged: the operator switched (reloaded).
    m_activeFinalId = finalId;
    m_statusDirty = true;
    if (m_configured && m_state != ConnectionState::NotAuthorized && !m_statusInFlight) {
        sendStatus();
    }
}

void PlannerSyncService::maybeSendStatus()
{
    if (!m_configured || m_state == ConnectionState::NotAuthorized || m_statusInFlight) {
        return;
    }
    const QDateTime now = QDateTime::currentDateTimeUtc();
    if (m_statusDirty || !m_lastStatusSent.isValid() || m_lastStatusSent.msecsTo(now) >= STATUS_INTERVAL_MS) {
        sendStatus();
    }
}

void PlannerSyncService::sendStatus()
{
    QString sha;
    if (!m_activeFinalId.isEmpty()) {
        const auto stored = m_store.find(m_activeFinalId);
        if (stored && stored->downloaded) {
            sha = stored->entry.sha256;
        }
    }
    const QString sentFinalId = m_activeFinalId;
    const QByteArray body = buildStatusBody(sha.isEmpty() ? QString() : sentFinalId, sha, m_appVersion);

    m_statusInFlight = true;
    const quint64 generation = m_generation;
    sendRequest("POST", QString::fromLatin1(Api::STATUS), body, QByteArray(), MAX_JSON_BYTES,
                [this, generation, sentFinalId](const Response& response) {
                    if (generation != m_generation) {
                        return;
                    }
                    m_statusInFlight = false;
                    const ResponseClass cls = response.classify();
                    const bool ok = cls == ResponseClass::Ok;
                    if (ok || cls == ResponseClass::ClientError) {
                        // A rejected body is not retried every cycle, only with the interval.
                        m_lastStatusSent = QDateTime::currentDateTimeUtc();
                        m_statusDirty = sentFinalId != m_activeFinalId;
                    }
                    if (!ok) {
                        qCWarning(plannerLog) << "Planner status report failed:"
                                              << describeFailure(response.httpStatus, response.errorString,
                                                                 response.errorCode());
                    }
                    if (cls == ResponseClass::NotAuthorized) {
                        clearProposal();
                        setState(ConnectionState::NotAuthorized);
                    }
                    emit statusReported(ok);
                    if (ok && m_statusDirty) {
                        // The active plan changed while this report was in flight.
                        sendStatus();
                    }
                });
}

void PlannerSyncService::answerProposal(const QString& proposalId, bool accepted)
{
    const QByteArray body = buildProposalAnswerBody(accepted);
    sendRequest("POST", Api::proposalAnswerPath(proposalId), body, QByteArray(), MAX_JSON_BYTES,
                [this, proposalId, accepted](const Response& response) {
                    AnswerOutcome outcome = AnswerOutcome::NotDelivered;
                    switch (response.classify()) {
                        case ResponseClass::Ok:
                            outcome = response.httpStatus == 204 || response.httpStatus == 200
                                ? AnswerOutcome::Confirmed : AnswerOutcome::NotDelivered;
                            break;
                        case ResponseClass::Conflict:
                        case ResponseClass::NotFound:
                            outcome = AnswerOutcome::Closed;
                            break;
                        case ResponseClass::NotAuthorized:
                            setState(ConnectionState::NotAuthorized);
                            break;
                        default:
                            break;
                    }

                    qCInfo(plannerLog) << "Planner proposal answer" << proposalId
                                       << (accepted ? "accepted" : "rejected") << "->"
                                       << describeFailure(response.httpStatus, response.errorString,
                                                          response.errorCode());
                    if (outcome != AnswerOutcome::NotDelivered) {
                        m_answeredProposals.insert(proposalId);
                        if (m_proposal && m_proposal->id == proposalId) {
                            m_proposal.reset();
                        }
                    }
                    emit proposalAnswered(proposalId, accepted, outcome);
                });
}

} // namespace planner
} // namespace multipack
