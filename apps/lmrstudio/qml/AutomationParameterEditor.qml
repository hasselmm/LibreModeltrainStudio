import Lmrs.Core.Automation as Automation
import Lmrs.Studio as Studio

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: editor

    required property Automation.Item item
    required property Automation.parameter parameter

    readonly property bool isOptional: parameter.flags & Automation.parameter.Optional
    readonly property bool hasValue: !!(item && item[parameter.hasValueKey])
    readonly property var value: item ? item[parameter.valueKey] : undefined

    function setValue(newValue) {
        item[parameter.valueKey] = newValue;
    }

    function resetValue(newValue) {
        item[parameter.resetValueKey]();
    }

    implicitWidth: loader.implicitWidth
    implicitHeight: loader.implicitHeight

    Component { // =====================================================================================================
        id: choiceEditorTemplate

        ComboBox {
            id: choiceEditor

            // FIXME: handle optional parameters
            model: editor.parameter.model.qmlChoices

            textRole: "text"
            valueRole: "value"

            currentIndex: {
                for (let i = 0; i < model.length; ++i) {
                    if (model[i].value === editor.value)
                        return i;
                }

                return -1;
            }

            onCurrentIndexChanged: {
                // FIXME: handle optional parameters
                const newValue = model[currentIndex].value;

                if (newValue !== editor.value)
                    editor.setValue(newValue);
            }
        }
    }

    Component { // =====================================================================================================
        id: numberEditorTemplate

        SpinBox {
            id: numberEditor

            FontMetrics {
                id: numberEditorMetrics
                font: numberEditor.font
            }

            editable: true
            value: editor.hasValue ? editor.value : from

            implicitWidth: {
                const minimumValueWidth = numberEditorMetrics.advanceWidth(editor.parameter.model.minimumValue);
                const maximumValueWidth = numberEditorMetrics.advanceWidth(editor.parameter.model.maximumValue);
                const invalidValueWidth = numberEditorMetrics.advanceWidth(editor.parameter.invalidValueText);
                const contentWidth      = Math.max(minimumValueWidth, maximumValueWidth, invalidValueWidth);
                const contentPadding    = contentItem.leftPadding + contentItem.rightPadding;
                const decorationPadding = 4;

                const indicatorWidth = Studio.Style.spinBox.indicatorOrientation === Qt.Vertical
                                     ? Math.max(up.indicator.width, down.indicator.width)
                                     : up.indicator.width + down.indicator.width;

                return contentWidth + indicatorWidth + contentPadding + decorationPadding;
            }

            from: editor.parameter.model.minimumValue + (editor.isOptional ? -1 : 0)
            to: editor.parameter.model.maximumValue || 0

            textFromValue: function(value, locale) {
                if (value < editor.parameter.model.minimumValue)
                    return editor.parameter.invalidValueText;

                if (editor.parameter.flags & Automation.parameter.Hexadecimal)
                    return '0x' + Number(value).toString(16);

                return Number(value).toLocaleString(locale, 'f', 0);
            }

            valueFromText: function(text, locale) {
                if (text !== '' && text === editor.parameter.invalidValueText)
                    return editor.parameter.model.minimumValue - 1;

                if (editor.parameter.flags & Automation.parameter.Hexadecimal)
                    return parseInt(text, 16);

                return Number.fromLocaleString(locale, text);
            }

            onValueModified: {
                if (numberEditor.value < editor.parameter.model.minimumValue)
                    editor.resetValue();
                else
                    editor.setValue(numberEditor.value);
            }

            TapHandler {
                target: numberEditor.up.indicator
                onTapped: editor.forceActiveFocus() // workaround up indicator not grabbing focus
            }

            TapHandler {
                target: numberEditor.down.indicator
                onTapped: editor.forceActiveFocus() // workaround down indicator not grabbing focus
            }
        }
    }

    Component { // =====================================================================================================
        id: textEditorTemplate

        TextField {
            id: textEditor

            text: editor.value
            width: 350 // FIXME: read from model?

            onEditingFinished: editor.setValue(text)
        }
    }

    Component { // =====================================================================================================
        id: unsupportedEditorTemplate

        Text {
            text: qsTr("Parameter type \"%1\" is not supported yet").arg(editor.parameter.typeName)
        }
    }

    Loader { // ========================================================================================================
        id: loader

        sourceComponent: {
            switch (parameter?.type) {
            case Automation.parameter.Choice:
                return choiceEditorTemplate;
            case Automation.parameter.Number:
                return numberEditorTemplate;
            case Automation.parameter.Text:
                return textEditorTemplate;
            }

            return unsupportedEditorTemplate;
        }
    }
}
