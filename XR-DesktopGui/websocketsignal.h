#ifndef WEBSOCKETSIGNAL_H
#define WEBSOCKETSIGNAL_H

#include <QObject>
#include <QWebSocket>
#include <QJsonObject>
#include <QJsonDocument>
#include "websocket.h"
#include "WebrtcSink/Signalling.h"

class WebSocketSignal : public WebSocket, public Signalling
{
    Q_OBJECT
private:
    explicit WebSocketSignal(QWebSocket* qsock);

public:
    virtual ~WebSocketSignal(){};
    static WebSocketSignal* create(QWebSocket* qsock);

    bool sendDescription(std::string &sdp) override;
    bool sendIceCandidate(gint sdpmlineindex, std::string &candidate) override;
    void close() override;

private:
    void messageReceivedSlot(const QString& msg);

signals:
    // 建立屏幕共享连接的信号
    void connection(Signalling* signal, const QJsonObject& media);
};

inline WebSocketSignal::WebSocketSignal(QWebSocket* qsock)
    : WebSocket(qsock)
{
    connect(this, &WebSocket::textMessageReceived, this, &WebSocketSignal::messageReceivedSlot);
}
//创建一个新的WebSocketSignal
inline WebSocketSignal* WebSocketSignal::create(QWebSocket* qsock)
{
    return new WebSocketSignal(qsock);
}
// 发送媒体描述
inline bool WebSocketSignal::sendDescription(std::string &sdp)
{
    QJsonObject rootObj;
    QJsonObject dataObj;
    dataObj.insert("type","offer");
    dataObj.insert("sdp",sdp.c_str());

    rootObj.insert("type","sdp");
    rootObj.insert("data",dataObj);
    QJsonDocument doc(rootObj);

    WebSocket::sendText(doc.toJson());
    return WebSocket::qWebSocket->isValid();
}
// 发送 ICE 候选
inline bool WebSocketSignal::sendIceCandidate(gint sdpmlineindex, std::string &candidate)
{
    QJsonObject rootObj;
    QJsonObject dataObj;
    dataObj.insert("sdpMLineIndex",(int)sdpmlineindex);
    dataObj.insert("candidate",candidate.c_str());

    rootObj.insert("type","ice");
    rootObj.insert("data",dataObj);
    QJsonDocument doc(rootObj);

    WebSocket::sendText(doc.toJson());
    return WebSocket::qWebSocket->isValid();
}
inline void WebSocketSignal::close()
{
    WebSocket::close();
}

// 接收消息槽
inline void WebSocketSignal::messageReceivedSlot(const QString &msg)
{
    QJsonDocument doc = QJsonDocument::fromJson(msg.toLocal8Bit().data());
    if(doc.isNull())
    {
        WebSocket::close();
        return ;
    }
    QJsonObject rootObj = doc.object();
    QJsonValue type = rootObj.value("type");
    if(type == "ice")
    {
        QJsonObject iceObj = rootObj.value("data").toObject();
        unsigned int sdpmlineindex = iceObj.value("sdpMLineIndex").toInt();
        std::string candidate = iceObj.value("candidate").toString().toStdString();
        Signalling::emitReadIceCandidate(sdpmlineindex, candidate);
    }
    else if(type == "sdp")
    {
        QJsonObject sdpObj = rootObj.value("data").toObject();
        std::string sdp = sdpObj.value("sdp").toString().toStdString();
        Signalling::emitReadDescription(sdp);
    }
    else if(type == "connect")
    {
        emit connection(this, rootObj.value("data").toObject());
    }
    else
    {
        WebSocket::close();
    }

}
#endif // WEBSOCKETSIGNAL_H
