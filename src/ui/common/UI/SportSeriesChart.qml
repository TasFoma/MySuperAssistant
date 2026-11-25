import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts

SurfaceCard {
    id: root
    required property var series
    required property var journal
    property bool loadMetric: false
    implicitHeight: chartColumn.implicitHeight + topPadding + bottomPadding
    ColumnLayout {
        id: chartColumn
        anchors.fill: parent
        spacing: Theme.spaceSmall
        SectionHeader {
            Layout.fillWidth: true
            title: root.loadMetric && root.series ? root.series.loadLabel : "Повторения"
            description: "Каждая точка — выполненный подход. Нажмите, чтобы открыть занятие."
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spaceSmall
            ColumnLayout {
                Layout.preferredWidth: Theme.sportChartAxisWidth
                Layout.preferredHeight: Theme.sportChartHeight
                Controls.Label {
                    text: root.series ? String(root.loadMetric ? root.series.maximumLoad : root.series.maximumRepetitions) : ""
                    color: Theme.secondaryText
                    font.pixelSize: Theme.fontSmall
                }
                Item { Layout.fillHeight: true }
                Controls.Label {
                    text: root.series ? String(root.loadMetric ? root.series.minimumLoad : root.series.minimumRepetitions) : ""
                    color: Theme.secondaryText
                    font.pixelSize: Theme.fontSmall
                }
            }
            Controls.ScrollView {
                id: chartScroll
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.sportChartHeight
                clip: true
                contentHeight: availableHeight
                contentWidth: Math.max(availableWidth, root.series ? root.series.points.length * Theme.controlHeight : 0)
                Item {
                    id: plot
                    width: chartScroll.contentWidth
                    height: Theme.sportChartHeight
                    Rectangle { width: parent.width; height: Theme.borderWidth; color: Theme.border; anchors.bottom: parent.bottom }
                    Repeater {
                        model: root.series ? root.series.points : []
                        Controls.Button {
                            id: point
                            required property var modelData
                            required property int index
                            objectName: "sportProgressPoint_" + index
                            visible: !root.loadMetric || modelData.hasLoad
                            width: Theme.controlHeight
                            height: Theme.controlHeight
                            x: (index + 0.5) * plot.width / root.series.points.length - width / 2
                            y: Theme.spaceSmall + (1 - (root.loadMetric ? modelData.loadPosition : modelData.repetitionPosition)) * (plot.height - height - 2 * Theme.spaceSmall)
                            Accessible.name: modelData.text + ". Открыть исходное занятие"
                            Controls.ToolTip.visible: root.visible && (hovered || activeFocus)
                            Controls.ToolTip.text: modelData.text
                            background: Rectangle {
                                anchors.centerIn: parent
                                width: Theme.sportChartPointSize
                                height: width
                                radius: width / 2
                                color: root.loadMetric ? Theme.progressAccent : Theme.accent
                                border.width: point.activeFocus ? Theme.focusWidth : 0
                                border.color: Theme.focusBorder
                            }
                            contentItem: Item {}
                            onClicked: root.journal.openWorkout(modelData.workoutId)
                        }
                    }
                }
            }
        }
        Controls.Label {
            Layout.fillWidth: true
            text: root.series && root.series.points.length ? root.series.points[0].date + " → " + root.series.points[root.series.points.length - 1].date + " · подходы по датам" : ""
            wrapMode: Text.Wrap
            color: Theme.secondaryText
            font.pixelSize: Theme.fontSmall
        }
    }
}
