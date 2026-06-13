import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Kaakao 1.0
import "../components"

Dialog {
    id: quickLook
    modal: true
    width: 640
    height: 520
    
    // Custom properties
    property var docData: null
    
    signal navigateRequested(string direction)

    onDocDataChanged: {
        if (!docData && opened) {
            quickLook.close();
        }
    }
    
    // Center in window
    x: parent ? (parent.width - width) / 2 : 100
    y: parent ? (parent.height - height) / 2 : 100

    background: Rectangle {
        color: Theme.windowBackground
        border.color: Theme.buttonBorder
        border.width: 1
        radius: Theme.radiusLarge
    }

    contentItem: ColumnLayout {
        spacing: 12
        focus: true

        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Space) {
                quickLook.close();
                event.accepted = true;
            } else if (event.key === Qt.Key_Up) {
                navigateRequested("up");
                event.accepted = true;
            } else if (event.key === Qt.Key_Down) {
                navigateRequested("down");
                event.accepted = true;
            } else if (event.key === Qt.Key_Left) {
                navigateRequested("left");
                event.accepted = true;
            } else if (event.key === Qt.Key_Right) {
                navigateRequested("right");
                event.accepted = true;
            }
        }

        // Header
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 12
            
            KaakaoLabel {
                text: quickLook.docData ? quickLook.docData.fileName : ""
                font.weight: Font.Bold
                font.pixelSize: 15
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
            
            KaakaoButton {
                text: "✕"
                implicitWidth: 28
                implicitHeight: 28
                onClicked: quickLook.close()
            }
        }

        // Preview Canvas
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.alternatingRowBackgroundOdd
            border.color: Theme.buttonBorder
            border.width: 1
            radius: Theme.radiusStandard
            clip: true

            DocumentPreview {
                anchors.fill: parent
                anchors.margins: 12
                fileName: quickLook.docData ? quickLook.docData.fileName : ""
                absolutePath: quickLook.docData ? quickLook.docData.absolutePath : ""
                thumbnailPath: (quickLook.docData && quickLook.docData.thumbnailPath) ? quickLook.docData.thumbnailPath : ""
                isOffline: !!(quickLook.docData && quickLook.docData.isOffline)
                isFolder: !!(quickLook.docData && quickLook.docData.isFolder)
                fontPixelSize: 48
            }
        }

        // Footer Metadata & Open button
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 12
            spacing: 12
            
            KaakaoLabel {
                text: quickLook.docData ? (quickLook.docData.fileSizeStr + " • " + quickLook.docData.absolutePath) : ""
                role: KaakaoLabel.Role.Small
                elide: Text.ElideMiddle
                Layout.fillWidth: true
            }
            
            KaakaoButton {
                text: "Open with Application"
                onClicked: {
                    if (quickLook.docData) {
                        libraryController.showInFinder(quickLook.docData.absolutePath)
                        quickLook.close()
                    }
                }
            }
        }
    }
}
