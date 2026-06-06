import QtQuick

Image {
    source: "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNkYAAAAAYAAjCB0C8AAAAASUVORK5CYII="
    fillMode: Image.PreserveAspectFit
    asynchronous: true
    autoTransform: true
    visible: status === Image.Ready
}
