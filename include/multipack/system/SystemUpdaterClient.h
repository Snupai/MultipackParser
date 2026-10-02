#ifndef MULTIPACK_SYSTEM_SYSTEMUPDATERCLIENT_H
#define MULTIPACK_SYSTEM_SYSTEMUPDATERCLIENT_H
#include <QObject>
#include <QLocalSocket>
#include <QTimer>
#include <QJsonObject>
namespace multipack::system {
// One bounded asynchronous request at a time. Production always uses the fixed socket.
class SystemUpdaterClient : public QObject {
    Q_OBJECT
public:
    explicit SystemUpdaterClient(QObject* parent = nullptr,
        const QString& socketPath = QStringLiteral("/run/hmi-updater/control.sock"),
        int timeoutMs = 4000);
    bool status();
    bool install(const QString& version);
    void cancel(); // Cancels observation only; never cancels a privileged installation.
    static bool validVersion(const QString& version);
signals:
    void response(const QJsonObject& value);
    void error(const QString& message);
private:
    bool request(const QJsonObject& value);
    void fail(const QString& message);
    QLocalSocket* m_socket;
    QTimer m_timeout;
    QString m_path;
    QByteArray m_request, m_buffer;
    int m_timeoutMs;
    bool m_busy = false;
};
}
#endif
