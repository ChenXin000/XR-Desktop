#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <QObject>
#include <QWebSocket>

class WebSocket : public QObject
{
    Q_OBJECT
public:
    explicit WebSocket(QWebSocket* qsock);
    virtual ~WebSocket();

public:
    void sendText(const QString &msg);
    void close();

signals:
    void sendSignal(const QString &msg);
    void closeSignal();

    void textMessageReceived(const QString& msg);
    void disconnected();

protected:
    void sendTextSolt(const QString &msg);
    void closeSolt();

protected:
    QWebSocket *qWebSocket;
};

inline WebSocket::WebSocket(QWebSocket* qsock) : qWebSocket(qsock)
{
    connect(qWebSocket, &QWebSocket::textMessageReceived, this, &WebSocket::textMessageReceived);
    connect(qWebSocket, &QWebSocket::disconnected,this, &WebSocket::disconnected);
    connect(this, &WebSocket::sendSignal, this, &WebSocket::sendTextSolt);
    connect(this, &WebSocket::closeSignal, this, &WebSocket::closeSolt);
}
inline WebSocket::~WebSocket()
{
    qWebSocket->close();
    qWebSocket->deleteLater();
}
// 发送文本, 使用信号解决无法跨线程问题
inline void WebSocket::sendText(const QString &msg)
{
    emit sendSignal(msg);
}
// 关闭
inline void WebSocket::close()
{
    emit closeSignal();
}

inline void WebSocket::sendTextSolt(const QString &msg)
{
    qWebSocket->sendTextMessage(msg);
}

inline void WebSocket::closeSolt()
{
    qWebSocket->close();
}
#endif // WEBSOCKET_H
