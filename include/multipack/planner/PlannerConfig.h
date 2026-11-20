/**
 * @file PlannerConfig.h
 * @brief Palettenplaner connection settings (HMI-Abgleich)
 *
 * The planner admin downloads a configuration file `hmi-<name>.json` with
 * `{serverUrl, hmiId, token}` (docs/hmi-api-v1.md in palette_3d_viewer). It is
 * imported from a USB stick into settings.json as `planner.serverUrl`,
 * `planner.hmiId` and `planner.token`. An optional factory CA certificate
 * (.pem) next to it is copied into the working directory and referenced by
 * `planner.caCertPath`.
 *
 * The token is a secret: it is never logged and never sent over plain HTTP
 * (HTTP is only accepted for loopback development servers).
 */
#ifndef MULTIPACK_PLANNER_PLANNERCONFIG_H
#define MULTIPACK_PLANNER_PLANNERCONFIG_H

#include <QByteArray>
#include <QList>
#include <QSslCertificate>
#include <QString>
#include <QStringList>
#include <QUrl>

namespace multipack {
namespace config { class SettingsManager; }

namespace planner {

namespace Keys {
constexpr const char* SERVER_URL = "planner.serverUrl";
constexpr const char* HMI_ID = "planner.hmiId";
constexpr const char* TOKEN = "planner.token";
constexpr const char* CA_CERT_PATH = "planner.caCertPath";
} // namespace Keys

/// Directory (relative to the working directory) for planner files.
constexpr const char* PLANNER_DIR = "planner";
/// File name of the imported factory CA inside PLANNER_DIR.
constexpr const char* CA_CERT_FILE = "planner-ca.pem";

/**
 * @struct PlannerConfig
 * @brief Validated planner connection settings
 */
struct PlannerConfig {
    QString serverUrl;   ///< Without trailing slash
    QString hmiId;       ///< UUID as issued by the planner
    QString token;       ///< Device token (secret)
    QString caCertPath;  ///< Optional, relative to the working directory or absolute

    /// All three required fields are present.
    [[nodiscard]] bool isComplete() const;

    /// QR code target: `<serverUrl>/hmi/<hmiId>`. Contains no secret.
    [[nodiscard]] QString hmiPageUrl() const;

    /// Absolute URL for an API path such as "/api/hmi/v1/finals".
    [[nodiscard]] QUrl apiUrl(const QString& path) const;
};

/**
 * @brief Result of parsing/validating a configuration
 */
struct ConfigParseResult {
    bool ok = false;
    PlannerConfig config;
    QString error;  ///< German, operator-facing; never contains the token
};

/**
 * @brief Whether a URL may carry the device token
 *
 * HTTPS always; plain HTTP only to localhost / 127.0.0.1 / ::1 (development).
 */
[[nodiscard]] bool isSecureTransport(const QUrl& url);

/**
 * @brief Validate and normalise a server URL
 * @param input URL as written in the configuration file
 * @param normalized Receives the URL without trailing slash
 * @param error Receives an operator-facing message on failure
 */
[[nodiscard]] bool normalizeServerUrl(const QString& input, QString* normalized, QString* error);

/**
 * @brief Parse the JSON configuration file the planner admin downloads
 *
 * Expects `{ "serverUrl", "hmiId", "token" }`; unknown fields are ignored.
 */
[[nodiscard]] ConfigParseResult parseConfigJson(const QByteArray& json);

/**
 * @brief Validate a configuration (e.g. as loaded from settings.json)
 */
[[nodiscard]] ConfigParseResult validateConfig(const PlannerConfig& config);

/**
 * @brief Read the planner section from settings (no validation)
 */
[[nodiscard]] PlannerConfig loadFromSettings(const config::SettingsManager& settings);

/**
 * @brief Write the planner section into settings (caller saves the file)
 */
void storeInSettings(config::SettingsManager& settings, const PlannerConfig& config);

/**
 * @brief Remove the planner section from settings (caller saves the file)
 */
void clearFromSettings(config::SettingsManager& settings);

/**
 * @brief Resolve caCertPath against a base directory
 */
[[nodiscard]] QString resolveCaCertPath(const PlannerConfig& config, const QString& baseDir);

/**
 * @brief Load PEM certificates from a file; empty list on failure
 */
[[nodiscard]] QList<QSslCertificate> loadCaCertificates(const QString& path);

/**
 * @brief Candidate configuration files on a USB stick (`hmi-*.json`)
 */
[[nodiscard]] QStringList findConfigFiles(const QString& usbDir);

/**
 * @brief Result of an import from a USB stick
 */
struct ImportResult {
    bool ok = false;
    PlannerConfig config;   ///< Imported configuration (caCertPath relative to working dir)
    bool caImported = false;
    QString error;          ///< German, operator-facing
};

/**
 * @brief Import a configuration file and an optional CA certificate
 *
 * The CA is looked up next to the configuration file: first
 * `<same base name>.pem`, otherwise the only `*.pem` in that directory.
 * It is validated and copied to `<workingDir>/planner/planner-ca.pem`.
 * Nothing is written unless the configuration file is valid.
 *
 * @param configFilePath Path to the `hmi-*.json` file
 * @param workingDir Application working directory (writable)
 */
[[nodiscard]] ImportResult importFromFile(const QString& configFilePath, const QString& workingDir);

} // namespace planner
} // namespace multipack

#endif // MULTIPACK_PLANNER_PLANNERCONFIG_H
