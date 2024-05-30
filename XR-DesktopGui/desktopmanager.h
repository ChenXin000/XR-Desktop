#ifndef DESKTOPMANAGER_H
#define DESKTOPMANAGER_H

#include <QObject>
#include <vector>
#include <QWebSocketServer>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcess>
#include <QFile>
#include <QDir>

#include <QNetworkInterface>

#include "websocketsignal.h"
#include "XR_Desktop.h"

class DesktopManager : public QObject
{
    Q_OBJECT
public:
    explicit DesktopManager(QObject *parent = nullptr);

    Q_INVOKABLE int getDesktopSum();
    Q_INVOKABLE bool createDesktop(int id, int enc, int vidBit, int audBit, int fps, int maxSum);
    Q_INVOKABLE void deleteDesktop(int id);
    Q_INVOKABLE bool setAddressPort(const QString ip,quint16 port);
    Q_INVOKABLE QString getOpcode();
    Q_INVOKABLE void updateOpcode();

private:
    int desktopSum;
    std::vector<XR_Desktop*> desktopList;
    QWebSocketServer webSocketServer;
    InputManager inputManager;

    int connectSum;

    void loadAddressPort();
    void newConnection();
    // 请求webrtcBin连接
    void connectionSlot(Signalling* signal, const QJsonObject &media);

signals:
    void newConnect(int num);
    void disConnect(int num);

};

#endif // DESKTOPMANAGER_H
