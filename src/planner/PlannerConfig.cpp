/**
 * @file PlannerConfig.cpp
 * @brief Palettenplaner connection settings (HMI-Abgleich)
 */
#include "multipack/planner/PlannerConfig.h"

#include "multipack/config/SettingsManager.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>

#include <optional>

namespace multipack {
namespace planner {

namespace {

constexpr qint64 MAX_CONFIG_FILE_BYTES = 64 * 1024;
constexpr qint64 MAX_CA_FILE_BYTES = 256 * 1024;

bool isLoopbackHost(const QString& host)
{
    if (host.compare(QStringLiteral("localhost"), Qt::CaseInsensitive) == 0) {
        return true;
    }
    const QHostAddress address(host);
    return !address.isNull() && address.isLoopback();
}

bool isValidToken(const QString& token)
{
    // 32 random bytes, base64url without padding = 43 characters.
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9_-]{43}$"));
    return pattern.match(token).hasMatch();
}

bool isValidHmiId(const QString& hmiId)
{
    const QUuid uuid = QUuid::fromString(hmiId);
    return !uuid.isNull() && !hmiId.startsWith('{');
}

} // namespace

bool PlannerConfig::isComplete() const
{
    return !serverUrl.isEmpty() && !hmiId.isEmpty() && !token.isEmpty();
}

QString PlannerConfig::hmiPageUrl() const
{
    if (serverUrl.isEmpty() || hmiId.isEmpty()) {
        return QString();
    }
    return serverUrl + QStringLiteral("/hmi/") + hmiId;
}

QUrl PlannerConfig::apiUrl(const QString& path) const
{
    return QUrl(serverUrl + path);
}

bool isSecureTransport(const QUrl& url)
{
    if (!url.isValid()) {
        return false;
    }
    const QString scheme = url.scheme().toLower();
    if (scheme == QLatin1String("https")) {
        return true;
    }
    return scheme == QLatin1String("http") && isLoopbackHost(url.host());
}

bool normalizeServerUrl(const QString& input, QString* normalized, QString* error)
{
    auto fail = [error](const QString& message) {
        if (error) {
            *error = message;
        }
        return false;
    };

    const QString trimmed = input.trimmed();
    if (trimmed.isEmpty()) {
        return fail(QStringLiteral("Server-Adresse fehlt"));
    }

    const QUrl url(trimmed, QUrl::StrictMode);
    const QString scheme = url.scheme().toLower();
    if (!url.isValid() || url.host().isEmpty()
        || (scheme != QLatin1String("https") && scheme != QLatin1String("http"))
        || !url.userInfo().isEmpty() || url.hasQuery() || url.hasFragment()) {
        return fail(QStringLiteral("Server-Adresse ungültig"));
    }

    if (!isSecureTransport(url)) {
        return fail(QStringLiteral("Server-Adresse muss HTTPS verwenden"));
    }

    QString result = url.toString(QUrl::FullyEncoded);
    while (result.endsWith('/')) {
        result.chop(1);
    }
    if (normalized) {
        *normalized = result;
    }
    return true;
}

ConfigParseResult validateConfig(const PlannerConfig& config)
{
    ConfigParseResult result;
    result.config = config;

    QString normalized;
    if (!normalizeServerUrl(config.serverUrl, &normalized, &result.error)) {
        return result;
    }
    result.config.serverUrl = normalized;

    result.config.hmiId = config.hmiId.trimmed().toLower();
    if (!isValidHmiId(result.config.hmiId)) {
        result.error = QStringLiteral("HMI-ID ungültig");
        return result;
    }

    result.config.token = config.token.trimmed();
    if (!isValidToken(result.config.token)) {
        result.error = QStringLiteral("Token ungültig");
        return result;
    }

    result.config.caCertPath = config.caCertPath.trimmed();
    result.ok = true;
    return result;
}

ConfigParseResult parseConfigJson(const QByteArray& json)
{
    ConfigParseResult result;

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        result.error = QStringLiteral("Konfigurationsdatei ist kein gueltiges JSON");
        return result;
    }

    const QJsonObject object = doc.object();
    const auto field = [&object](const char* key) -> std::optional<QString> {
        const QJsonValue value = object.value(QLatin1String(key));
        if (!value.isString()) {
            return std::nullopt;
        }
        return value.toString();
    };

    const auto serverUrl = field("serverUrl");
    const auto hmiId = field("hmiId");
    const auto token = field("token");
    if (!serverUrl || !hmiId || !token) {
        result.error = QStringLiteral("Konfigurationsdatei unvollständig (serverUrl, hmiId, token)");
        return result;
    }

    PlannerConfig config;
    config.serverUrl = *serverUrl;
    config.hmiId = *hmiId;
    config.token = *token;
    return validateConfig(config);
}

