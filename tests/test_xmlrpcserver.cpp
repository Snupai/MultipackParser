#include "TestHelpers.h"

#include "multipack/network/XmlRpcServer.h"
#include "multipack/core/GlobalState.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTcpSocket>
#include <QtTest/QtTest>

using multipack::core::GlobalState;
using multipack::network::RpcValue;
using multipack::network::XmlRpcServer;

namespace {

QByteArray sendXmlRpcRequest(XmlRpcServer& server, const QString& methodName)
{
    const QByteArray xmlBody =
        "<?xml version=\"1.0\"?>"
        "<methodCall>"
        "<methodName>" + methodName.toUtf8() + "</methodName>"
        "</methodCall>";

    const QByteArray request =
        "POST /RPC2 HTTP/1.1\r\n"
        "Host: 127.0.0.1\r\n"
        "Content-Type: text/xml\r\n"
        "Content-Length: " + QByteArray::number(xmlBody.size()) + "\r\n\r\n" +
        xmlBody;

    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, static_cast<quint16>(server.port()));
    if (!socket.waitForConnected(5000)) {
        return QByteArray();
    }

    socket.write(request);
    socket.flush();

    QElapsedTimer timer;
    timer.start();
    while (socket.bytesAvailable() == 0 && timer.elapsed() < 5000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        socket.waitForReadyRead(20);
    }
    if (socket.bytesAvailable() == 0) {
        return QByteArray();
    }

    QByteArray response = socket.readAll();
    if (socket.waitForReadyRead(100)) {
        response.append(socket.readAll());
    }
    return response;
}

} // namespace

class XmlRpcServerTest : public QObject
{
    Q_OBJECT

private slots:
    void startAndStopServer();
    void fragmentedHttpRequestIsBuffered();
    void operationalRpcBeforeReadyReturnsFault();
    void operationalRpcAfterReadyReturnsSuccess();
    void earlyRobotConnectDoesNotCrash();
};

void XmlRpcServerTest::startAndStopServer()
{
    XmlRpcServer server;
    QVERIFY(server.start(0));
    QVERIFY(server.isRunning());
    QVERIFY(server.port() > 0);

    server.stop();
    QVERIFY(!server.isRunning());
}

void XmlRpcServerTest::fragmentedHttpRequestIsBuffered()
{
    XmlRpcServer server;
    server.registerMethod("Echo", [](const QVector<RpcValue>& params) {
        return params.isEmpty() ? RpcValue(QString()) : params.first();
    });

    QVERIFY(server.start(0));

    const QByteArray xmlBody =
        "<?xml version=\"1.0\"?>"
        "<methodCall>"
        "<methodName>Echo</methodName>"
        "<params><param><value><string>hello</string></value></param></params>"
        "</methodCall>";

    const QByteArray request =
        "POST /RPC2 HTTP/1.1\r\n"
        "Host: 127.0.0.1\r\n"
        "Content-Type: text/xml\r\n"
        "Content-Length: " + QByteArray::number(xmlBody.size()) + "\r\n\r\n" +
        xmlBody;

    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, static_cast<quint16>(server.port()));
    QVERIFY(socket.waitForConnected(5000));

    const int splitIndex = request.size() / 2;
    QVERIFY(socket.write(request.left(splitIndex)) == splitIndex);
    QVERIFY(socket.flush());
    QTest::qWait(150);
    QCOMPARE(socket.bytesAvailable(), 0);

    QVERIFY(socket.write(request.mid(splitIndex)) == request.size() - splitIndex);
    QVERIFY(socket.flush());
    QTRY_VERIFY_WITH_TIMEOUT(socket.bytesAvailable() > 0, 5000);

    QByteArray response = socket.readAll();
    if (socket.bytesAvailable() == 0) {
        QTest::qWait(100);
    }
    if (socket.bytesAvailable() > 0) {
        response.append(socket.readAll());
    }

    QVERIFY(response.contains("<string>hello</string>"));
}

void XmlRpcServerTest::operationalRpcBeforeReadyReturnsFault()
{
    GlobalState::instance().clear();

    XmlRpcServer server;
    server.setGlobalState(&GlobalState::instance());
    server.registerStandardMethods();
    QVERIFY(server.start(0));

    const QByteArray statusResponse = sendXmlRpcRequest(server, "get_status");
    QVERIFY(statusResponse.contains("<name>ready</name>"));
    QVERIFY(statusResponse.contains("<boolean>0</boolean>"));

    const QByteArray response = sendXmlRpcRequest(server, "UR_Palette");
    QVERIFY(response.contains("<fault>"));
    QVERIFY(response.contains("Application not ready"));
}

void XmlRpcServerTest::operationalRpcAfterReadyReturnsSuccess()
{
    GlobalState::instance().clear();
    GlobalState::instance().applyPaletteData(testhelpers::makeSamplePaletteData());

    XmlRpcServer server;
    server.setGlobalState(&GlobalState::instance());
    server.registerStandardMethods();
    QVERIFY(server.start(0));

    const QByteArray statusResponse = sendXmlRpcRequest(server, "get_status");
    QVERIFY(statusResponse.contains("<name>ready</name>"));
    QVERIFY(statusResponse.contains("<boolean>1</boolean>"));

    const QByteArray response = sendXmlRpcRequest(server, "UR_Palette");
    QVERIFY(!response.contains("<fault>"));
    QVERIFY(response.contains("<int>1200</int>"));
    QVERIFY(response.contains("<int>800</int>"));
}

void XmlRpcServerTest::earlyRobotConnectDoesNotCrash()
{
    GlobalState::instance().clear();

    XmlRpcServer server;
    server.setGlobalState(&GlobalState::instance());
    server.registerStandardMethods();
    QVERIFY(server.start(0));

    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, static_cast<quint16>(server.port()));
    QVERIFY(socket.waitForConnected(5000));
    socket.disconnectFromHost();

    const QByteArray response = sendXmlRpcRequest(server, "UR_AnzPakete");
    QVERIFY(response.contains("<fault>"));
    QVERIFY(server.isRunning());
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    XmlRpcServerTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_xmlrpcserver.moc"
