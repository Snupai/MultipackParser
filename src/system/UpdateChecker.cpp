/**
 * @file UpdateChecker.cpp
 * @brief Implementation of update checker via GitHub releases API
 */

#include "multipack/system/UpdateChecker.h"
#include "multipack/config/ConfigDefaults.h"

#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

namespace multipack {
namespace system {

UpdateChecker::UpdateChecker(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &UpdateChecker::onNetworkReply);

    qDebug() << "UpdateChecker - initialized";
}

UpdateChecker::~UpdateChecker()
{
    qDebug() << "UpdateChecker - destroyed";
}

void UpdateChecker::checkForUpdates()
{
    if (m_checking) {
        qDebug() << "UpdateChecker - already checking";
        return;
    }

    qDebug() << "UpdateChecker - checking for updates";
    m_checking = true;
    emit checkingStarted();

    QNetworkRequest request;
    request.setUrl(QUrl(GITHUB_RELEASES_URL));
    request.setHeader(QNetworkRequest::UserAgentHeader, "MultipackParser-Updater");
    request.setRawHeader("Accept", "application/vnd.github.v3+json");

    m_networkManager->get(request);
}

QString UpdateChecker::currentVersion() const
{
    return config::Defaults::VERSION;
}

bool UpdateChecker::updateAvailable() const
{
    return m_updateAvailable;
}

UpdateInfo UpdateChecker::updateInfo() const
{
    return m_updateInfo;
}

int UpdateChecker::compareVersions(const QString& v1, const QString& v2)
{
    // Remove 'v' prefix if present
    QString ver1 = v1.startsWith('v') ? v1.mid(1) : v1;
    QString ver2 = v2.startsWith('v') ? v2.mid(1) : v2;

    QStringList parts1 = ver1.split('.');
    QStringList parts2 = ver2.split('.');

    int maxLen = qMax(parts1.size(), parts2.size());

    for (int i = 0; i < maxLen; ++i) {
        int num1 = (i < parts1.size()) ? parts1[i].toInt() : 0;
        int num2 = (i < parts2.size()) ? parts2[i].toInt() : 0;

        if (num1 < num2) return -1;
        if (num1 > num2) return 1;
    }

    return 0;
}

void UpdateChecker::onNetworkReply(QNetworkReply* reply)
{
    m_checking = false;

    if (reply->error() != QNetworkReply::NoError) {
        QString errorMsg;

        switch (reply->error()) {
            case QNetworkReply::ConnectionRefusedError:
            case QNetworkReply::HostNotFoundError:
            case QNetworkReply::TimeoutError:
                errorMsg = tr("Network connection error");
                break;
            case QNetworkReply::ContentNotFoundError:
                errorMsg = tr("GitHub repository not found");
                break;
            case QNetworkReply::ContentAccessDenied:
                errorMsg = tr("GitHub API rate limit exceeded");
                break;
            default:
                errorMsg = tr("Network error: %1").arg(reply->errorString());
                break;
        }

        qWarning() << "UpdateChecker - network error:" << errorMsg;
        emit checkFailed(errorMsg);
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    parseGitHubResponse(data);
}

void UpdateChecker::parseGitHubResponse(const QByteArray& data)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "UpdateChecker - JSON parse error:" << parseError.errorString();
        emit checkFailed(tr("Invalid response from server"));
        return;
    }

    if (!doc.isObject()) {
        qWarning() << "UpdateChecker - expected JSON object";
        emit checkFailed(tr("Invalid response format"));
        return;
    }

    QJsonObject root = doc.object();

    // Get version from tag_name
    QString tagName = root["tag_name"].toString();
    QString version = tagName.startsWith('v') ? tagName.mid(1) : tagName;

    if (version.isEmpty()) {
        qWarning() << "UpdateChecker - no version found";
        emit checkFailed(tr("No version information in release"));
        return;
    }

    // Find the correct asset
    QString downloadUrl;
    QJsonArray assets = root["assets"].toArray();
    for (const QJsonValue& assetVal : assets) {
        QJsonObject asset = assetVal.toObject();
        if (asset["name"].toString() == ASSET_NAME) {
            downloadUrl = asset["browser_download_url"].toString();
            break;
        }
    }

    if (downloadUrl.isEmpty()) {
        qDebug() << "UpdateChecker - no suitable asset found";
        // Still report the version, just no download URL
    }

    // Store update info
    m_updateInfo.version = version;
    m_updateInfo.releaseNotes = root["body"].toString();
    m_updateInfo.downloadUrl = downloadUrl;

    // Compare versions
    QString current = currentVersion();
    int cmp = compareVersions(version, current);

    m_updateAvailable = (cmp > 0);

    qDebug() << "UpdateChecker - current:" << current
             << "latest:" << version
             << "update available:" << m_updateAvailable;

    emit checkCompleted(m_updateAvailable);
}

} // namespace system
} // namespace multipack
