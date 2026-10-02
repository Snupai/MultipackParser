#ifndef MULTIPACK_SYSTEM_WIFISSIDPROBE_H
#define MULTIPACK_SYSTEM_WIFISSIDPROBE_H
#include <QObject>
#include <QProcess>
#include <QTimer>
namespace multipack::system {
class WifiSsidProbe : public QObject {
    Q_OBJECT
public:
    explicit WifiSsidProbe(QObject* parent = nullptr, const QString& program = "nmcli", int timeoutMs = 1500);
    ~WifiSsidProbe() override;
    void refresh();
    static QString parse(const QByteArray& output);
signals:
    void result(const QString& ssid);
private:
    QProcess* m_process;
    QTimer m_timer;
    QString m_program;
    int m_timeoutMs;
    bool m_expired = false;
};
}
#endif
