#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include "QDebug"
#include "desktopmanager.h"
#include <QIcon>
#include <QFont>
#include <QSharedMemory>

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QGuiApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/images/x.png"));
    QFont font;
    font.setFamily("Arial");
    app.setFont(font);

    QSharedMemory sharedMem("XR-Desktop");
    if(!sharedMem.create(1))
    {
        qApp->quit();
        return -1;
    }

    gst_init(&argc,&argv);
    qmlRegisterType<DesktopManager>("DesktopManager",1,0,"DesktopManager");

    QQmlApplicationEngine engine;
    const QUrl url(QStringLiteral("qrc:/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.load(url);

    // 结束所有正在运行的 nginx
    QProcess process;
    process.start("taskkill.exe /f /im nginx.exe");
    process.waitForFinished();

    QProcess::startDetached("nginx-1.22.1/nginx.exe",QStringList(),"nginx-1.22.1");
    int ret = app.exec();
    QProcess::startDetached("nginx-1.22.1/nginx.exe",QStringList() << "-s" << "stop","nginx-1.22.1");

    return ret;
}
