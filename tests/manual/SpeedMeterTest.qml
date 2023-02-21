import "../../lmrs/widgets/qml" as LmrsWidgets

import QtQuick

LmrsWidgets.SpeedMeter {
    id: speedMeter

    width: 400
    height: 400

    component ValueRow: Row {
        id: valueRow

        required property real value
        required property real maximumValue
        required property real increment

        signal valueAccepted(var value)

        TextInput {
            text: value
            validator: IntValidator { bottom: 0; top: valueRow.maximumValue }
            width: 20

            onTextChanged: valueAccepted(parseInt(text))
        }

        MouseArea {
            height: parent.height
            width: height

            Rectangle {
                anchors.fill: parent
                color: "silver"
            }

            Text {
                anchors.centerIn: parent
                text: "<"
            }

            onClicked: valueAccepted(Math.max(0, valueRow.value - valueRow.increment))
        }

        MouseArea {
            height: parent.height
            width: height

            Rectangle {
                anchors.fill: parent
                color: "silver"
            }

            Text {
                anchors.centerIn: parent
                text: ">"
            }

            onClicked: valueAccepted(Math.min(valueRow.maximumValue, valueRow.value + valueRow.increment))
        }
    }

    Column {
        ValueRow {
            value: speedMeter.maximumValue
            maximumValue: 1000
            increment: 10

            onValueAccepted: function(value) {
                speedMeter.maximumValue = value;
            }
        }

        ValueRow {
            value: speedMeter.value
            maximumValue: speedMeter.maximumValue
            increment: 5

            onValueAccepted: function(value) {
                speedMeter.value = value;
            }
        }

        Text {
            text: speedMeter.tickDistance
        }

        Text {
            text: tickRepeater.count
        }
    }
}
