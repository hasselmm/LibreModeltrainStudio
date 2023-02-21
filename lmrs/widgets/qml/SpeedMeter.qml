import QtQuick

Item {
    id: speedMeter

    // model properties

    property real value: 0
    property real maximumValue: 100

    property string topText
    property string bottomText

    // geometry properties

    property real size: Math.min(width, height)
    property real centerSize: Math.round(size / 10)
    property real pointerWidth: 3
    property real textHeight: Math.round(size / 3)
    property real tickDistance: Math.max(2, Math.ceil(maximumValue / 200) * 2)
    property real tickLength: Math.round(size / 20)
    property real tickWidth: 3
    property real shortTickLength: Math.round(tickLength / 2)
    property real shortTickWidth: 1

    // behavior properties

    property real measurementInterval: 125

    // style properties

    property color color: "#444"
    property color centerColor: "#333"
    property color pointerColor: "#fff"
    property color textColor: "#ddd"
    property font textFont: Qt.font({pixelSize: Math.round(tickLength * 1.2)})
    property color tickColor: "#f90"
    property color tickTextColor: "#fff"
    property font tickFont: Qt.font({pixelSize: Math.round(tickLength * 0.95)})
    property color shortTickColor: "#ddd"

    implicitWidth: 400
    implicitHeight: 400

    Rectangle { // this is the large background circle
        id: backgroundCircle

        anchors.centerIn: parent
        color: speedMeter.color

        width: 2 * radius
        height: 2 * radius
        radius: speedMeter.size/2
    }

    Text { // this is the text at the big circle's top
        anchors {
            horizontalCenter: backgroundCircle.horizontalCenter
            verticalCenterOffset: Math.round(speedMeter.size / 4) + speedMeter.tickLength
            verticalCenter: backgroundCircle.top
        }

        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter

        color: speedMeter.textColor
        font: speedMeter.textFont
        height: speedMeter.textHeight
        text: speedMeter.topText.trim()

        opacity: text !== "" ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 1500 } }
    }

    Text { // this is the text at the big circle's bottom
        anchors {
            horizontalCenter: backgroundCircle.horizontalCenter
            bottom: backgroundCircle.bottom
        }

        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter

        color: speedMeter.textColor
        font: speedMeter.textFont
        height: speedMeter.textHeight
        text: speedMeter.bottomText.trim()

        opacity: text !== "" ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 1500 } }
    }

    Rectangle { // this is the tiny circle in the middle "holding" the needle
        anchors.centerIn: parent
        color: speedMeter.centerColor

        width: 2 * radius
        height: 2 * radius
        radius: speedMeter.centerSize/2
    }

    Item { // this is the needle showing the current speed
        anchors.centerIn: parent

        rotation: tickRepeater.valueToAngle(speedMeter.value)
        Behavior on rotation { NumberAnimation { duration: speedMeter.measurementInterval } }

        width: speedMeter.pointerWidth
        height: speedMeter.size

        Rectangle {
            antialiasing: true
            color: speedMeter.pointerColor

            radius: width/2
            width: parent.width
            height: parent.height/2 - speedMeter.tickLength
            y: Math.round(speedMeter.tickLength + speedMeter.tickFont.pixelSize * 1.5)
        }
    }

    Repeater { // this repeater places the ticks
        id: tickRepeater

        property int tickCount: Math.ceil(speedMeter.maximumValue / speedMeter.tickDistance) + 1
        Behavior on tickCount { NumberAnimation {} }

        function valueToAngle(value) {
            return 270 * value / speedMeter.tickDistance / (count - 1) - 135;
        }

        model: tickCount

        Item { // this item apply proper rotation to the ticks
            property int value: Math.round(speedMeter.tickDistance * modelData)
            Behavior on value { NumberAnimation {} }

            anchors.centerIn: parent

            rotation: tickRepeater.valueToAngle(value)
            width: fullTickLabel.visible ? speedMeter.tickWidth : speedMeter.shortTickWidth
            height: speedMeter.size

            Rectangle { // this is one of the ticks
                antialiasing: true
                color: fullTickLabel.visible ? speedMeter.tickColor : speedMeter.shortTickColor
                height: fullTickLabel.visible ? speedMeter.tickLength : speedMeter.shortTickLength
                width: parent.width

                Text { // the text below a tick
                    id: fullTickLabel

                    anchors.horizontalCenter: parent.horizontalCenter
                    y: parent.height

                    color: speedMeter.tickTextColor
                    font: speedMeter.tickFont
                    visible: modelData % 5 === 0

                    text: value
                }
            }
        }
    }
}
