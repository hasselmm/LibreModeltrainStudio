import Lmrs.Core.Automation as Automation

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

FocusScope {
    id: automationView

    property int currentIndex: -1 // FIXME: we also need a current index for currently selection action or binding
    property alias model: repeater.model

    property color borderColor: "black"
    property color selectedBorderColor: "blue"
    property color separatorColor: "black"
    property color itemColor: "silver"
    property color selectedItemColor: "lightblue"

    property real itemSpacing: 24
    property real horizontalItemPadding: 8
    property real verticalItemPadding: 4
    property real innerSpacing: 4
    property real actionSpacing: 12
    property real bindingPadding: 30
    property real bindingArrowLength: 8

    component BindingView : Canvas { // ================================================================================
        id: bindingView

        required property bool firstItem
        required property bool lastItem
        required property real anchorLine

        implicitWidth: automationView.bindingPadding * 2 + bindingInformation.width
        implicitHeight: automationView.bindingPadding * 2

        onPaint: function(region) {
            const ctx = getContext("2d");
            const cx = Math.ceil(automationView.bindingPadding) - 0.5;
            const cy = Math.ceil(anchorLine) - 0.5;
            const al = automationView.bindingArrowLength;

            ctx.moveTo(firstItem ? 0 : cx, cy);
            ctx.lineTo(width, cy);

            ctx.moveTo(cx, firstItem ? cy : 0);
            ctx.lineTo(cx, lastItem ? cy : height);

            ctx.stroke();

            ctx.moveTo(width, cy);
            ctx.lineTo(width - al, cy - al/2);
            ctx.lineTo(width - al, cy + al/2);

            ctx.fill();
        }

        Column {
            id: bindingInformation

            x: Math.ceil(automationView.bindingPadding) + 2 // FIXME: proper properties for padding at least
            y: Math.ceil(bindingView.anchorLine) + 2 // ...but maybe also for lines

            Repeater {
                model: bindingView.firstItem ? ["Primary Address → Address", "Primary State → State"]
                                             : [qsTr("No bindings defined.")]
                Text { text: modelData; font.pixelSize: 12 }
            }
        }
    }

    component ItemView : FocusScope { // ===============================================================================
        id: itemView

        required property Automation.Item item
        readonly property real anchorLine: titleSeparator.y + titleSeparator.height/2 // FIXME: find proper anchor line for collapsed items

        property bool expanded: true

        clip: true
        implicitWidth: content.width
        implicitHeight: content.height

        Rectangle { // background effect of the item view --------------------------------------------------------------
            anchors.fill: parent

            color: itemView.activeFocus ? automationView.selectedItemColor : automationView.itemColor
            border.color: itemView.activeFocus ? automationView.selectedBorderColor : automationView.borderColor
            border.width: 1

            radius: Math.max(automationView.horizontalItemPadding, automationView.verticalItemPadding)
        }

        Column { // actual content of the item view --------------------------------------------------------------------
            id: content

            anchors.centerIn: parent

            topPadding: automationView.verticalItemPadding
            bottomPadding: automationView.verticalItemPadding

            leftPadding: automationView.horizontalItemPadding
            rightPadding: automationView.horizontalItemPadding

            spacing: automationView.innerSpacing

            width: {
                let max = title.implicitWidth;

                // bypassing parameterColumn.implicitWidth to avoid an infinite loop during polish()
                for (let i = 0; i < parameterRepeater.count; ++i)
                    max = Math.max(max, parameterRepeater.itemAt(i).implicitWidth);

                return max + leftPadding + rightPadding;
            }

            Text {
                id: title

                font.bold: true
                text: qsTr(itemView.item?.name)
                width: parent.width

                MouseArea {
                    anchors.fill: parent

                    onClicked: {
                        itemView.forceActiveFocus();
                    }

                    onDoubleClicked: {
                        itemView.expanded ^= true;
                        itemView.forceActiveFocus();
                    }
                }
            }

            Rectangle {
                id: titleSeparator

                color: automationView.separatorColor
                x: -parent.leftPadding
                width: parent.width
                height: 1

                opacity: parameterColumn.opacity
                visible: opacity > 0
            }

            Column { // parameters of the automation item --------------------------------------------------------------
                id: parameterColumn

                opacity: itemView.expanded ? 1 : 0
                spacing: automationView.innerSpacing

                width: parent.width - parent.leftPadding - parent.rightPadding
                height: implicitHeight * opacity

                Behavior on opacity {
                    NumberAnimation { duration: 120 }
                }

                Repeater {
                    id: parameterRepeater

                    model: itemView.item?.parameters

                    Item { // cannot use Row here because expanding parameterLabel to fit space would cause a polish() loop
                        id: parameterRow

                        readonly property Automation.parameter parameter: modelData

                        implicitWidth: parameterLabel.implicitWidth + parameterEditor.implicitWidth + automationView.innerSpacing
                        implicitHeight: Math.max(parameterLabel.implicitHeight, parameterEditor.implicitHeight)

                        width: parent.width

                        Text {
                            id: parameterLabel

                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr(parameterRow.parameter.name) + ':'
                        }

                        AutomationParameterEditor {
                            id: parameterEditor

                            anchors.verticalCenter: parent.verticalCenter
                            anchors.right: parent.right
                            item: itemView.item
                            parameter: parameterRow.parameter
                        }
                    }
                }

                Text {
                    text: qsTr("No parameters available.")
                    visible: parameterRepeater.count === 0
                }
            }
        }

        TapHandler {
            onTapped: itemView.forceActiveFocus() // ensure we get focus when the item gets tapped somewhere
        }
    }

    component EventView : FocusScope { // ==============================================================================
        id: eventView

        property Automation.Event event

        width: eventItemContent.width
        height: eventItemContent.height

        Row {
            id: eventItemContent

            ItemView {
                id: itemDetail

                focus: true
                item: eventView.event
            }

            Column {
                id: actionColumn

                readonly property real actionWidth: {
                    let max = 0;

                    for (let i = 0; i < actionRepeater.count; ++i)
                        max = Math.max(max, actionRepeater.itemAt(i).actionWidth);

                    return max;
                }

                readonly property real bindingWidth: {
                    let max = 0;

                    for (let i = 0; i < actionRepeater.count; ++i)
                        max = Math.max(max, actionRepeater.itemAt(i).bindingWidth);

                    return max;
                }

                width: (actionsHint.visible ? actionsHint.implicitWidth : actionWidth + bindingWidth)
                       * actionColumn.opacity

                opacity: itemDetail.expanded ? 1 : 0
                visible: opacity > 0

                Behavior on opacity {
                    NumberAnimation { duration: 120 }
                }

                Repeater {
                    id: actionRepeater

                    model: eventView.event?.actions // FIXME: use item model to avoid the entire view being repainted and loosing state when actions get added, or removed

                    Row {
                        readonly property real actionWidth: actionDetail.implicitWidth
                        readonly property real bindingWidth: bindingDetail.implicitWidth

                        BindingView {
                            id: bindingDetail

                            firstItem: model.index === 0
                            lastItem: model.index === actionRepeater.count - 1
                            anchorLine: actionDetail.anchorLine

                            width: actionColumn.bindingWidth
                            height: actionDetail.height + automationView.actionSpacing
                        }

                        ItemView {
                            id: actionDetail

                            item: modelData
                        }
                    }
                }

                Text {
                    id: actionsHint

                    leftPadding: automationView.bindingPadding
                    text: qsTr("No actions defined.")
                    visible: actionRepeater.count === 0
                }
            }
        }
    }

    Rectangle { // =====================================================================================================
        anchors.fill: parent
        color: "gray"
    }

    Flickable {
        anchors {
            fill: parent
            leftMargin: Math.ceil(automationView.itemSpacing/2)
            rightMargin: Math.ceil(automationView.itemSpacing/2)
            topMargin: Math.ceil(automationView.itemSpacing/2)
            bottomMargin: Math.ceil(automationView.itemSpacing/2)
        }

        MouseArea {
            anchors.fill: parent
            onClicked: automationView.currentIndex = -1
        }

        Flow {
            width: automationView.width
            spacing: automationView.itemSpacing

            Repeater {
                id: repeater

                EventView {
                    focus: model.index === automationView.currentIndex
                    event: model.event

                    onFocusChanged: {
                        if (focus) {
                            forceActiveFocus();
                            automationView.currentIndex = model.index;
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        anchors.fill: debugView
        anchors.margins: -2
    }

    Text {
        id: debugView
        font.bold: true
        text: automationView.currentIndex
    }
}
