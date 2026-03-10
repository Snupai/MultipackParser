/**
 * @file XmlRpcServer.cpp
 * @brief Full implementation of XML-RPC server
 */
#include "multipack/network/XmlRpcServer.h"
#include "multipack/database/DatabaseManager.h"
#include "multipack/core/GlobalState.h"
#include "multipack/network/URCommonFunctions.h"

#include <QTcpSocket>
#include <QRegularExpression>
#include <QDebug>
#include <QDateTime>

namespace multipack {
namespace network {

// Pre-compiled regex patterns for XML-RPC parsing (file scope for efficiency)
namespace {
    const QRegularExpression RE_METHOD_NAME("<methodName>([^<]+)</methodName>");
    const QRegularExpression RE_PARAM("<param>\\s*<value>([\\s\\S]*?)</value>\\s*</param>");
    const QRegularExpression RE_INT("<(?:int|i4)>(-?\\d+)</(?:int|i4)>");
    const QRegularExpression RE_DOUBLE("<double>([^<]+)</double>");
    const QRegularExpression RE_BOOL("<boolean>([01])</boolean>");
    const QRegularExpression RE_STRING("<string>([^<]*)</string>");
    const QRegularExpression RE_ARRAY("<array>\\s*<data>([\\s\\S]*)</data>\\s*</array>");
    const QRegularExpression RE_VALUE("<value>([\\s\\S]*?)</value>");
} // anonymous namespace

XmlRpcServer::XmlRpcServer(QObject* parent)
    : QObject(parent)
    , m_server(std::make_unique<QTcpServer>(this))
{
    qDebug() << "XmlRpcServer - initialized";

    connect(m_server.get(), &QTcpServer::newConnection,
            this, &XmlRpcServer::onNewConnection);
}

XmlRpcServer::~XmlRpcServer()
{
    stop();
}

bool XmlRpcServer::start(int port)
{
    if (m_running) {
        qWarning() << "XmlRpcServer already running";
        return true;
    }

    m_port = port;

    if (!m_server->listen(QHostAddress::Any, port)) {
        qCritical() << "XmlRpcServer failed to start:" << m_server->errorString();
        emit error(m_server->errorString());
        return false;
    }

    m_running = true;
    qDebug() << "XmlRpcServer started on port" << port;
    emit started();
    return true;
}

void XmlRpcServer::stop()
{
    if (!m_running) {
        return;
    }

    m_server->close();
    m_running = false;
    qDebug() << "XmlRpcServer stopped";
    emit stopped();
}

bool XmlRpcServer::isRunning() const
{
    return m_running;
}

void XmlRpcServer::registerMethod(const QString& name, RpcMethod method)
{
    m_methods[name] = method;
    qDebug() << "Registered RPC method:" << name;
}

void XmlRpcServer::unregisterMethod(const QString& name)
{
    m_methods.remove(name);
}

void XmlRpcServer::setDatabaseManager(database::DatabaseManager* db)
{
    m_database = db;
}

void XmlRpcServer::setGlobalState(core::GlobalState* state)
{
    m_state = state;
}

void XmlRpcServer::registerStandardMethods()
{
    qDebug() << "Registering standard RPC methods";

    auto markPaletteNotEmpty = [this](int paletteNumber) {
        if (!m_state) {
            return;
        }

        qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
        if (paletteNumber == 1) {
            m_state->setUr20Palette1Empty(false);
            if (m_state->palette1NonEmptyTimestamp() == 0) {
                m_state->setPalette1NonEmptyTimestamp(currentTime);
            }
        } else if (paletteNumber == 2) {
            m_state->setUr20Palette2Empty(false);
            if (m_state->palette2NonEmptyTimestamp() == 0) {
                m_state->setPalette2NonEmptyTimestamp(currentTime);
            }
        }
    };

    // Register all standard methods using lambdas that call member functions
    // Use Python-style names (UR_*) for compatibility with robot URscript

    // Set filename - robot calls this to specify which palette to load
    registerMethod("UR_SetFileName", [this](const QVector<RpcValue>& params) {
        if (params.isEmpty()) {
            return RpcValue(QString());
        }
        QString articleNumber = params[0].toString();
        QString filename = articleNumber + ".rob";
        qDebug() << "RPC: UR_SetFileName -" << filename;
        if (m_state) {
            m_state->setCurrentFileName(filename);
        }
        return RpcValue(filename);
    });

    // Read data from USB stick / database
    registerMethod("UR_ReadDataFromUsbStick", [this](const QVector<RpcValue>& params) {
        Q_UNUSED(params);
        qDebug() << "RPC: UR_ReadDataFromUsbStick called";

        if (!m_state || !m_database) {
            return RpcValue(1);
        }

        const QString fileName = m_state->currentFileName();
        if (fileName.isEmpty()) {
            qWarning() << "UR_ReadDataFromUsbStick: no file selected";
            return RpcValue(1);
        }

        m_state->clear();

        auto data = m_database->loadPaletteData(fileName);
        if (!data.has_value()) {
            qWarning() << "UR_ReadDataFromUsbStick: palette not found:" << fileName;
            return RpcValue(1);
        }

        m_state->applyPaletteData(*data);
        return RpcValue(0);
    });

    // Palette dimensions [length, width, height]
    registerMethod("UR_Palette", [this](const QVector<RpcValue>& p) {
        return rpcGetPalettenDaten(p);
    });

    // Package dimensions [length, width, height, gap]
    registerMethod("UR_Karton", [this](const QVector<RpcValue>& p) {
        return rpcGetPaketDaten(p);
    });

    // Layer assignments (which layer type for each layer)
    registerMethod("UR_Lagen", [this](const QVector<RpcValue>& p) {
        return rpcGetLageZuordnung(p);
    });

    // Intermediate layers
    registerMethod("UR_Zwischenlagen", [this](const QVector<RpcValue>& p) {
        return rpcGetZwischenlagen(p);
    });

    // Package position for a given index
    registerMethod("UR_PaketPos", [this](const QVector<RpcValue>& p) {
        return rpcGetPaketPos(p);
    });

    // Number of layers
    registerMethod("UR_AnzLagen", [this](const QVector<RpcValue>& p) {
        return rpcGetAnzLagen(p);
    });

    // Number of packages
    registerMethod("UR_AnzPakete", [this](const QVector<RpcValue>& p) {
        return rpcGetAnzPakete(p);
    });

    // Packages per layer type
    registerMethod("UR_PaketeZuordnung", [this](const QVector<RpcValue>& p) {
        return rpcGetPaketeZuordnung(p);
    });

    // Package height
    registerMethod("UR_Paket_hoehe", [this](const QVector<RpcValue>& p) {
        return rpcGetKartonhoehe(p);
    });

    // Start layer
    registerMethod("UR_Startlage", [this](const QVector<RpcValue>& p) {
        return rpcGetStartlage(p);
    });

    // Single package lengthwise
    registerMethod("UR_Quergreifen", [this](const QVector<RpcValue>& p) {
        return rpcGetEinzelpaketLaengs(p);
    });

    // Center of gravity calculation
    registerMethod("UR_CoG", [this](const QVector<RpcValue>& params) {
        qDebug() << "RPC: UR_CoG called";
        if (params.size() < 2 || !m_state) {
            return RpcValue::fromDoubleArray({0.0, 0.0, 0.0});
        }

        double massePaket = params[0].toDouble();
        double masseGreifer = params[1].toDouble();
        int anzahlPakete = params.size() > 2 ? params[2].toInt() : 1;

        if (anzahlPakete == 0) {
            massePaket = 0;
        }

        int kartonZ = m_state->packageHeight();

        // Calculate Y (horizontal offset)
        double y = (1.0 / (masseGreifer + massePaket)) *
                   ((-0.045 * masseGreifer) + (-0.045 * massePaket));

        // Calculate Z (vertical offset)
        double z = (1.0 / (masseGreifer + (massePaket * anzahlPakete))) *
                   ((0.047 * masseGreifer) +
                    ((0.047 + (kartonZ / 2000.0)) * massePaket * anzahlPakete));

        return RpcValue::fromDoubleArray({y, z, 0.0});
    });

    // Estimated mass
    registerMethod("UR_MasseGeschaetzt", [this](const QVector<RpcValue>& p) {
        return rpcGetGewicht(p);
    });

    // Pick offset X
    registerMethod("UR_PickOffsetX", [this](const QVector<RpcValue>& p) {
        return rpcGetVerschiebungX(p);
    });

    // Pick offset Y
    registerMethod("UR_PickOffsetY", [this](const QVector<RpcValue>& p) {
        return rpcGetVerschiebungY(p);
    });

    // Also register with alternative names for backward compatibility
    registerMethod("getPalettenDaten", [this](const QVector<RpcValue>& p) {
        return rpcGetPalettenDaten(p);
    });
    registerMethod("getData", [](const QVector<RpcValue>& p) {
        return URCommonFunctions::getData(p);
    });
    registerMethod("getPaketDaten", [this](const QVector<RpcValue>& p) {
        return rpcGetPaketDaten(p);
    });
    registerMethod("getPaketPos", [this](const QVector<RpcValue>& p) {
        return rpcGetPaketPos(p);
    });
    registerMethod("getLageArten", [this](const QVector<RpcValue>& p) {
        return rpcGetLageArten(p);
    });
    registerMethod("getAnzLagen", [this](const QVector<RpcValue>& p) {
        return rpcGetAnzLagen(p);
    });
    registerMethod("getAnzPakete", [this](const QVector<RpcValue>& p) {
        return rpcGetAnzPakete(p);
    });
    registerMethod("getLageZuordnung", [this](const QVector<RpcValue>& p) {
        return rpcGetLageZuordnung(p);
    });
    registerMethod("getZwischenlagen", [this](const QVector<RpcValue>& p) {
        return rpcGetZwischenlagen(p);
    });
    registerMethod("getPaketeZuordnung", [this](const QVector<RpcValue>& p) {
        return rpcGetPaketeZuordnung(p);
    });
    registerMethod("getStartlage", [this](const QVector<RpcValue>& p) {
        return rpcGetStartlage(p);
    });
    registerMethod("getEinzelpaketLaengs", [this](const QVector<RpcValue>& p) {
        return rpcGetEinzelpaketLaengs(p);
    });
    registerMethod("getKartonhoehe", [this](const QVector<RpcValue>& p) {
        return rpcGetKartonhoehe(p);
    });
    registerMethod("getGewicht", [this](const QVector<RpcValue>& p) {
        return rpcGetGewicht(p);
    });
    registerMethod("getKlemmungAktiv", [this](const QVector<RpcValue>& p) {
        return rpcGetKlemmungAktiv(p);
    });
    registerMethod("getVerschiebungX", [this](const QVector<RpcValue>& p) {
        return rpcGetVerschiebungX(p);
    });
    registerMethod("getVerschiebungY", [this](const QVector<RpcValue>& p) {
        return rpcGetVerschiebungY(p);
    });
    registerMethod("setLage", [this](const QVector<RpcValue>& p) {
        return rpcSetLage(p);
    });
    registerMethod("getLabelInvert", [this](const QVector<RpcValue>& p) {
        return rpcGetLabelInvert(p);
    });

    // UR10 scanner status methods
    registerMethod("UR_scanner1and2niobild", [this](const QVector<RpcValue>& params) {
        Q_UNUSED(params);
        qDebug() << "RPC: UR_scanner1and2niobild called";
        return m_state ? RpcValue(m_state->scanner1and2NioValue()) : RpcValue(0);
    });
    registerMethod("UR_scanner1bild", [this](const QVector<RpcValue>& params) {
        Q_UNUSED(params);
        qDebug() << "RPC: UR_scanner1bild called";
        return m_state ? RpcValue(m_state->scanner1Value()) : RpcValue(0);
    });
    registerMethod("UR_scanner2bild", [this](const QVector<RpcValue>& params) {
        Q_UNUSED(params);
        qDebug() << "RPC: UR_scanner2bild called";
        return m_state ? RpcValue(m_state->scanner2Value()) : RpcValue(0);
    });
    registerMethod("UR_scanner1and2iobild", [this](const QVector<RpcValue>& params) {
        Q_UNUSED(params);
        qDebug() << "RPC: UR_scanner1and2iobild called";
        return m_state ? RpcValue(m_state->scanner1and2IoValue()) : RpcValue(0);
    });

    // UR20-specific methods
    registerMethod("UR_scannerStatus", [this](const QVector<RpcValue>& params) {
        if (params.isEmpty() || !m_state) {
            qWarning() << "RPC: UR_scannerStatus - missing status";
            return RpcValue(-1);
        }

        QString status = params[0].toString();
        qDebug() << "RPC: UR_scannerStatus -" << status;

        QString previousStatus = m_state->previousScannerStatus();
        qint64 currentTime = QDateTime::currentMSecsSinceEpoch();

        m_state->setScannerStatus(status);

        if (status == "True,True,True") {
            m_state->setTimestampScannerSafe(currentTime);
            if (m_state->timestampScannerFault() != 0) {
                m_state->setTimestampScannerFault(0);
            }
        } else if (previousStatus == "True,True,True") {
            if (m_state->timestampScannerFault() == 0) {
                m_state->setTimestampScannerFault(currentTime);
            }
            qint64 lastWarning = m_state->lastScannerWarningTime();
            if (lastWarning == 0 || (currentTime - lastWarning) >= 15000) {
                m_state->setLastScannerWarningTime(currentTime);
            }
        }

        return RpcValue(0);
    });
    registerMethod("UR_SetActivePalette", [this, markPaletteNotEmpty](const QVector<RpcValue>& params) {
        if (params.isEmpty() || !m_state) {
            return RpcValue(404);
        }

        int paletteNumber = params[0].toInt();
        if (paletteNumber != 1 && paletteNumber != 2) {
            return RpcValue(404);
        }

        bool isEmpty = (paletteNumber == 1) ? m_state->ur20Palette1Empty()
                                            : m_state->ur20Palette2Empty();
        if (!isEmpty) {
            return RpcValue(503);
        }

        m_state->setUr20ActivePalette(paletteNumber);
        markPaletteNotEmpty(paletteNumber);
        return RpcValue(paletteNumber);
    });
    registerMethod("UR_RequestPaletteChange", [this, markPaletteNotEmpty](const QVector<RpcValue>& params) {
        if (params.size() < 2 || !m_state) {
            return RpcValue(404);
        }

        int newPalette = params[1].toInt();
        if (newPalette != 1 && newPalette != 2) {
            return RpcValue(404);
        }

        bool newEmpty = (newPalette == 1) ? m_state->ur20Palette1Empty()
                                          : m_state->ur20Palette2Empty();
        if (!newEmpty) {
            return RpcValue(0);
        }

        m_state->setUr20ActivePalette(newPalette);
        markPaletteNotEmpty(newPalette);
        return RpcValue(1);
    });
    registerMethod("UR_GetActivePaletteNumber", [this, markPaletteNotEmpty](const QVector<RpcValue>& params) {
        Q_UNUSED(params);
        if (!m_state) {
            return RpcValue(0);
        }

        int activePalette = m_state->ur20ActivePalette();
        if (activePalette == 1) {
            if (m_state->ur20Palette1Empty()) {
                markPaletteNotEmpty(1);
                return RpcValue(1);
            }
            return RpcValue(0);
        }
        if (activePalette == 2) {
            if (m_state->ur20Palette2Empty()) {
                markPaletteNotEmpty(2);
                return RpcValue(2);
            }
            return RpcValue(0);
        }

        return RpcValue(0);
    });
    registerMethod("UR_GetPaletteStatus", [this](const QVector<RpcValue>& params) {
        if (params.isEmpty() || !m_state) {
            return RpcValue(-1);
        }

        int paletteNumber = params[0].toInt();
        if (paletteNumber == 1) {
            return RpcValue(m_state->ur20Palette1Empty() ? 1 : 0);
        }
        if (paletteNumber == 2) {
            return RpcValue(m_state->ur20Palette2Empty() ? 1 : 0);
        }
        return RpcValue(-1);
    });
    registerMethod("UR_SetZwischenLageLegen", [this](const QVector<RpcValue>& params) {
        if (params.isEmpty() || !m_state) {
            return RpcValue(0);
        }

        bool aktiv = params[0].toBool();
        m_state->setUr20Zwischenlage(aktiv);
        return RpcValue(1);
    });
    registerMethod("UR_GetKlemmungAktiv", [this](const QVector<RpcValue>& params) {
        Q_UNUSED(params);
        return m_state ? RpcValue(m_state->klemmungAktiv()) : RpcValue(false);
    });
    registerMethod("UR_GetScannerOverwrite", [this](const QVector<RpcValue>& params) {
        Q_UNUSED(params);
        if (!m_state) {
            return RpcValue::fromArray({});
        }

        QVector<RpcValue> result;
        for (bool value : m_state->scannerOverride()) {
            result.append(RpcValue(value));
        }
        return RpcValue::fromArray(result);
    });
    registerMethod("UR_GetScannerOverride", [this](const QVector<RpcValue>& params) {
        return callMethod("UR_GetScannerOverwrite", params);
    });
    registerMethod("get_available_functions", [this](const QVector<RpcValue>& params) {
        Q_UNUSED(params);
        QVector<RpcValue> result;
        for (auto it = m_methods.constBegin(); it != m_methods.constEnd(); ++it) {
            result.append(RpcValue(it.key()));
        }
        return RpcValue::fromArray(result);
    });

    qDebug() << "Registered" << m_methods.size() << "RPC methods";
}

void XmlRpcServer::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket* socket = m_server->nextPendingConnection();

