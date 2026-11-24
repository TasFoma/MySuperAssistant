import QtQuick

Canvas {
    id: root
    property string name: ""
    property color ink: Theme.text
    implicitWidth: Theme.iconSize
    implicitHeight: Theme.iconSize
    onNameChanged: requestPaint()
    onInkChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onPaint: {
        const painter = getContext("2d");
        painter.reset();
        painter.scale(width / 24, height / 24);
        painter.strokeStyle = ink;
        painter.lineWidth = 1.8;
        painter.lineCap = "round";
        painter.lineJoin = "round";
        painter.beginPath();
        if (name === "today") {
            painter.moveTo(3, 10);
            painter.lineTo(12, 3);
            painter.lineTo(21, 10);
            painter.moveTo(5, 9);
            painter.lineTo(5, 21);
            painter.lineTo(19, 21);
            painter.lineTo(19, 9);
            painter.moveTo(10, 21);
            painter.lineTo(10, 14);
            painter.lineTo(14, 14);
            painter.lineTo(14, 21);
        } else if (name === "plans") {
            painter.rect(4, 5, 16, 16);
            painter.moveTo(8, 3);
            painter.lineTo(8, 7);
            painter.moveTo(16, 3);
            painter.lineTo(16, 7);
            painter.moveTo(4, 10);
            painter.lineTo(20, 10);
            painter.moveTo(8, 15);
            painter.lineTo(11, 18);
            painter.lineTo(16, 13);
        } else if (name === "workouts") {
            painter.moveTo(2, 9);
            painter.lineTo(2, 15);
            painter.moveTo(5, 6);
            painter.lineTo(5, 18);
            painter.moveTo(5, 12);
            painter.lineTo(19, 12);
            painter.moveTo(19, 6);
            painter.lineTo(19, 18);
            painter.moveTo(22, 9);
            painter.lineTo(22, 15);
        } else if (name === "music") {
            painter.moveTo(10, 17);
            painter.lineTo(10, 5);
            painter.lineTo(20, 3);
            painter.lineTo(20, 15);
            painter.moveTo(10, 9);
            painter.lineTo(20, 7);
            painter.moveTo(10, 17);
            painter.ellipse(4, 15, 6, 5);
            painter.moveTo(20, 15);
            painter.ellipse(14, 13, 6, 5);
        } else if (name === "assistant") {
            painter.moveTo(4, 4);
            painter.lineTo(20, 4);
            painter.lineTo(20, 17);
            painter.lineTo(10, 17);
            painter.lineTo(4, 21);
            painter.closePath();
            painter.moveTo(8, 9);
            painter.lineTo(16, 9);
            painter.moveTo(8, 13);
            painter.lineTo(13, 13);
        } else if (name === "more") {
            painter.moveTo(5, 12);
            painter.arc(4, 12, 1, 0, Math.PI * 2);
            painter.moveTo(13, 12);
            painter.arc(12, 12, 1, 0, Math.PI * 2);
            painter.moveTo(21, 12);
            painter.arc(20, 12, 1, 0, Math.PI * 2);
        } else if (name === "settings") {
            painter.ellipse(4, 4, 16, 16);
            painter.moveTo(16, 12);
            painter.arc(12, 12, 4, 0, Math.PI * 2);
        } else if (name === "previous" || name === "next") {
            const left = name === "previous";
            painter.moveTo(left ? 16 : 8, 5);
            painter.lineTo(left ? 7 : 17, 12);
            painter.lineTo(left ? 16 : 8, 19);
            painter.moveTo(left ? 4 : 20, 5);
            painter.lineTo(left ? 4 : 20, 19);
        } else if (name === "pause") {
            painter.moveTo(8, 5);
            painter.lineTo(8, 19);
            painter.moveTo(16, 5);
            painter.lineTo(16, 19);
        } else if (name === "play") {
            painter.moveTo(7, 4);
            painter.lineTo(20, 12);
            painter.lineTo(7, 20);
            painter.closePath();
        }
        painter.stroke();
    }
}
