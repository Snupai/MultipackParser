/**
 * @file FakePlannerServer.h
 * @brief Minimal in-process HTTP/1.1 (+TLS) server that plays the planner
 *
 * Every connection serves one request and closes. Requests are recorded so
 * tests can check headers (Authorization, If-None-Match) and bodies.
 */
#ifndef MULTIPACK_TESTS_FAKEPLANNERSERVER_H
#define MULTIPACK_TESTS_FAKEPLANNERSERVER_H

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QPair>
#include <QSslCertificate>
#include <QSslKey>
#include <QSslSocket>
#include <QTcpServer>
#include <QTcpSocket>
#include <QVector>

#include <functional>
#include <memory>

namespace testhelpers {

class FakePlannerServer : public QTcpServer
{
public:
    struct Request {
        QByteArray method;
        QByteArray path;
        QHash<QByteArray, QByteArray> headers;  ///< Lower-case names
        QByteArray body;
    };

    struct Reply {
        int status = 200;
        QList<QPair<QByteArray, QByteArray>> headers;
        QByteArray body;

        Reply& header(const QByteArray& name, const QByteArray& value)
        {
            headers.append(qMakePair(name, value));
            return *this;
        }

        static Reply json(int status, const QByteArray& body)
        {
            Reply reply;
            reply.status = status;
            reply.body = body;
            reply.header("Content-Type", "application/json");
            return reply;
        }
        static Reply empty(int status)
        {
            Reply reply;
            reply.status = status;
            return reply;
        }
    };

    using Handler = std::function<Reply(const Request&)>;

    explicit FakePlannerServer(QObject* parent = nullptr)
        : QTcpServer(parent)
    {
    }

    void setHandler(Handler handler) { m_handler = std::move(handler); }

    void enableTls(const QSslCertificate& certificate, const QSslKey& key)
    {
        m_tls = true;
        m_certificate = certificate;
        m_key = key;
    }

    QVector<Request> requests;
    int connections = 0;

    QVector<Request> requestsFor(const QByteArray& method, const QByteArray& pathPrefix) const
    {
        QVector<Request> result;
        for (const Request& request : requests) {
            if (request.method == method && request.path.startsWith(pathPrefix)) {
                result.append(request);
            }
        }
        return result;
    }

protected:
    void incomingConnection(qintptr descriptor) override
    {
        ++connections;
        QTcpSocket* socket = nullptr;
        if (m_tls) {
            auto* ssl = new QSslSocket(this);
            ssl->setLocalCertificate(m_certificate);
            ssl->setPrivateKey(m_key);
            ssl->setPeerVerifyMode(QSslSocket::VerifyNone);  // server does not ask for client certs
            socket = ssl;
        } else {
            socket = new QTcpSocket(this);
        }
        if (!socket->setSocketDescriptor(descriptor)) {
            socket->deleteLater();
            return;
        }
        if (m_tls) {
            static_cast<QSslSocket*>(socket)->startServerEncryption();
        }

        auto buffer = std::make_shared<QByteArray>();
        connect(socket, &QTcpSocket::readyRead, socket, [this, socket, buffer]() {
            buffer->append(socket->readAll());
            const int headerEnd = buffer->indexOf("\r\n\r\n");
            if (headerEnd < 0) {
                return;
            }
            Request request;
            const QList<QByteArray> lines = buffer->left(headerEnd).split('\n');
            const QList<QByteArray> requestLine = lines.value(0).trimmed().split(' ');
            request.method = requestLine.value(0);
            request.path = requestLine.value(1);
            for (int i = 1; i < lines.size(); ++i) {
                const QByteArray line = lines[i].trimmed();
                const int colon = line.indexOf(':');
                if (colon > 0) {
                    request.headers.insert(line.left(colon).trimmed().toLower(), line.mid(colon + 1).trimmed());
                }
            }
            const int length = request.headers.value("content-length", "0").toInt();
            if (buffer->size() < headerEnd + 4 + length) {
                return;
            }
            request.body = buffer->mid(headerEnd + 4, length);
            buffer->clear();
            requests.append(request);

            const Reply reply = m_handler ? m_handler(request) : Reply::empty(404);
            QByteArray response = "HTTP/1.1 " + QByteArray::number(reply.status) + " " + reason(reply.status) + "\r\n";
            for (const auto& header : reply.headers) {
                response += header.first + ": " + header.second + "\r\n";
            }
            response += "Content-Length: " + QByteArray::number(reply.body.size()) + "\r\n";
            response += "Connection: close\r\n\r\n";
            response += reply.body;
            socket->write(response);
            socket->disconnectFromHost();
        });
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    }

private:
    static QByteArray reason(int status)
    {
        switch (status) {
            case 200: return "OK";
            case 204: return "No Content";
            case 304: return "Not Modified";
            case 400: return "Bad Request";
            case 401: return "Unauthorized";
            case 404: return "Not Found";
            case 409: return "Conflict";
            case 429: return "Too Many Requests";
            case 500: return "Internal Server Error";
            default: return "Status";
        }
    }

    Handler m_handler;
    bool m_tls = false;
    QSslCertificate m_certificate;
    QSslKey m_key;
};

} // namespace testhelpers

#endif // MULTIPACK_TESTS_FAKEPLANNERSERVER_H