        qDebug() << "New connection from" << socket->peerAddress().toString();

        connect(socket, &QTcpSocket::readyRead,
                this, &XmlRpcServer::onClientReadyRead);
        connect(socket, &QTcpSocket::disconnected,
                this, &XmlRpcServer::onClientDisconnected);
    }
}

void XmlRpcServer::onClientReadyRead()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    QByteArray data = socket->readAll();
    QString clientIp = socket->peerAddress().toString();

    qDebug() << "Received" << data.size() << "bytes from" << clientIp;

    // Parse HTTP request and extract XML-RPC call
    QString methodName;
    QVector<RpcValue> params;
    QString xmlBody = parseHttpRequest(data, methodName, params);

    if (methodName.isEmpty()) {
        // Try parsing the body as XML-RPC
        if (!parseMethodCall(xmlBody, methodName, params)) {
            QByteArray response = buildFaultResponse(-1, "Invalid request");
            socket->write(buildHttpResponse(response));
            socket->flush();
            return;
        }
    }

    qDebug() << "RPC call:" << methodName << "with" << params.size() << "params";
    emit methodCalled(methodName, clientIp);

    // Call the method
    RpcValue result = callMethod(methodName, params);

    // Build and send response
    QByteArray xmlResponse = buildResponse(result);
    QByteArray httpResponse = buildHttpResponse(xmlResponse);

    socket->write(httpResponse);
    socket->flush();
}

