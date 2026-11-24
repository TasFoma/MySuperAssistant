import QtQuick
import QtQuick.Layouts

ColumnLayout {
    id: root
    required property var field
    required property var editor
    property bool readOnly: false
    property var viewport: null
    readonly property var choices: field.choices || []
    function revealInput() {
        if (!viewport || !input.activeFocus || !viewport.contentItem || !viewport.contentItem.contentItem) return;
        var flick = viewport.contentItem;
        var position = input.mapToItem(flick.contentItem, 0, 0);
        if (position.y < flick.contentY) flick.contentY = Math.max(0, position.y - Theme.spaceSmall);
        else if (position.y + input.height > flick.contentY + flick.height)
            flick.contentY = Math.max(0, Math.min(flick.contentHeight - flick.height, position.y + input.height - flick.height + Theme.spaceSmall));
    }
    Connections {
        target: Qt.inputMethod
        function onVisibleChanged() { Qt.callLater(root.revealInput); }
    }
    Connections {
        target: root.viewport
        function onHeightChanged() { Qt.callLater(root.revealInput); }
    }
    Layout.fillWidth: true
    spacing: Theme.spaceTiny
    Text {
        Layout.fillWidth: true
        text: root.field.label
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        color: Theme.secondaryText
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSmall
    }
    SportFilterBox {
        Layout.fillWidth: true
        visible: root.choices.length > 0
        enabled: !root.readOnly
        model: root.choices
        textRole: "label"
        valueRole: "value"
        currentIndex: {
            for (var index = 0; index < root.choices.length; ++index)
                if (root.choices[index].value === root.field.value) return index;
            return -1;
        }
        Accessible.name: root.field.label
        onActivated: root.editor.setField(root.field.key, currentValue)
    }
    AppTextField {
        id: input
        visible: root.choices.length === 0
        inputMethodHints: root.field.inputHints || Qt.ImhNone
        readOnly: root.readOnly || !!root.field.readOnly
        onActiveFocusChanged: if (activeFocus) Qt.callLater(root.revealInput)
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        objectName: "sportField_" + root.field.key
        text: root.field.value
        Accessible.name: root.field.label
        onTextEdited: root.editor.setField(root.field.key, text)
    }
}
