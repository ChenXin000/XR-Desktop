#include "desktopmanager.h"

DesktopManager::DesktopManager(QObject *parent)
    : QObject(parent)
    , webSocketServer("WebSocketServer",QWebSocketServer::NonSecureMode)
    , connectSum(0)
{
    inputManager.updateOpcode();

    getDesktopSum();
    // 监听websocket Server
    connect(&webSocketServer,&QWebSocketServer::newConnection,this,&DesktopManager::newConnection);
    loadAddressPort();
}

int DesktopManager::getDesktopSum()
{
    desktopSum = DX::GetMonitors().size();
    if(desktopSum > (int)desktopList.size())
        desktopList.resize(desktopSum);
    return desktopSum;
}

bool DesktopManager::createDesktop(int id, int enc, int vidBit, int audBit, int fps, int maxSum)
{
    if(id < 0 || id >= (int)desktopList.size())
        return false;

    XR_Desktop* desk = new XR_Desktop(id);

    desk->onChannelMessage([this](WebrtcBin* bin, std::string &msg){
        const char* str = msg.c_str();
        int len = msg.size();

        if(str[0] == 'i')
        {
            IN_EVENT_T ty = inputManager.parser(str + 2,len - 2);
            if(ty == IN_OPCODE_OK)
            {
                bin->sendChannelMessage("{\"type\":\"o\",\"data\":true}");
            }
            else if(ty == IN_OPCODE_ERROR)
            {
                bin->sendChannelMessage("{\"type\":\"o\",\"data\":false}");
            }
        }
    });

    desk->onChannelOpen([this](WebrtcBin* bin){
        emit newConnect(++connectSum);
    });

    desk->onChannelClose([this](WebrtcBin* bin){
        emit disConnect(--connectSum);
    });

    desktopList[id] = desk;

    if(!desk->initialize((EncType)enc,vidBit,audBit))
        goto RETURN;
    if(fps)
        desk->setFramerate(fps,true);
    else
        desk->setFramerate(60,false);

    if(!desk->start())
        goto RETURN;

    return true;

RETURN:
    deleteDesktop(id);
    return false;
}

void DesktopManager::deleteDesktop(int id)
{
    if(id < 0 || id >= (int)desktopList.size())
        return ;

    XR_Desktop* desk = desktopList[id];
    desktopList[id] = nullptr;
    if(desk)
    {
        delete desk;
    }

}
// 设置 nginx 配置文件的地址和端口
bool DesktopManager::setAddressPort(const QString ip, quint16 port)
{

    QFile file(QDir::currentPath() + "/nginx-1.22.1/conf/nginx.conf");
    if(file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QString data = file.readAll();
        file.close();

        int b = data.indexOf("listen");
        int e = data.indexOf('\n',b);
        if(b < 0 || e < 0)
            return false;
        data.replace(b,e-b,"listen " + QString::number(port) + ';');

        b = data.indexOf("server_name");
        e = data.indexOf('\n',b);
        if(b < 0 || e < 0)
            return false;
        data.replace(b,e-b,"server_name " + ip + ';');

        if(file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            file.write(data.toUtf8());
            file.close();
            webSocketServer.close();
            webSocketServer.listen(QHostAddress(ip),port + 1);
            QProcess::startDetached("nginx-1.22.1/nginx.exe",QStringList() << "-s" << "reload","nginx-1.22.1");
            return true;
        }
    }
    return false;
}
// 获取操作码
QString DesktopManager::getOpcode() {
    return inputManager.getOpcode().c_str();
}
// 更新操作码
void DesktopManager::updateOpcode() {
    inputManager.updateOpcode();
}

// 加载 nginx 配置文件的地址和端口
void DesktopManager::loadAddressPort()
{
    QFile file(QDir::currentPath() + "/nginx-1.22.1/conf/nginx.conf");
    if(file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QString data = file.readAll();
        file.close();

        int b = data.indexOf("listen");
        int e = data.indexOf(';',b);
        int port = 80;
        if(b >= 0 && e >= 1)
            port = data.mid(b+6,e-b-6).toInt();

        b = data.indexOf("server_name");
        e = data.indexOf(';',b);
        QString ip = "localhost";
        if(b >= 0 && e >= 7)
            ip = data.mid(b+12,e-b-12);
        if(webSocketServer.listen(QHostAddress(ip),port + 1))
            qDebug() << ip << port;
    }
}
// 新的 websocket 连接
void DesktopManager::newConnection()
{
    WebSocketSignal* signal = WebSocketSignal::create(webSocketServer.nextPendingConnection());

    QJsonArray array;
    for(int i=0;i<desktopSum;i++)
    {
        if(desktopList[i])
            array.append(i);
    }
    QJsonObject rootObj;
    rootObj.insert("type","HELLO");
    rootObj.insert("data",array);
    QJsonDocument doc(rootObj);

    connect(signal,&WebSocketSignal::connection,this,&DesktopManager::connectionSlot);
    signal->sendText(doc.toJson());
}
// 新的屏幕共享连接
void DesktopManager::connectionSlot(Signalling* signal, const QJsonObject& media)
{
    int deskID = media.value("desktopID").toInt();
    bool vid = media.value("video").toBool();
    bool aud = media.value("audio").toBool();


    if(deskID >= 0 && deskID < desktopSum)
    {
        if(desktopList[deskID])
        {
            desktopList[deskID]->newConnect(signal, vid, aud);
        }
    }
}