void XmlRpcServer::onClientDisconnected()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket) {
        qDebug() << "Client disconnected:" << socket->peerAddress().toString();
        socket->deleteLater();
    }
}

QString XmlRpcServer::parseHttpRequest(const QByteArray& data, QString& methodName,
                                        QVector<RpcValue>& params)
{
    QString request = QString::fromUtf8(data);

    // Find empty line separating headers from body
    int bodyStart = request.indexOf("\r\n\r\n");
    if (bodyStart == -1) {
        bodyStart = request.indexOf("\n\n");
        if (bodyStart != -1) bodyStart += 2;
    } else {
        bodyStart += 4;
    }

    if (bodyStart == -1 || bodyStart >= request.length()) {
        return QString();
    }

    QString body = request.mid(bodyStart);

    // Parse XML-RPC from body
    parseMethodCall(body, methodName, params);

    return body;
}

bool XmlRpcServer::parseMethodCall(const QString& xml, QString& methodName,
                                    QVector<RpcValue>& params)
{
    // Simple XML parsing for methodCall
    auto match = RE_METHOD_NAME.match(xml);
    if (!match.hasMatch()) {
        return false;
    }

    methodName = match.captured(1);

    // Parse params
    auto paramIt = RE_PARAM.globalMatch(xml);

    while (paramIt.hasNext()) {
        auto paramMatch = paramIt.next();
        QString valueXml = paramMatch.captured(1);
        params.append(parseValue(valueXml));
    }

    return true;
}

