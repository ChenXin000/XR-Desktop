QT += quick network websockets

CONFIG += c++11

# 应用图标
RC_ICONS = logo.ico
# 请求以管理员权限运行
CONFIG(release, debug|release) {
    QMAKE_LFLAGS += /MANIFESTUAC:\"level=\'requireAdministrator\' uiAccess=\'false\'\"
}

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Refer to the documentation for the
# deprecated API to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

INCLUDEPATH += $$PWD/include
# gstreamer 路径
GSTREAMER_PATH = D:/gstreamer/1.0/msvc_x86_64

INCLUDEPATH += $${GSTREAMER_PATH}/include/gstreamer-1.0
INCLUDEPATH += $${GSTREAMER_PATH}/include/glib-2.0
INCLUDEPATH += $${GSTREAMER_PATH}/lib/glib-2.0/include
LIBS += -L$${GSTREAMER_PATH}/lib \
    -lgmodule-2.0 \
    -lgstapp-1.0 \
    -lgobject-2.0 \
    -lglib-2.0 \
    -lgstreamer-1.0 \
    -lgstsdp-1.0 \
    -lgstwebrtc-1.0 \
    -lgstrtp-1.0

LIBS += -L$$PWD/lib -lXR-Desktop

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        desktopmanager.cpp \
        main.cpp

RESOURCES += qml.qrc

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Additional import path used to resolve QML modules just for Qt Quick Designer
QML_DESIGNER_IMPORT_PATH =

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

HEADERS += \
    websocketsignal.h \
    desktopmanager.h \
    websocket.h
