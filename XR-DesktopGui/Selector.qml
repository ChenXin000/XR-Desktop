import QtQuick 2.14
import QtQuick.Controls 2.2
import QtGraphicalEffects 1.12

Column {
    id: root
    property bool isStart: false
    signal start(var enc,var vidBit,var audBit,var fps,var maxSum);
    signal stop();

    padding: 0

    ListModel {
        id: encListModel
        ListElement{
            encName: "X264"
            isCheckable: true
        }
        ListElement{
            encName: "NVH264"
            isCheckable: false
        }
        ListElement{
            encName: "VP8"
            isCheckable: false
        }
    }

    ListModel {
        id: listModel
        ListElement {
            titleText: "视频比特率"
            defValue: 1
            value_: "4000K"
            infoList: [
                ListElement{v:"2000K"},
                ListElement{v:"4000K"},
                ListElement{v:"8000K"},
                ListElement{v:"16000K"}
            ]
        }
        ListElement {
            titleText: "音频比特率"
            defValue: 1
            value_: "256K"
            infoList: [
                ListElement{v:"128K"},
                ListElement{v:"256K"},
                ListElement{v:"512K"}
            ]
        }
        ListElement {
            titleText: "帧率"
            defValue: 0
            value_: "Auto"
            infoList: [
                ListElement{v:"Auto"},
                ListElement{v:"30"},
                ListElement{v:"60"},
                ListElement{v:"120"}
            ]
        }
        ListElement {
            titleText: "最大连接数"
            defValue: 100
            value_: "100"
            infoList: [ListElement{v:"1024"}]
        }
    }

    ListView {
        width: root.width
        height: parent.height
        model: listModel
        spacing: 20
        clip: true
        boundsBehavior:Flickable.StopAtBounds   // 禁止边缘过度滑动
        // 头部编码器选择
        header:Row {
            width: parent.width - 40
            x:parent.width / 2 - width / 2
            height: 100
            spacing: 10
            Label {
                id:label_rect
                width: 140
                y: parent.height / 2 - height / 2
                font.pointSize: 14
                font.bold: true
                color: "#eee"
                text: "编码器"
                horizontalAlignment: Label.AlignHCenter
            }
            //  编码器列表
            ListView {
                width: parent.width - label_rect.width - parent.spacing
                height: 50
                y: parent.height / 2 - height / 2
                orientation:ListView.Horizontal // 水平排列
                boundsBehavior:Flickable.StopAtBounds   // 禁止边缘过度滑动
                model: encListModel
                spacing: 20
                delegate: Button {
                    id:enc_but
                    width: parent.height * 2
                    height: parent.height
                    checkable: isCheckable
                    onClicked: {
                        if(isCheckable) return
                        for(let i=0;i<encListModel.count;i++) {
                            encListModel.get(i).isCheckable = false
                        }
                        isCheckable = true
                    }
                    background: Rectangle {
                        anchors.fill: parent
                        border.width: enc_but.hovered ? 0 : 1
                        border.color: "#082189ff"
                        color: enc_but.pressed ? "#882189ff" : enc_but.checkable ? "#2189ff" : "#000"
                        radius: 5
                        Label {
                            anchors.fill: parent
                            text: encName
                            font.pointSize: 14
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

        }
        // 参数设置列表
        delegate: Row {
            width: parent.width - 40
            height: 50
            x:parent.width / 2 - width / 2
            Label {
                id: label_row
                width: 140
                y: parent.height / 2 - height / 2
                font.pointSize: 14
                font.bold: true
                color: "#eee"
                text: titleText
                horizontalAlignment: Label.AlignHCenter
            }
            MySlider {
                y: parent.height / 2 - height / 2
                width: parent.width - label_row.width
                height: 30
                padding: 10
                value: defValue
                from: index === 3 ? 1 : 0
                to: index === 3 ? parseInt(infoList.get(0).v) : infoList.count - 1
                text: index === 3 ? value : infoList.get(value).v

                onValueChanged: {
                    if(index === 3) {
                        value_ = String(value)
                    }
                    else {
                        value_ = infoList.get(value).v
                    }
                }
            }
        }
        // 底部开始按钮
        footer: Rectangle {
            width: parent.width - 40
            x:parent.width / 2 - width / 2
            height: 120
            color: "transparent"
            Button {
                id: start_but
                width: parent.width / 3
                height: parent.height / 2 + 10
                x: parent.width / 2 - width / 2
                y: parent.height - height - 10
                onClicked: {
                    if(!isStart) {
                        let enc = ""
                        for(let i=0;i<encListModel.count;i++) {
                            if(encListModel.get(i).isCheckable)
                                enc = encListModel.get(i).encName
                        }
                        start(enc,
                              listModel.get(0).value_,
                              listModel.get(1).value_,
                              listModel.get(2).value_,
                              listModel.get(3).value_)
                    } else {
                        stop();
                    }
                }

                background: Rectangle {
                    anchors.fill: parent
                    border.width: start_but.hovered ? 0 : 1
                    border.color: "#082189ff"
                    color: isStart ? "#2189ff" : "#000"
                    radius: 5
                    Label {
                        anchors.fill: parent
                        text: "START"
                        font.pointSize: 18
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
    }
}

