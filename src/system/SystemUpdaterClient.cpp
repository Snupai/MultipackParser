#include "multipack/system/SystemUpdaterClient.h"
#include <QJsonDocument>
#include <QRegularExpression>
namespace multipack::system {
SystemUpdaterClient::SystemUpdaterClient(QObject* parent, const QString& path, int timeoutMs)
    : QObject(parent), m_socket(new QLocalSocket(this)), m_path(path), m_timeoutMs(timeoutMs)
{
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this] { fail(tr("System updater timed out")); });
    connect(m_socket, &QLocalSocket::connected, this, [this] { m_socket->write(m_request); });
    connect(m_socket, &QLocalSocket::errorOccurred, this, [this] {
        if (m_busy) fail(tr("System updater: %1").arg(m_socket->errorString()));
    });
    connect(m_socket, &QLocalSocket::disconnected, this, [this] {
        if (m_busy) fail(tr("System updater disconnected before replying"));
    });
    connect(m_socket, &QLocalSocket::readyRead, this, [this] {
        if (!m_busy) return;
        m_buffer += m_socket->readAll();
        if (m_buffer.size() > 65536) { fail(tr("System updater reply too large")); return; }
        const int end = m_buffer.indexOf('\n');
        if (end < 0) return;
        QJsonParseError parseError;
        const auto doc = QJsonDocument::fromJson(m_buffer.left(end), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            fail(tr("Invalid system updater reply")); return;
        }
        const auto value = doc.object();
        cancel();
        if (value.contains("error") && !value.value("error").isNull())
            emit error(tr("System updater: %1").arg(value.value("error").toString()));
        else if (value.value("accepted") != true && !value.value("phase").isString())
            emit error(tr("Invalid system updater status"));
        else emit response(value);
    });
}
bool SystemUpdaterClient::validVersion(const QString& version)
{
    static const QRegularExpression pattern(QStringLiteral("\\A[a-zA-Z0-9][a-zA-Z0-9._-]{0,79}\\z"));
    return pattern.match(version).hasMatch();
}
bool SystemUpdaterClient::status() { return request({{"op", "status"}}); }
bool SystemUpdaterClient::install(const QString& version)
{
    if (!validVersion(version)) { emit error(tr("Invalid system release VERSION")); return false; }
    return request({{"op", "install"}, {"version", version}});
}
bool SystemUpdaterClient::request(const QJsonObject& value)
{
    if (m_busy) return false;
    m_buffer.clear();
    m_request = QJsonDocument(value).toJson(QJsonDocument::Compact) + '\n';
    m_busy = true;
    m_timeout.start(m_timeoutMs);
    m_socket->connectToServer(m_path);
    return true;
}
void SystemUpdaterClient::cancel() { m_busy = false; m_timeout.stop(); m_socket->abort(); }
void SystemUpdaterClient::fail(const QString& message) { cancel(); emit error(message); }
}
