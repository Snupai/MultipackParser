/**
 * @file PlannerProtocol.cpp
 * @brief HMI API v1 wire format (pure functions, no networking)
 */
#include "multipack/planner/PlannerProtocol.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrl>
#include <QUuid>

#include <algorithm>

namespace multipack {
namespace planner {

namespace {

bool isUuid(const QString& value)
{
    return !value.isEmpty() && !value.startsWith('{') && !QUuid::fromString(value).isNull();
}

bool isSha256Hex(const QString& value)
{
    static const QRegularExpression pattern(QStringLiteral("^[0-9a-fA-F]{64}$"));
    return pattern.match(value).hasMatch();
}

QDateTime parseIsoUtc(const QJsonValue& value)
{
    if (!value.isString()) {
        return QDateTime();
    }
    QDateTime dt = QDateTime::fromString(value.toString(), Qt::ISODateWithMs);
    if (!dt.isValid()) {
        dt = QDateTime::fromString(value.toString(), Qt::ISODate);
    }
    return dt.isValid() ? dt.toUTC() : QDateTime();
}

} // namespace

namespace Api {

QString robPath(const QString& finalId)
{
    return QStringLiteral("/api/hmi/v1/finals/%1/rob").arg(QString::fromLatin1(QUrl::toPercentEncoding(finalId)));
}

QString proposalAnswerPath(const QString& proposalId)
{
    return QStringLiteral("/api/hmi/v1/proposal/%1").arg(QString::fromLatin1(QUrl::toPercentEncoding(proposalId)));
}

} // namespace Api

std::optional<QVector<FinalEntry>> parseFinalsResponse(const QByteArray& body)
{
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        return std::nullopt;
    }
    const QJsonValue finalsValue = doc.object().value(QStringLiteral("finals"));
    if (!finalsValue.isArray()) {
        return std::nullopt;
    }

    QVector<FinalEntry> entries;
    for (const QJsonValue& value : finalsValue.toArray()) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject object = value.toObject();
        FinalEntry entry;
        entry.id = object.value(QStringLiteral("id")).toString();
        entry.recordId = object.value(QStringLiteral("recordId")).toString();
        entry.name = object.value(QStringLiteral("name")).toString();
        entry.fileName = object.value(QStringLiteral("fileName")).toString();
        entry.sha256 = object.value(QStringLiteral("sha256")).toString().toLower();
        entry.markedAt = parseIsoUtc(object.value(QStringLiteral("markedAt")));

        // The id becomes part of a URL path and a database key: only accept UUIDs.
        if (!isUuid(entry.id) || entry.recordId.isEmpty() || !isSha256Hex(entry.sha256)) {
            continue;
        }
        if (entry.name.trimmed().isEmpty()) {
            entry.name = entry.fileName;
        }
        entries.append(entry);
    }
    return entries;
}

std::optional<Proposal> parseProposalResponse(const QByteArray& body)
{
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        return std::nullopt;
    }
    const QJsonValue proposalValue = doc.object().value(QStringLiteral("proposal"));
    if (!proposalValue.isObject()) {
        return std::nullopt;
    }
    const QJsonObject object = proposalValue.toObject();

    Proposal proposal;
    proposal.id = object.value(QStringLiteral("id")).toString();
    proposal.finalId = object.value(QStringLiteral("finalId")).toString();
    proposal.name = object.value(QStringLiteral("name")).toString();
    proposal.sha256 = object.value(QStringLiteral("sha256")).toString().toLower();
    const QJsonValue proposedBy = object.value(QStringLiteral("proposedBy"));
    proposal.proposedBy = proposedBy.isString() ? proposedBy.toString() : QString();
    proposal.createdAt = parseIsoUtc(object.value(QStringLiteral("createdAt")));

    if (!isUuid(proposal.id) || !isUuid(proposal.finalId) || !isSha256Hex(proposal.sha256)) {
        return std::nullopt;
    }
    return proposal;
}

QByteArray buildStatusBody(const QString& activeFinalId,
                           const QString& activeSha256,
                           const QString& appVersion)
{
    QJsonObject object;
    object.insert(QStringLiteral("activeFinalId"),
                  activeFinalId.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(activeFinalId));
    object.insert(QStringLiteral("activeSha256"),
                  activeSha256.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(activeSha256));
    object.insert(QStringLiteral("appVersion"),
                  appVersion.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(appVersion.left(1000)));
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

QByteArray buildProposalAnswerBody(bool accepted)
{
    QJsonObject object;
    object.insert(QStringLiteral("result"), accepted ? QStringLiteral("accepted") : QStringLiteral("rejected"));
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

QString sha256Hex(const QByteArray& bytes)
{
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

bool sha256Matches(const QString& expected, const QByteArray& bytes)
{
    return isSha256Hex(expected) && expected.compare(sha256Hex(bytes), Qt::CaseInsensitive) == 0;
}

std::optional<qint64> parseRetryAfterMs(const QByteArray& value, const QDateTime& now)
{
    const QByteArray trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return std::nullopt;
    }

    bool ok = false;
    const qint64 seconds = trimmed.toLongLong(&ok);
    if (ok) {
        return seconds < 0 ? std::nullopt : std::optional<qint64>(seconds * 1000);
    }

    // HTTP-date, e.g. "Wed, 21 Oct 2015 07:28:00 GMT".
    QString text = QString::fromLatin1(trimmed);
    if (text.endsWith(QStringLiteral(" GMT"))) {
        text.chop(4);
        text += QStringLiteral(" +0000");
    }
    const QDateTime when = QDateTime::fromString(text, Qt::RFC2822Date);
    if (!when.isValid()) {
        return std::nullopt;
    }
    return std::max<qint64>(0, now.msecsTo(when));
}

QString parseErrorCode(const QByteArray& body)
{
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isObject()) {
        return QString();
    }
    return doc.object().value(QStringLiteral("error")).toString();
}

ResponseClass classifyResponse(int httpStatus, const QString& errorCode)
{
    if (httpStatus >= 200 && httpStatus < 300) {
        return ResponseClass::Ok;
    }
    if (httpStatus == 304) {
        return ResponseClass::Ok;
    }
    if (httpStatus == 401) {
        return ResponseClass::NotAuthorized;
    }
    if (httpStatus == 404) {
        return errorCode == QLatin1String("server_storage_disabled") ? ResponseClass::NotAuthorized
                                                                    : ResponseClass::NotFound;
    }
    if (httpStatus == 409) {
        return ResponseClass::Conflict;
    }
    if (httpStatus == 429) {
        return ResponseClass::RateLimited;
    }
    if (httpStatus >= 400 && httpStatus < 500) {
        return ResponseClass::ClientError;
    }
    return ResponseClass::TransientError;
}

} // namespace planner
} // namespace multipack
