import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Pdf
import Kaakao 1.0

Item {
    id: root

    property string thumbnailPath: ""
    property string absolutePath: ""
    property string fileName: ""
    property bool isOffline: false
    property bool isFolder: false
    property int fontPixelSize: 24
    property bool interactive: false
    property int currentPage: 0
    property alias pdfDoc: pdfDoc
    property int testPageCount: -1
    readonly property int totalPages: testPageCount >= 0 ? testPageCount : (showPdfPages ? pdfDoc.pageCount : 0)

    readonly property bool isImageFile: {
        var ext = root.fileName.substring(root.fileName.lastIndexOf('.') + 1).toLowerCase();
        return ext === "png" || ext === "jpg" || ext === "jpeg" || ext === "gif" || ext === "bmp" || ext === "tiff";
    }

    readonly property bool isPdfFile: {
        var ext = root.fileName.substring(root.fileName.lastIndexOf('.') + 1).toLowerCase();
        return ext === "pdf";
    }

    readonly property bool isTextFile: {
        var ext = root.fileName.substring(root.fileName.lastIndexOf('.') + 1).toLowerCase();
        return ext === "txt" || ext === "md";
    }

    readonly property bool showPdfPages: root.isPdfFile && root.interactive && !root.isOffline
    readonly property bool showTextPreview: root.isTextFile && root.absolutePath !== "" && !root.isOffline

    property string textContent: ""

    onShowTextPreviewChanged: updateTextContent()

    function updateTextContent() {
        if (showTextPreview) {
            textContent = libraryController.readTextFile(root.absolutePath);
        } else {
            textContent = "";
        }
    }

    onAbsolutePathChanged: {
        currentPage = 0;
        updateImageSource();
        updateTextContent();
    }

    onThumbnailPathChanged: updateImageSource()
    onIsOfflineChanged: {
        updateImageSource();
        updateTextContent();
    }

    function updateImageSource() {
        if (!imageLoader.item)
            return;
        var hasThumbnail = (root.thumbnailPath !== "" && root.thumbnailPath !== "file://");
        if (root.isImageFile && root.interactive && root.absolutePath !== "" && !root.isOffline) {
            imageLoader.item.source = "file://" + root.absolutePath;
        } else if (hasThumbnail) {
            imageLoader.item.source = root.thumbnailPath;
        } else if (root.isImageFile && root.absolutePath !== "" && !root.isOffline) {
            imageLoader.item.source = "file://" + root.absolutePath;
        } else {
            imageLoader.item.source = "";
        }
    }

    function prevPage() {
        if (currentPage > 0) {
            currentPage--;
        }
    }

    function nextPage() {
        if (showPdfPages && currentPage < totalPages - 1) {
            currentPage++;
        }
    }

    Loader {
        id: imageLoader
        anchors.fill: parent
        active: !root.showPdfPages && ((root.thumbnailPath !== "" && root.thumbnailPath !== "file://") || (root.isImageFile && root.absolutePath !== "" && !root.isOffline))
        source: "ImagePreview.qml"

        onLoaded: {
            if (item) {
                root.updateImageSource();
            }
        }
    }

    Connections {
        target: imageLoader.item ? imageLoader.item : null
        ignoreUnknownSignals: true
        function onStatusChanged() {
            if (imageLoader.item && imageLoader.item.status === Image.Error) {
                var hasThumbnail = (root.thumbnailPath !== "" && root.thumbnailPath !== "file://");
                var originalPath = "file://" + root.absolutePath;
                if (hasThumbnail && imageLoader.item.source == root.thumbnailPath && root.isImageFile && root.absolutePath !== "" && !root.isOffline) {
                    console.log("DocumentPreview: Thumbnail failed, falling back to original image: " + originalPath);
                    imageLoader.item.source = originalPath;
                }
            }
        }
    }

    // PDF Preview Components (only active if showPdfPages is true)
    PdfDocument {
        id: pdfDoc
        source: root.showPdfPages ? "file://" + root.absolutePath : ""
    }

    // White background for PDFs to support transparent PDF pages
    Rectangle {
        anchors.fill: parent
        color: "#ffffff"
        visible: root.showPdfPages && totalPages > 0
        z: -1
    }

    PdfPageImage {
        id: pdfPageImage
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: (root.showPdfPages && totalPages > 1) ? pdfControlsContainer.top : parent.bottom
        
        // Add margins to prevent overlapping with overlay controls (close button in top-right, page HUD in bottom)
        anchors.leftMargin: root.interactive ? 36 : 0
        anchors.rightMargin: root.interactive ? 36 : 0
        anchors.topMargin: root.interactive ? 36 : 0
        anchors.bottomMargin: (root.showPdfPages && totalPages > 1) ? 8 : (root.interactive ? 36 : 0)
        
        document: pdfDoc
        currentFrame: root.currentPage
        fillMode: Image.PreserveAspectFit
        visible: root.showPdfPages && totalPages > 0
    }

    Item {
        id: pdfControlsContainer
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.margins: 16
        width: pdfControlsRow.width + 12
        height: pdfControlsRow.height + 12
        visible: root.showPdfPages && totalPages > 1
        z: 10

        // Semi-transparent background for overlay look
        Rectangle {
            anchors.fill: parent
            color: Theme.isDarkMode ? Qt.rgba(0.12, 0.12, 0.12, 0.85) : Qt.rgba(0.95, 0.95, 0.95, 0.85)
            border.color: Theme.buttonBorder
            border.width: 1
            radius: Theme.radiusStandard
        }

        Row {
            id: pdfControlsRow
            anchors.centerIn: parent
            spacing: 8

            KaakaoButton {
                text: "◀"
                implicitWidth: 24
                implicitHeight: 24
                leftPadding: 0
                rightPadding: 0
                padding: 0
                enabled: root.currentPage > 0
                onClicked: root.prevPage()
            }

            KaakaoLabel {
                anchors.verticalCenter: parent.verticalCenter
                text: (root.currentPage + 1) + " / " + totalPages
                font.pixelSize: 11
                font.weight: Font.DemiBold
                color: Theme.primaryText
                horizontalAlignment: Text.AlignHCenter
            }

            KaakaoButton {
                text: "▶"
                implicitWidth: 24
                implicitHeight: 24
                leftPadding: 0
                rightPadding: 0
                padding: 0
                enabled: root.currentPage < totalPages - 1
                onClicked: root.nextPage()
            }
        }
    }

    // Scrollable Text Viewer for .txt and .md files
    ScrollView {
        id: textPreviewScroll
        anchors.fill: parent
        anchors.leftMargin: root.interactive ? 36 : 8
        anchors.rightMargin: root.interactive ? 36 : 8
        anchors.topMargin: root.interactive ? 36 : 8
        anchors.bottomMargin: root.interactive ? 36 : 8
        visible: root.showTextPreview
        clip: true

        background: Rectangle {
            color: Theme.contentBackground
            border.color: Theme.buttonBorder
            border.width: 1
            radius: Theme.radiusStandard
        }

        TextArea {
            id: textArea
            text: root.textContent
            readOnly: true
            selectByMouse: true
            wrapMode: TextEdit.Wrap
            font.family: Qt.platform.os === "osx" ? "Menlo" : "monospace"
            font.pixelSize: 12
            color: Theme.primaryText
            leftPadding: 16
            rightPadding: 16
            topPadding: 16
            bottomPadding: 16
            background: null // transparent background
        }
    }

    // Fallback preview
    ColumnLayout {
        anchors.centerIn: parent
        visible: (!root.showPdfPages || totalPages === 0) && (imageLoader.status !== Loader.Ready || (imageLoader.item && imageLoader.item.status !== Image.Ready)) && !root.showTextPreview
        spacing: 4

        KaakaoLabel {
            text: {
                if (root.isOffline)
                    return "⚠️";
                if (root.isFolder)
                    return "📁";
                var ext = root.fileName.substring(root.fileName.lastIndexOf('.') + 1).toLowerCase();
                if (ext === "pdf")
                    return "📄";
                if (ext === "txt" || ext === "md")
                    return "📝";
                if (ext === "png" || ext === "jpg" || ext === "jpeg" || ext === "gif" || ext === "bmp")
                    return "🖼️";
                if (ext === "xls" || ext === "xlsx" || ext === "csv")
                    return "📊";
                if (ext === "doc" || ext === "docx")
                    return "📘";
                return "📄";
            }
            font.pixelSize: root.fontPixelSize + 8
            opacity: 1.0
            Layout.alignment: Qt.AlignHCenter
            visible: text !== ""
        }

        KaakaoLabel {
            text: {
                if (root.isFolder)
                    return "FOLDER";
                var ext = root.fileName.substring(root.fileName.lastIndexOf('.') + 1).toUpperCase();
                return ext === "" ? "DOC" : ext;
            }
            font.pixelSize: root.fontPixelSize
            font.weight: Font.Bold
            color: Theme.sidebarSectionText
            opacity: 1.0
            Layout.alignment: Qt.AlignHCenter
            visible: !root.isOffline
        }

        KaakaoLabel {
            text: {
                if (root.isOffline)
                    return "UNAVAILABLE";
                var hasError = imageLoader.status === Loader.Error || (imageLoader.item && imageLoader.item.status === Image.Error);
                if (hasError)
                    return "";
                return (root.thumbnailPath !== "" && root.thumbnailPath !== "file://") ? "LOADING..." : "";
            }
            visible: text !== ""
            font.pixelSize: 9
            font.weight: Font.DemiBold
            color: root.isOffline ? Theme.colorError : Theme.sidebarSectionText
            opacity: 1.0
            Layout.alignment: Qt.AlignHCenter
        }
    }
}