PlannerConfig loadFromSettings(const config::SettingsManager& settings)
{
    PlannerConfig config;
    config.serverUrl = settings.value(Keys::SERVER_URL).toString();
    config.hmiId = settings.value(Keys::HMI_ID).toString();
    config.token = settings.value(Keys::TOKEN).toString();
    config.caCertPath = settings.value(Keys::CA_CERT_PATH).toString();
    return config;
}

void storeInSettings(config::SettingsManager& settings, const PlannerConfig& config)
{
    settings.setValue(Keys::SERVER_URL, config.serverUrl);
    settings.setValue(Keys::HMI_ID, config.hmiId);
    settings.setValue(Keys::TOKEN, config.token);
    settings.setValue(Keys::CA_CERT_PATH, config.caCertPath);
}

void clearFromSettings(config::SettingsManager& settings)
{
    settings.remove(QStringLiteral("planner"));
}

QString resolveCaCertPath(const PlannerConfig& config, const QString& baseDir)
{
    if (config.caCertPath.isEmpty()) {
        return QString();
    }
    const QFileInfo info(config.caCertPath);
    return info.isAbsolute() ? info.absoluteFilePath() : QDir(baseDir).absoluteFilePath(config.caCertPath);
}

QList<QSslCertificate> loadCaCertificates(const QString& path)
{
    QFile file(path);
    if (path.isEmpty() || !file.open(QIODevice::ReadOnly) || file.size() > MAX_CA_FILE_BYTES) {
        return {};
    }
    return QSslCertificate::fromData(file.readAll(), QSsl::Pem);
}

QStringList findConfigFiles(const QString& usbDir)
{
    QStringList result;
    const QDir dir(usbDir);
    if (usbDir.isEmpty() || !dir.exists()) {
        return result;
    }
    const QFileInfoList files = dir.entryInfoList({QStringLiteral("hmi-*.json")}, QDir::Files, QDir::Name);
    for (const QFileInfo& info : files) {
        result.append(info.absoluteFilePath());
    }
    return result;
}

ImportResult importFromFile(const QString& configFilePath, const QString& workingDir)
{
    ImportResult result;

    QFile file(configFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        result.error = QStringLiteral("Konfigurationsdatei nicht lesbar");
        return result;
    }
    if (file.size() > MAX_CONFIG_FILE_BYTES) {
        result.error = QStringLiteral("Konfigurationsdatei zu groß");
        return result;
    }

    const ConfigParseResult parsed = parseConfigJson(file.readAll());
    file.close();
    if (!parsed.ok) {
        result.error = parsed.error;
        return result;
    }
    result.config = parsed.config;

    // Optional factory CA next to the configuration file.
    const QFileInfo configInfo(configFilePath);
    const QDir configDir = configInfo.absoluteDir();
    QString caSource = configDir.filePath(configInfo.completeBaseName() + QStringLiteral(".pem"));
    if (!QFileInfo::exists(caSource)) {
        const QStringList pems = configDir.entryList({QStringLiteral("*.pem")}, QDir::Files);
        caSource = pems.size() == 1 ? configDir.filePath(pems.first()) : QString();
    }

    if (!caSource.isEmpty()) {
        QFile caFile(caSource);
        if (!caFile.open(QIODevice::ReadOnly) || caFile.size() > MAX_CA_FILE_BYTES) {
            result.error = QStringLiteral("CA-Zertifikat nicht lesbar");
            return result;
        }
        const QByteArray pem = caFile.readAll();
        const QList<QSslCertificate> certs = QSslCertificate::fromData(pem, QSsl::Pem);
        if (certs.isEmpty()) {
            result.error = QStringLiteral("CA-Zertifikat ungültig (PEM erwartet)");
            return result;
        }

        const QString targetDir = QDir(workingDir).filePath(QString::fromLatin1(PLANNER_DIR));
        if (!QDir().mkpath(targetDir)) {
            result.error = QStringLiteral("Ordner für CA-Zertifikat nicht anlegbar");
            return result;
        }
        QSaveFile out(QDir(targetDir).filePath(QString::fromLatin1(CA_CERT_FILE)));
        if (!out.open(QIODevice::WriteOnly) || out.write(pem) != pem.size() || !out.commit()) {
            result.error = QStringLiteral("CA-Zertifikat konnte nicht gespeichert werden");
            return result;
        }

        result.config.caCertPath = QString::fromLatin1(PLANNER_DIR) + '/' + QString::fromLatin1(CA_CERT_FILE);
        result.caImported = true;
    }

    qInfo() << "Planner configuration parsed from" << configInfo.fileName()
            << "server" << result.config.serverUrl << "hmi" << result.config.hmiId
            << "ca" << (result.caImported ? "imported" : "system trust store");
    result.ok = true;
    return result;
}

} // namespace planner
} // namespace multipack
