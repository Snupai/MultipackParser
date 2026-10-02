#include "multipack/system/WifiSsidProbe.h"
namespace multipack::system {
WifiSsidProbe::WifiSsidProbe(QObject* parent, const QString& program, int timeoutMs)
    : QObject(parent), m_process(new QProcess(this)), m_program(program), m_timeoutMs(timeoutMs)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        m_expired = true; m_process->kill(); emit result(tr("SSID unavailable (timeout)"));
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            m_timer.stop(); emit result(tr("SSID unavailable (nmcli not installed)"));
        }
    });
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
        [this](int code, QProcess::ExitStatus status) {
            m_timer.stop();
            if (m_expired) return;
            emit result(code == 0 && status == QProcess::NormalExit
                ? parse(m_process->readAllStandardOutput()) : tr("SSID unavailable"));
        });
}
WifiSsidProbe::~WifiSsidProbe() { m_process->disconnect(this); m_process->kill(); }
void WifiSsidProbe::refresh()
{
    if (m_process->state() != QProcess::NotRunning) return;
    m_expired = false;
    m_process->start(m_program, {"--terse", "--escape", "no", "--fields", "ACTIVE,SSID",
        "device", "wifi", "list", "ifname", "wlan0", "--rescan", "no"});
    m_timer.start(m_timeoutMs);
}
QString WifiSsidProbe::parse(const QByteArray& output)
{
    for (const auto& line : QString::fromUtf8(output).split('\n'))
        if (line.startsWith("yes:")) return line.mid(4); // Preserve SSID spaces/colons/backslashes.
    return tr("Not connected");
}
}
