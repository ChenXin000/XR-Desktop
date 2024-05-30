import QtQuick 2.14
import QtQuick.Window 2.2
import QtQuick.Controls 2.2
import QtGraphicalEffects 1.12
import DesktopManager 1.0
import Qt.labs.platform 1.1

Window {
    id: root
    visible: true
    width: 960
    height: 630
    color: "#000"
    title: qsTr("XR-Desktop")
    property bool isFirst: true

    onClosing: {
        close.accepted = false;
        root.hide();
        if(isFirst) {
            systemTrayIcon.showMessage("已最小化到系统托盘","点击消息框不再提示",SystemTrayIcon.Information)
        }
    }

    DesktopManager {
        id: desktopManager
        property string opcode: getOpcode()
        onNewConnect: {
            desktop.connectedSum = num
        }
        onDisConnect: {
            desktop.connectedSum = num
        }
    }

    SystemTrayIcon {
        id:systemTrayIcon
        visible: true
        icon.mask: true
        icon.source: "qrc:/images/x.png"

        onActivated: {
            if(reason === SystemTrayIcon.Trigger) {
                root.show();
            }
        }

        onMessageClicked: root.isFirst = false

        menu: Menu {
            MenuItem {
                text: qsTr("退出")
                onTriggered: Qt.quit()
            }
        }
    }

    Column {
        anchors.fill: parent
        // 头部
        Rectangle {
            color: "transparent"
            width: parent.width
            height: 60
            // 操作码
            Rectangle {
                width: 350
                height: 50
                color: "transparent"
                anchors.centerIn: parent

                ListView {
                    anchors.fill: parent
                    model: desktopManager.opcode.length
                    orientation:ListView.Horizontal // 水平排列
                    boundsBehavior:Flickable.StopAtBounds   // 禁止边缘过度滑动
                    spacing: 10
                    delegate: Rectangle {
                        width: 50
                        height: 50
                        color: "#000"
                        border.color: "#2189ff"
                        radius: 5
                        Label {
                            anchors.fill: parent
                            verticalAlignment: Label.AlignVCenter
                            horizontalAlignment: Label.AlignHCenter
                            text: opc_mouse.isHover ? desktopManager.opcode[index] : "*"
                            font.pointSize: 20
                            font.bold: true
                            color: "#eee"
                            font.capitalization: Font.AllUppercase
                        }

                        layer.enabled: true
                        layer.effect: DropShadow {
                            color: "#2189ff"
                            samples: 16
                            spread: 0.4
                            horizontalOffset: 0
                        }
                    }
                }

                MouseArea {
                    id:opc_mouse
                    property bool isHover: false
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        desktopManager.updateOpcode();
                        desktopManager.opcode = desktopManager.getOpcode();
                    }
                    onEntered: { isHover = true; }
                    onExited: { isHover = false; }
                }


            }

            Button {
                id: quit_but
                width: 100
                height: 50

                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.rightMargin: 20
                onClicked: Qt.quit()

                background: Rectangle {
                    anchors.fill: parent
                    border.width: quit_but.hovered ? 0 : 1
                    border.color: "#082189ff"
                    color: quit_but.pressed ? "#2189ff" : "#000"
                    radius: 5
                    Label {
                        anchors.fill: parent
                        text: "Quit"
                        font.pointSize: 15
                        font.bold: true
                        color: "#eee"
                        verticalAlignment: Label.AlignVCenter
                        horizontalAlignment: Label.AlignHCenter
                    }
                    layer.enabled: true
                    layer.effect: DropShadow {
                        color: "#2189ff"
                        samples: 16
                        spread: 0.4
                        horizontalOffset: 0
                    }
                }
            }
        }

        Rectangle {
            width: parent.width - 50
            height: parent.height
            x: parent.width / 2 - width / 2
            color: "#000"
            Desktop {
                id: desktop
                desktopSum: desktopManager.getDesktopSum()
                width: parent.width
                height: root.height
                onStart:  {
                    let e = 1
                    switch(enc) {
                    case "X264":
                        e = 1
                        break;
                    case "NVH264":
                        e = 2
                        break;
                    case "VP8":
                        e = 4
                        break;
                    }
                    let vid = parseInt(vidBit)
                    let aud = parseInt(audBit)
                    let f = 0
                    if(fps !== "Auto")
                        f = parseInt(fps)

                    if(desktopManager.createDesktop(desktopID,e,vid,aud,f,parseInt(maxSum)))
                        isStart = true;
                    else isStart = false;
                }
                onStop: {
                    desktopManager.deleteDesktop(desktopID)
                    connectedSum = 0
                    isStart = false;
                }
            }
        }
    }
}