RpcValue XmlRpcServer::parseValue(const QString& xml)
{
    QString trimmed = xml.trimmed();

    // Integer
    auto match = RE_INT.match(trimmed);
    if (match.hasMatch()) {
        return RpcValue(match.captured(1).toInt());
    }

    // Double
    match = RE_DOUBLE.match(trimmed);
    if (match.hasMatch()) {
        return RpcValue(match.captured(1).toDouble());
    }

    // Boolean
    match = RE_BOOL.match(trimmed);
    if (match.hasMatch()) {
        return RpcValue(match.captured(1) == "1");
    }

    // String
    match = RE_STRING.match(trimmed);
    if (match.hasMatch()) {
        return RpcValue(match.captured(1));
    }

    // Plain string (no type tag)
    if (!trimmed.startsWith("<")) {
        return RpcValue(trimmed);
    }

    // Array
    match = RE_ARRAY.match(trimmed);
    if (match.hasMatch()) {
        RpcValue arr;
        arr.type = RpcValue::Array;

        auto valueIt = RE_VALUE.globalMatch(match.captured(1));
        while (valueIt.hasNext()) {
            auto valueMatch = valueIt.next();
            arr.arrayValue.append(parseValue(valueMatch.captured(1)));
        }
        return arr;
    }

    return RpcValue();
}

