import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Kaakao 1.0
import "../components"

Dialog {
    id: quickLook
    modal: true
    padding: Theme.paddingSmall
    width: parent ? Math.round(parent.width * 0.95) : 760
    height: parent ? Math.round(parent.height * 0.95) : 600
    
    // Custom properties
    property var docData: null
    property alias docPreview: docPreview
    
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
        spacing: 6
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
            } else if (event.key === Qt.Key_PageUp) {
                docPreview.prevPage();
                event.accepted = true;
            } else if (event.key === Qt.Key_PageDown) {
                docPreview.nextPage();
                event.accepted = true;
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
                id: docPreview
                anchors.fill: parent
                anchors.margins: 4
                fileName: quickLook.docData ? quickLook.docData.fileName : ""
                absolutePath: quickLook.docData ? quickLook.docData.absolutePath : ""
                thumbnailPath: (quickLook.docData && quickLook.docData.thumbnailPath) ? quickLook.docData.thumbnailPath : ""
                isOffline: !!(quickLook.docData && quickLook.docData.isOffline)
                isFolder: !!(quickLook.docData && quickLook.docData.isFolder)
                fontPixelSize: 48
                interactive: true
            }

            // Close button overlayed in top-right
            KaakaoControlButton {
                id: closeButton
                objectName: "closeButton"
                controlStyle: KaakaoControlButton.ControlStyle.Inline
                implicitWidth: 18
                implicitHeight: 18
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: 8
                z: 11
                onClicked: quickLook.close()
            }
        }

        // Footer Metadata & Open button
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Theme.paddingSmall
            spacing: 12
            
            KaakaoLabel {
                text: quickLook.docData ? (quickLook.docData.fileName + " • " + quickLook.docData.fileSizeStr + " • " + quickLook.docData.absolutePath) : ""
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
