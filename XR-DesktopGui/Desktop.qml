import QtQuick 2.14
import QtQuick.Controls 2.2
import QtGraphicalEffects 1.12

Column {
    id: root
    property int connectedSum: 0
    property int desktopSum: 0
    property bool isStart: false
    signal start(var desktopID, var enc,var vidBit,var audBit,var fps,var maxSum);
    signal stop(var desktopID);

    ListModel {
        id: desktopList
        ListElement {
            isCheckable:true
            desktopID:0
        }
    }

    onDesktopSumChanged: {
        for(let i=desktopList.count;i < desktopSum;i++) {
            desktopList.append({isCheckable:false, desktopID:i})
        }
    }

    // 头部
    Row {
        id:headRow
        width: root.width - root.padding
        height: 70
        Rectangle {
            width: 165
            height: parent.height
            color: "transparent"
            Label {
                anchors.fill: parent
                verticalAlignment: Label.AlignVCenter
                horizontalAlignment: Label.AlignHCenter
                text: "Desktop"
                font.pointSize: 14
                font.bold: true
                color: "#eee"
            }
        }
        // 桌面列表
        ListView {
            id:deskListview
            width: parent.width - 200 - 165
            height: parent.height - 10
            y:headRow.height / 2 - height / 2
            orientation:ListView.Horizontal // 水平排列
            spacing: 20
            clip: true
            model:desktopList
            delegate: Button {
                id: desk_but
                width: 78
                height: parent.height
                checkable: isCheckable
                onClicked: {
                    selector_id.currentIndex = index
                }

                background: Rectangle {
                    radius: 5
                    color: desk_but.hovered ? "#552189ff" : "#000"
                }
                Column {
                    anchors.fill: parent
                    padding: 5
                    spacing: 5
                    Rectangle {
                        width: 68
                        height: 40
                        color: "#552189ff"
                        border.width: 3
                        border.color:  "#2189ff"

                        Label {
                            anchors.fill: parent
                            verticalAlignment: Label.AlignVCenter
                            horizontalAlignment: Label.AlignHCenter
                            text: desktopID + 1
                            font.pointSize: 20
                            font.bold: true
                            color: "#eee"
                        }
                    }
                    Rectangle {
                        id:bot_line
                        width: 68
                        height: 5
                        color: desk_but.checkable ? "#2189ff" : "#502189ff"
                    }
                }
            }
        }
        // 连接数量显示
        Rectangle {
            width: 200
            height: parent.height
            color: "transparent"
            Label {
                anchors.fill: parent
                verticalAlignment: Label.AlignVCenter
                horizontalAlignment: Label.AlignHCenter
                font.pointSize: 16
                font.bold: true
                color: "#eee"
                text: "已连接: " + connectedSum
            }
        }
    }

    Rectangle {
        width: root.width - root.padding
        height: 1
        color: "#2189ff"
    }

    ListView {
        id:selector_id
        width: parent.width
        height: parent.height - headRow.height
        model: desktopList
        clip: true
        highlightRangeMode: ListView.StrictlyEnforceRange // 设置currentIndex动态变换
        maximumFlickVelocity:7000 // 滚动速度
        orientation:ListView.Horizontal // 水平排列
        snapMode:ListView.SnapOneItem   // 单张切换
        boundsBehavior:Flickable.StopAtBounds   // 禁止边缘过度滑动
        highlightMoveDuration:300 // currentIndex滚动速度
        delegate: Selector {
            width: root.width
            height: root.height - headRow.height
            isStart: root.isStart
            onStart: {
                root.start(desktopID, enc, vidBit, audBit, fps, maxSum);
            }
            onStop: {
                root.stop(desktopID)
            }
        }

        onCurrentIndexChanged: {
            for(let i=0;i<desktopList.count;i++) {
                desktopList.get(i).isCheckable = false
            }
            desktopList.get(currentIndex).isCheckable = true
            deskListview.currentIndex = currentIndex
        }
    }


}

