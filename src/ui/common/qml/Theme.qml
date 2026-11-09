pragma Singleton
import QtQuick

QtObject {
    readonly property color background: "#15151e"
    readonly property color surface: "#20202c"
    readonly property color raised: "#292937"
    readonly property color hover: "#323243"
    readonly property color pressed: "#3b3b50"
    readonly property color border: "#48485f"
    readonly property color text: "#f3f0fa"
    readonly property color secondaryText: "#c0bbd0"
    readonly property color disabledText: "#9791a8"
    readonly property color accent: "#b7a2ff"
    readonly property color accentHover: "#c8b8ff"
    readonly property color accentPressed: "#a28be9"
    readonly property color accentText: "#20172f"
    readonly property color userBubble: "#302a43"
    readonly property color danger: "#ffb0b8"
    readonly property color dangerSurface: "#382a34"
    readonly property color success: "#b0dec3"
    readonly property color warning: "#ead09b"
    readonly property string fontFamily: Qt.application.font.family
    readonly property int fontSmall: 12
    readonly property int fontBody: 15
    readonly property int fontTitle: 20
    readonly property int fontHeading: 26
    readonly property int fontWeightStrong: Font.DemiBold
    readonly property int spaceTiny: 4
    readonly property int spaceSmall: 8
    readonly property int spaceMedium: 12
    readonly property int spaceLarge: 16
    readonly property int spaceSection: 24
    readonly property int spacePage: 32
    readonly property int radiusControl: 8
    readonly property int radiusCard: 12
    readonly property int borderWidth: 1
    readonly property int focusWidth: 2
    readonly property int controlHeight: 44
    readonly property int multilineHeight: 112
    readonly property int indicatorSize: 22
    readonly property int statusDotSize: 8
    readonly property int progressHeight: 6
    readonly property int shortMotion: 90
    readonly property int inputWidth: 260
    readonly property int cardWidth: 380
}