QByteArray XmlRpcServer::buildResponse(const RpcValue& result)
{
    QString xml = "<?xml version=\"1.0\"?>\n<methodResponse>\n<params>\n<param>\n<value>";

    switch (result.type) {
        case RpcValue::Int:
            xml += QString("<int>%1</int>").arg(result.intValue);
            break;
        case RpcValue::Double:
            xml += QString("<double>%1</double>").arg(result.doubleValue, 0, 'f', 6);
            break;
        case RpcValue::Bool:
            xml += QString("<boolean>%1</boolean>").arg(result.boolValue ? 1 : 0);
            break;
        case RpcValue::String:
            xml += QString("<string>%1</string>").arg(result.stringValue);
            break;
        case RpcValue::Array:
            xml += "<array><data>\n";
            for (const auto& item : result.arrayValue) {
                xml += "<value>";
                switch (item.type) {
                    case RpcValue::Int:
                        xml += QString("<int>%1</int>").arg(item.intValue);
                        break;
                    case RpcValue::Double:
                        xml += QString("<double>%1</double>").arg(item.doubleValue, 0, 'f', 6);
                        break;
                    case RpcValue::Bool:
                        xml += QString("<boolean>%1</boolean>").arg(item.boolValue ? 1 : 0);
                        break;
                    case RpcValue::String:
                        xml += QString("<string>%1</string>").arg(item.stringValue);
                        break;
                    default:
                        xml += "<nil/>";
                }
                xml += "</value>\n";
            }
            xml += "</data></array>";
            break;
        default:
            xml += "<nil/>";
    }

    xml += "</value>\n</param>\n</params>\n</methodResponse>\n";
    return xml.toUtf8();
}

