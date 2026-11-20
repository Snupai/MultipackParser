/**
 * @file PlannerProtocol.h
 * @brief HMI API v1 wire format (pure functions, no networking)
 *
 * Contract: docs/hmi-api-v1.md in the palette_3d_viewer repository.
 */
#ifndef MULTIPACK_PLANNER_PLANNERPROTOCOL_H
#define MULTIPACK_PLANNER_PLANNERPROTOCOL_H

#include <QByteArray>
#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QVector>

#include <optional>

namespace multipack {
namespace planner {

namespace Api {
constexpr const char* FINALS = "/api/hmi/v1/finals";
constexpr const char* PROPOSAL = "/api/hmi/v1/proposal";
constexpr const char* STATUS = "/api/hmi/v1/status";
QString robPath(const QString& finalId);
QString proposalAnswerPath(const QString& proposalId);
} // namespace Api

/**
 * @struct FinalEntry
 * @brief One Fertigstand from `GET /api/hmi/v1/finals`
 */
struct FinalEntry {
    QString id;         ///< Immutable Fertigstand id (UUID)
    QString recordId;   ///< Plan (project) id across versions
    QString name;
    QString fileName;
    QString sha256;     ///< Lower-case hex
    QDateTime markedAt; ///< UTC
};

/**
 * @struct Proposal
 * @brief Open Planvorschlag from `GET /api/hmi/v1/proposal`
 */
struct Proposal {
    QString id;
    QString finalId;
    QString name;
    QString sha256;
    QString proposedBy;  ///< Empty when the account was deleted
    QDateTime createdAt;
};

/**
 * @brief Parse the finals list body
 * @return nullopt when the body is malformed; entries with invalid fields are skipped
 */
[[nodiscard]] std::optional<QVector<FinalEntry>> parseFinalsResponse(const QByteArray& body);

/**
 * @brief Parse a `200` proposal body; nullopt when malformed
 */
[[nodiscard]] std::optional<Proposal> parseProposalResponse(const QByteArray& body);

/**
 * @brief JSON body for `POST /api/hmi/v1/status`
 * @param activeFinalId Empty for "no planner plan" (sent as null)
 * @param activeSha256 Empty for null
 */
[[nodiscard]] QByteArray buildStatusBody(const QString& activeFinalId,
                                         const QString& activeSha256,
                                         const QString& appVersion);

/**
 * @brief JSON body for `POST /api/hmi/v1/proposal/{id}`
 */
[[nodiscard]] QByteArray buildProposalAnswerBody(bool accepted);

/**
 * @brief Lower-case hex SHA-256 over the raw bytes
 */
[[nodiscard]] QString sha256Hex(const QByteArray& bytes);

/**
 * @brief Case-insensitive comparison of two hex digests (both must be 64 hex chars)
 */
[[nodiscard]] bool sha256Matches(const QString& expected, const QByteArray& bytes);

/**
 * @brief Parse a `Retry-After` header (delta seconds or HTTP date)
 * @param value Header value
 * @param now Current time, for the HTTP-date form
 * @return Delay in milliseconds, or nullopt when absent/invalid
 */
[[nodiscard]] std::optional<qint64> parseRetryAfterMs(const QByteArray& value, const QDateTime& now);

/**
 * @brief Extract `error` from a JSON error body (empty if absent)
 */
[[nodiscard]] QString parseErrorCode(const QByteArray& body);

/**
 * @enum ResponseClass
 * @brief How the sync loop reacts to an HTTP outcome
 */
enum class ResponseClass {
    Ok,             ///< 2xx / 304
    NotAuthorized,  ///< 401, or 404 server_storage_disabled: "HMI nicht freigeschaltet"
    RateLimited,    ///< 429
    NotFound,       ///< Other 404 (final_not_found, proposal_not_found)
    Conflict,       ///< 409 (proposal_closed)
    TransientError, ///< Network error, 5xx, timeouts, unexpected status
    ClientError     ///< Other 4xx (e.g. 400 invalid_request)
};

/**
 * @brief Classify an HTTP outcome
 * @param httpStatus HTTP status (0 when there was no HTTP response)
 * @param errorCode Value of the JSON `error` field (may be empty)
 */
[[nodiscard]] ResponseClass classifyResponse(int httpStatus, const QString& errorCode);

} // namespace planner
} // namespace multipack

Q_DECLARE_METATYPE(multipack::planner::Proposal)
Q_DECLARE_METATYPE(multipack::planner::FinalEntry)

#endif // MULTIPACK_PLANNER_PLANNERPROTOCOL_H
