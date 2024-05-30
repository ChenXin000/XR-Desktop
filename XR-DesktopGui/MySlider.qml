import QtQuick 2.14
import QtQuick.Controls 2.2
import QtGraphicalEffects 1.12

Slider {
    id:slider
    width: parent.width
    from: 0
    to:4
    height: 20
    stepSize: 1
    snapMode: Slider.SnapOnRelease

    property string text: ""

    background: Rectangle {
        x:slider.leftPadding
        y:slider.topPadding + slider.availableHeight / 2 - height / 2
        width: slider.availableWidth
        height: slider.availableHeight
        border.width: 1
        border.color: "#012189ff"
        radius: height / 2
        color: "#000"

        layer.enabled: true
        layer.effect: DropShadow {
            color: "#2189ff"
            samples: 16
            spread: 0.4
            horizontalOffset: 0
        }

        Rectangle {
            width: slider.visualPosition * parent.width
            height: parent.height
            color: "#2189ff"
            radius: height / 2


        }
    }
    handle: Rectangle {
        x:slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
        y:slider.topPadding + slider.availableHeight / 2 - height / 2
        width: slider.height / 2
        height: slider.height
        color: slider.pressed ? "#2189ff" : "#000"
        border.width: 1
        border.color: "#2189ff"
        radius: 2

        layer.enabled: true
        layer.effect: DropShadow {
            color: "#2189ff"
            samples: 16
            spread: 0.2
            horizontalOffset: 0

            Label {
                x: parent.width / 2 - width / 2
                y: parent.height
                text: slider.text
                font.bold: true
                font.pointSize: 14
                color: "#eee"
            }
        }



    }
}
