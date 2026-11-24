import QtQuick
import Msa.Ui.Common 1.0

SportLibraryPanel {
    required property var desktop
    objectName: "workoutsScreen"
    sport: desktop.sport
}
