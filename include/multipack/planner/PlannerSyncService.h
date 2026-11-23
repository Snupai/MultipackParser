/**
 * @file PlannerSyncService.h
 * @brief HMI-Abgleich: pulls Fertigstände and Planvorschläge from the planner
 *
 * Pull only: the HMI polls the planner over HTTPS, the planner never
 * connects to the HMI. One cycle every 10 s:
 *
 *   1. GET /api/hmi/v1/finals (If-None-Match)  -> update the local list
 *   2. GET .../finals/{id}/rob for every listed Fertigstand not stored yet
 *      -> SHA-256 check, .rob parser, then store (PlannerStore)
 *   3. GET /api/hmi/v1/proposal                 -> Planvorschlag for the UI
 *
 * Errors back off (SyncBackoff). The status (active plan, app version) is
 * posted after every plan change and every 60 s.
 *
 * Syncing only changes the list. This class never switches the running plan;
 * the UI does that after the operator confirms.
 */
#ifndef MULTIPACK_PLANNER_PLANNERSYNCSERVICE_H
#define MULTIPACK_PLANNER_PLANNERSYNCSERVICE_H

#include "multipack/planner/PlannerConfig.h"
#include "multipack/planner/PlannerProtocol.h"
#include "multipack/planner/PlannerStore.h"
#include "multipack/planner/SyncBackoff.h"

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QSslConfiguration>

#include <functional>
#include <optional>

QT_BEGIN_NAMESPACE
class QNetworkAccessManager;
class QNetworkReply;
class QTimer;
QT_END_NAMESPACE

namespace multipack {
namespace planner {

class PlannerSyncService : public QObject
{
    Q_OBJECT

public:
    enum class ConnectionState {
        NotConfigured, ///< No (valid) planner configuration
        Connecting,    ///< Configured, no answer yet
        Online,        ///< Last poll succeeded
        Offline,       ///< Last poll failed (network, 5xx, 429); local copies in use
        NotAuthorized  ///< 401 / server_storage_disabled: "HMI nicht freigeschaltet"
    };
    Q_ENUM(ConnectionState)

    enum class AnswerOutcome {
        Confirmed,    ///< 204: the planner recorded the answer
        Closed,       ///< 404/409: proposal taken back, superseded or expired
        NotDelivered  ///< Network error etc.: keep the dialog, retry
    };
    Q_ENUM(AnswerOutcome)

    static constexpr qint64 STATUS_INTERVAL_MS = 60'000;
    static constexpr qint64 INVALID_ROB_RETRY_MS = 5 * 60'000;
    static constexpr qint64 MAX_JSON_BYTES = 2 * 1024 * 1024;
    static constexpr qint64 MAX_ROB_BYTES = 1024 * 1024;
    static constexpr int TRANSFER_TIMEOUT_MS = 20'000;
    /// The operator waits for this one: give up early and let them retry.
    static constexpr int ANSWER_TIMEOUT_MS = 8'000;

    explicit PlannerSyncService(QObject* parent = nullptr);
    ~PlannerSyncService() override;

    /// Open the local store in the application database.
    [[nodiscard]] bool openStore(const QString& databasePath);

    /**
     * @brief Apply a configuration and (re)start polling if it is valid
     * @param config Configuration from settings.json
     * @param workingDir Base for a relative CA certificate path
     */
    void setConfig(const PlannerConfig& config, const QString& workingDir);

    /// Stop polling and abort requests.
    void stop();

    /// Run a cycle now (resets nothing; used after import and in tests).
    void pollNow();

    /**
     * @brief The running plan changed (after an operator-confirmed switch)
     * @param finalId Fertigstand id, or empty when the plan is not from the planner
     *
     * Reports the status right away.
     */
    void setActivePlan(const QString& finalId);
    [[nodiscard]] QString activeFinalId() const { return m_activeFinalId; }

    /**
     * @brief Report the operator's decision on a Planvorschlag
     *
     * Emits proposalAnswered(). For an acceptance the UI may only switch
     * after AnswerOutcome::Confirmed.
     */
    void answerProposal(const QString& proposalId, bool accepted);

    [[nodiscard]] ConnectionState state() const { return m_state; }
    [[nodiscard]] QString stateText() const;
    [[nodiscard]] QDateTime lastSuccess() const { return m_lastSuccess; }
    [[nodiscard]] const PlannerConfig& config() const { return m_config; }
    [[nodiscard]] bool isConfigured() const { return m_configured; }
    [[nodiscard]] std::optional<Proposal> currentProposal() const { return m_proposal; }

