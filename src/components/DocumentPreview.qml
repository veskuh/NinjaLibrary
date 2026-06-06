import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Kaakao 1.0

Item {
    id: root

    property string thumbnailPath: ""
    property string absolutePath: ""
    property string fileName: ""
    property bool isOffline: false
    property int fontPixelSize: 24

    readonly property bool isImageFile: {
        var ext = root.fileName.substring(root.fileName.lastIndexOf('.') + 1).toLowerCase();
        return ext === "png" || ext === "jpg" || ext === "jpeg" || ext === "gif" || ext === "bmp" || ext === "tiff";
    }

    onThumbnailPathChanged: updateImageSource()
    onAbsolutePathChanged: updateImageSource()
    onIsOfflineChanged: updateImageSource()

    function updateImageSource() {
        if (!imageLoader.item) return;
        var hasThumbnail = (root.thumbnailPath !== "" && root.thumbnailPath !== "file://");
        if (hasThumbnail) {
            imageLoader.item.source = root.thumbnailPath;
        } else if (root.isImageFile && root.absolutePath !== "" && !root.isOffline) {
            imageLoader.item.source = "file://" + root.absolutePath;
        } else {
            imageLoader.item.source = "";
        }
    }

    Loader {
        id: imageLoader
        anchors.fill: parent
        active: (root.thumbnailPath !== "" && root.thumbnailPath !== "file://") || (root.isImageFile && root.absolutePath !== "" && !root.isOffline)
        source: "ImagePreview.qml"

        onLoaded: {
            if (item) {
                root.updateImageSource()
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

    // Fallback preview
    ColumnLayout {
        anchors.centerIn: parent
        visible: imageLoader.status !== Loader.Ready || (imageLoader.item && imageLoader.item.status !== Image.Ready)
        spacing: 4

        KaakaoLabel {
            text: {
                if (root.isOffline) return "⚠️";
                var ext = root.fileName.substring(root.fileName.lastIndexOf('.') + 1).toLowerCase();
                if (ext === "pdf") return "📄";
                if (ext === "txt" || ext === "md") return "📝";
                if (ext === "png" || ext === "jpg" || ext === "jpeg" || ext === "gif" || ext === "bmp") return "🖼️";
                if (ext === "xls" || ext === "xlsx" || ext === "csv") return "📊";
                if (ext === "doc" || ext === "docx") return "📘";
                return "📄";
            }
            font.pixelSize: root.fontPixelSize + 8
            opacity: 1.0
            Layout.alignment: Qt.AlignHCenter
            visible: text !== ""
        }

        KaakaoLabel {
            text: {
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
                if (root.isOffline) return "UNAVAILABLE";
                var hasError = imageLoader.status === Loader.Error || (imageLoader.item && imageLoader.item.status === Image.Error);
                if (hasError) return "";
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