QByteArray XmlRpcServer::buildFaultResponse(int code, const QString& message)
{
    QString xml = QString(
        "<?xml version=\"1.0\"?>\n"
        "<methodResponse>\n"
        "<fault>\n"
        "<value><struct>\n"
        "<member><name>faultCode</name><value><int>%1</int></value></member>\n"
        "<member><name>faultString</name><value><string>%2</string></value></member>\n"
        "</struct></value>\n"
        "</fault>\n"
        "</methodResponse>\n"
    ).arg(code).arg(message);

    return xml.toUtf8();
}

QByteArray XmlRpcServer::buildHttpResponse(const QByteArray& content)
{
    QByteArray response;
    response += "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: text/xml\r\n";
    response += QString("Content-Length: %1\r\n").arg(content.size()).toUtf8();
    response += "Server: MultipackParser/1.0\r\n";
    response += "\r\n";
    response += content;
    return response;
}

RpcValue XmlRpcServer::callMethod(const QString& name, const QVector<RpcValue>& params)
{
    if (!m_methods.contains(name)) {
        qWarning() << "Unknown RPC method:" << name;
        return RpcValue(QString("Unknown method: %1").arg(name));
    }

    try {
        return m_methods[name](params);
    } catch (const std::exception& e) {
        qWarning() << "RPC method error:" << e.what();
        return RpcValue(QString("Error: %1").arg(e.what()));
    }
}