    /// Planvorschläge answered "rejected" because their Fertigstand cannot be
    /// loaded, not yet shown to the operator. Taking them clears the list, so
    /// each is shown once, also when it happened before the UI was connected.
    struct UnloadableNotice {
        Proposal proposal;
        QString loadError;
    };
    [[nodiscard]] QVector<UnloadableNotice> takeUnloadableNotices();
    [[nodiscard]] PlannerStore& store() { return m_store; }
    [[nodiscard]] const PlannerStore& store() const { return m_store; }

    /// Delay before the next cycle as last scheduled (for tests and display).
    [[nodiscard]] qint64 nextDelayMs() const { return m_backoff.currentDelayMs(); }

    /// Tests: deterministic jitter.
    void setJitterSource(SyncBackoff::JitterSource jitter);
    /// Tests: do not start the timer automatically after a cycle.
    void setAutoSchedule(bool enabled) { m_autoSchedule = enabled; }
    /// App version string sent in the status report.
    void setAppVersion(const QString& version) { m_appVersion = version; }

signals:
    void stateChanged(multipack::planner::PlannerSyncService::ConnectionState state);
    /// Local list changed (new entries, withdrawals, downloads).
    void finalsChanged();
    /// A new Planvorschlag is open; its Fertigstand is stored locally.
    void proposalAvailable(const multipack::planner::Proposal& proposal);
    /// The open Planvorschlag is gone (answered, taken back, superseded, expired).
    void proposalCleared(const QString& proposalId);
    /**
     * A Planvorschlag names a Fertigstand that this HMI cannot load (the .rob
     * parser rejects it, or its checksum never matches). The service answers
     * it "rejected"; the UI tells the operator. Emitted once per proposal.
     * @param loadError LOAD_ERROR_* code
     */
    void proposalUnloadable(const multipack::planner::Proposal& proposal, const QString& loadError);
    void proposalAnswered(const QString& proposalId, bool accepted,
                          multipack::planner::PlannerSyncService::AnswerOutcome outcome);
    void statusReported(bool ok);
    /// A full cycle finished (success or failure).
    void cycleFinished(bool success);

private:
    struct Response {
        int httpStatus = 0;
        bool networkError = false;
        QString errorString;
        QByteArray body;
        QByteArray etag;
        QByteArray retryAfter;
        [[nodiscard]] QString errorCode() const { return parseErrorCode(body); }
        [[nodiscard]] ResponseClass classify() const;
    };
    using Callback = std::function<void(const Response&)>;

    void sendRequest(const QByteArray& verb, const QString& path, const QByteArray& body,
                     const QByteArray& ifNoneMatch, qint64 maxBytes, Callback callback,
                     int timeoutMs = TRANSFER_TIMEOUT_MS);

    void scheduleNext(qint64 delayMs);
    void startCycle();
    void onFinalsResponse(const Response& response);
    void downloadNext();
    void requestProposal();
    void onProposalResponse(const Response& response);
    void finishCycle(bool success, const Response* failure = nullptr);
    void maybeSendStatus();
    void sendStatus();
    void setState(ConnectionState state);
    void clearProposal();
    /// The proposed Fertigstand cannot be loaded here: tell the UI and answer "rejected".
    void rejectUnloadable(const Proposal& proposal, const QString& loadError);
    void abortAll();
    [[nodiscard]] bool retryAllowed(const QString& finalId) const;

    QNetworkAccessManager* m_network = nullptr;
    QTimer* m_timer = nullptr;
    PlannerStore m_store;
    PlannerConfig m_config;
    bool m_configured = false;
    QSslConfiguration m_sslConfig;
    SyncBackoff m_backoff;
    ConnectionState m_state = ConnectionState::NotConfigured;
    bool m_cycleRunning = false;
    bool m_autoSchedule = true;
    quint64 m_generation = 0;  ///< Bumped on config change; stale replies are ignored
    QSet<QNetworkReply*> m_inFlight;

    QDateTime m_lastSuccess;
    QHash<QString, QDateTime> m_retryAfter;  ///< finalId -> earliest next download

    std::optional<Proposal> m_proposal;
    QSet<QString> m_answeredProposals;
    QSet<QString> m_unloadableNotified;
    QVector<UnloadableNotice> m_unloadableNotices;
    QSet<QString> m_autoRejectInFlight;
    QString m_activeFinalId;
    QDateTime m_lastStatusSent;
    bool m_statusDirty = true;
    bool m_statusInFlight = false;
    QString m_appVersion;
};

} // namespace planner
} // namespace multipack

#endif // MULTIPACK_PLANNER_PLANNERSYNCSERVICE_H
