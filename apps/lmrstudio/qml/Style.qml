pragma Singleton

import QtQuick
import QtQuick.Controls

QtObject {
    readonly property QtObject spinBox: SpinBox {
        readonly property int indicatorOrientation: up.indicator.x === down.indicator.x ? Qt.Vertical : Qt.Horizontal
    }
}