// === RPC Method Implementations ===

RpcValue XmlRpcServer::rpcGetPalettenDaten(const QVector<RpcValue>& params)
{
    Q_UNUSED(params);
    qDebug() << "RPC: UR_Palette called";

    if (!m_state) {
        return RpcValue::fromIntArray({0, 0, 0});
    }

    return RpcValue::fromIntArray(m_state->paletteDimensions());
}

RpcValue XmlRpcServer::rpcGetPaketDaten(const QVector<RpcValue>& params)
{
    Q_UNUSED(params);
    qDebug() << "RPC: UR_Karton called";

    if (!m_state) {
        return RpcValue::fromIntArray({0, 0, 0, 0});
    }

    return RpcValue::fromIntArray(m_state->packageDimensions());
}

RpcValue XmlRpcServer::rpcGetPaketPos(const QVector<RpcValue>& params)
{
    qDebug() << "RPC: UR_PaketPos called";

    if (params.isEmpty()) {
        qWarning() << "UR_PaketPos: missing package index parameter";
        return RpcValue::fromIntArray({});
    }

    int packageIndex = params[0].toInt();

    if (!m_state) {
        return RpcValue::fromIntArray({});
    }

    QVector<int> pos = m_state->packagePosition(packageIndex);
    if (pos.isEmpty()) {
        qWarning() << "UR_PaketPos: invalid package index" << packageIndex;
        return RpcValue::fromIntArray({});
    }

    // Position format: [xPick, yPick, anglePick, xDrop, yDrop, angleDrop, count, xVec, yVec]
    // Apply transformations based on UR20 active palette if needed
    int px = pos[0], py = pos[1], pr = pos[2];
    int x = pos[3], y = pos[4], r = pos[5];
    int n = pos[6], dx = pos[7], dy = pos[8];

    // Apply label invert rotation if checkbox is checked (matching Python behavior)
    if (m_state->labelInvert()) {
        r = (r + 180) % 360;
    }

    // UR20 palette 2 coordinate transformation
    if (m_state->ur20ActivePalette() == 2) {
        // Swap x and y, adjust rotation
        int temp = x;
        x = y;
        y = temp;
        if (r == 0 || r == 180) {
            r = (r + 180) % 360;
        }
        temp = dx;
        dx = dy;
        dy = temp;
    }

    return RpcValue::fromIntArray({px, py, pr, x, y, r, n, dx, dy});
}

RpcValue XmlRpcServer::rpcGetLageArten(const QVector<RpcValue>& params)
{
    Q_UNUSED(params);
    qDebug() << "RPC: UR_LageArten called";

    if (!m_state) {
        return RpcValue(0);
    }

    return RpcValue(m_state->layerTypeCount());
}

RpcValue XmlRpcServer::rpcGetAnzLagen(const QVector<RpcValue>& params)
{
    Q_UNUSED(params);
    qDebug() << "RPC: UR_AnzLagen called";

    if (!m_state) {
        return RpcValue(0);
    }

    return RpcValue(m_state->numberOfLayers());
}

RpcValue XmlRpcServer::rpcGetAnzPakete(const QVector<RpcValue>& params)
{
    Q_UNUSED(params);
    qDebug() << "RPC: UR_AnzPakete called";

    if (!m_state) {
        return RpcValue(0);
    }

    return RpcValue(m_state->totalPackages());
}

