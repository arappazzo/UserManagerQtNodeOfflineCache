#include "websocketclient.h"
#include <QDebug>
#include <QAbstractSocket>
#include <QJsonDocument>
#include <QJsonObject>

WebSocketClient::WebSocketClient(const QUrl &url, QObject *parent)
    : QObject(parent), m_url(url)
{
    connect(&m_webSocket, &QWebSocket::connected, this, &WebSocketClient::onConnected);
    connect(&m_webSocket, &QWebSocket::disconnected, this, &WebSocketClient::onDisconnected);
    connect(&m_webSocket, &QWebSocket::textMessageReceived, this, &WebSocketClient::onTextMessageReceived);

    connect(&m_webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
            this, &WebSocketClient::onError);

    m_reconnectTimer.setInterval(2000);
    m_reconnectTimer.setSingleShot(false);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &WebSocketClient::tryReconnect);
}

WebSocketClient::~WebSocketClient()
{
    m_reconnectTimer.stop();
    m_webSocket.close();
}

void WebSocketClient::start()
{
    tryReconnect();
}

void WebSocketClient::onConnected()
{
    qDebug() << "WebSocket connected";
    m_reconnectTimer.stop();
    emit serverOnline();
}

void WebSocketClient::onDisconnected()
{
    qDebug() << "WebSocket disconnected";
    emit serverOffline();
    m_reconnectTimer.start();
}

void WebSocketClient::onTextMessageReceived(const QString &message)
{
    qDebug() << "WebSocket message:" << message;

    // forward raw message
    emit textMessageReceivedSignal(message);

    // also parse simple event
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (doc.isObject()) {
        QJsonObject obj = doc.object();
        if (obj.contains("event") && obj["event"].toString() == "serverOnline") {
            emit serverOnline();
        }
    }
}

void WebSocketClient::tryReconnect()
{
    if (m_webSocket.state() != QAbstractSocket::ConnectedState) {
        qDebug() << "Trying to connect to WebSocket server at" << m_url;
        m_webSocket.open(m_url);
    }
}

void WebSocketClient::onError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);
    qWarning() << "WebSocket error:" << m_webSocket.errorString();
    emit serverOffline();
}
