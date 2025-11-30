#ifndef WEBSOCKETCLIENT_H
#define WEBSOCKETCLIENT_H

#include <QObject>
#include <QWebSocket>
#include <QTimer>

class WebSocketClient : public QObject
{
    Q_OBJECT
public:
    explicit WebSocketClient(const QUrl &url, QObject *parent = nullptr);
    ~WebSocketClient();

    void start();

signals:
    void serverOnline();
    void serverOffline();
    void textMessageReceivedSignal(const QString &msg);

private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString &message);
    void tryReconnect();
    void onError(QAbstractSocket::SocketError error);

private:
    QWebSocket m_webSocket;
    QUrl m_url;
    QTimer m_reconnectTimer;
};

#endif // WEBSOCKETCLIENT_H