RpcValue XmlRpcServer::rpcGetLageZuordnung(const QVector<RpcValue>& params)
{
    Q_UNUSED(params);
    qDebug() << "RPC: UR_Lagen called";

    if (!m_state) {
        return RpcValue::fromIntArray({});
    }

    return RpcValue::fromIntArray(m_state->layerAssignments());
}

RpcValue XmlRpcServer::rpcGetZwischenlagen(const QVector<RpcValue>& params)
{
    Q_UNUSED(params);
    qDebug() << "RPC: UR_Zwischenlagen called";

    if (!m_state) {
        return RpcValue::fromIntArray({});
    }

    return RpcValue::fromIntArray(m_state->intermediateLayers());
}

RpcValue XmlRpcServer::rpcGetPaketeZuordnung(const QVector<RpcValue>& params)
{
    Q_UNUSED(params);
    qDebug() << "RPC: UR_PaketeZuordnung called";

    if (!m_state) {
        return RpcValue::fromIntArray({});
    }

    return RpcValue::fromIntArray(m_state->packagesPerLayerType());
}

RpcValue XmlRpcServer::rpcGetStartlage(const QVector<RpcValue>& params)
{
    Q_UNUSED(params);
    qDebug() << "RPC: UR_Startlage called";

    if (!m_state) {
        return RpcValue(1);
    }

    return RpcValue(m_state->startLayer());
}

RpcValue XmlRpcServer::rpcGetEinzelpaketLaengs(const QVector<RpcValue>& params)
{
    Q_UNUSED(params);
    qDebug() << "RPC: UR_Quergreifen called";

    if (!m_state) {
        return RpcValue(false);
    }

    return RpcValue(m_state->einzelpaketLaengs());
}

RpcValue XmlRpcServer::rpcGetKartonhoehe(const QVector<RpcValue>& params)
{
    Q_UNUSED(params);
    qDebug() << "RPC: UR_Paket_hoehe called";

    if (!m_state) {
        return RpcValue(0);
    }

    return RpcValue(m_state->packageHeight());
}

RpcValue XmlRpcServer::rpcGetGewicht(const QVector<RpcValue>& params)
{
    Q_UNUSED(params);
    qDebug() << "RPC: UR_MasseGeschaetzt called";

    if (!m_state) {
        return RpcValue(0.0);
    }

    return RpcValue(m_state->packageWeight());
}

RpcValue XmlRpcServer::rpcGetKlemmungAktiv(const QVector<RpcValue>& params)
{
    Q_UNUSED(params);
    qDebug() << "RPC: getKlemmungAktiv called";
    return m_state ? RpcValue(m_state->klemmungAktiv()) : RpcValue(false);
}

RpcValue XmlRpcServer::rpcGetVerschiebungX(const QVector<RpcValue>& params)
{
    Q_UNUSED(params);
    qDebug() << "RPC: UR_PickOffsetX called";
    if (!m_state) {
        return RpcValue(0);
    }

    return RpcValue(static_cast<int>(m_state->pickOffsetX()));
}

RpcValue XmlRpcServer::rpcGetVerschiebungY(const QVector<RpcValue>& params)
{
    Q_UNUSED(params);
    qDebug() << "RPC: UR_PickOffsetY called";
    if (!m_state) {
        return RpcValue(0);
    }

    return RpcValue(static_cast<int>(m_state->pickOffsetY()));
}

RpcValue XmlRpcServer::rpcSetLage(const QVector<RpcValue>& params)
{
    qDebug() << "RPC: setLage called";

    if (params.isEmpty()) {
        return RpcValue(false);
    }

    int layer = params[0].toInt();
    qDebug() << "Setting current layer to:" << layer;

    if (m_state) {
        m_state->setCurrentLayer(layer);
    }

    return RpcValue(true);
}

RpcValue XmlRpcServer::rpcGetLabelInvert(const QVector<RpcValue>& params)
{
    Q_UNUSED(params);
    qDebug() << "RPC: getLabelInvert called";
    if (!m_state) {
        return RpcValue(false);
    }

    return RpcValue(m_state->labelInvert());
}

} // namespace network
} // namespace multipack
