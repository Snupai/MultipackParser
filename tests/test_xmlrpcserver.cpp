#include "TestHelpers.h"

#include "multipack/network/XmlRpcServer.h"

#include <QCoreApplication>
#include <QTcpSocket>
#include <QtTest/QtTest>

using multipack::network::RpcValue;
using multipack::network::XmlRpcServer;

class XmlRpcServerTest : public QObject
{
    Q_OBJECT

private slots:
    void startAndStopServer();
    void fragmentedHttpRequestIsBuffered();
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

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    XmlRpcServerTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_xmlrpcserver.moc"
