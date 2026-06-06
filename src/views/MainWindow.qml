/*
 * Copyright (c) 2026, NinjaLibrary
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Qt.labs.platform 1.1 as Labs
import QtCore
import Kaakao 1.0
import "../panels"

KaakaoWindow {
    id: window
    title: "NinjaLibrary - Local-First Document Gallery"
    
    x: winSettings.x
    y: winSettings.y
    width: winSettings.width
    height: winSettings.height
    minimumWidth: 800
    minimumHeight: 500
    
    visible: true

    Settings {
        id: winSettings
        category: "MainWindow"
        property int x: 100
        property int y: 100
        property int width: 1024
        property int height: 700
    }

    property string pendingSelectDocPath: ""

    function selectDocument(docId) {
        inspector.selectedIds = [docId]
        gridCanvas.selectId(docId)
        tableCanvas.selectId(docId)
    }

    function findDocIdByPath(path) {
        for (var i = 0; i < documentModel.rowCount(); i++) {
            var idx = documentModel.index(i, 0)
            var absPath = documentModel.data(idx, 260) // AbsolutePathRole
            if (absPath === path) {
                return documentModel.data(idx, 257) // IdRole
            }
        }
        return -1
    }

    function checkPendingSelection() {
        if (pendingSelectDocPath !== "") {
            var docId = findDocIdByPath(pendingSelectDocPath)
            if (docId !== -1) {
                selectDocument(docId)
                pendingSelectDocPath = ""
            }
        }
    }

    Connections {
        target: documentModel
        ignoreUnknownSignals: true
        function onDataChanged(topLeft, bottomRight, roles) { window.checkPendingSelection() }
        function onModelReset() { window.checkPendingSelection() }
        function onRowsInserted(parent, first, last) { window.checkPendingSelection() }
    }

    Connections {
        target: libraryController
        ignoreUnknownSignals: true
        function onFolderAdded(folderPath) {
            if (searchField.text !== "") {
                searchField.text = ""
            }
            proxyFilter.scopeFilter = "All"
        }
    }

    onXChanged: {
        if (window.visibility === Window.Windowed) {
            winSettings.x = window.x
        }
    }
    onYChanged: {
        if (window.visibility === Window.Windowed) {
            winSettings.y = window.y
        }
    }
    onWidthChanged: {
        if (window.visibility === Window.Windowed) {
            winSettings.width = window.width
        }
    }
    onHeightChanged: {
        if (window.visibility === Window.Windowed) {
            winSettings.height = window.height
        }
    }

    property alias mainMenuBar: mainMenuBar
    property alias itemContextMenu: itemContextMenu
    property alias trashDialog: trashDialog

    menuBar: AppMenuBar {
        id: mainMenuBar
        onOpenFolderRequested: folderDialog.open()
        onFocusSearchRequested: searchField.forceActiveFocus()
        onToggleSidebarRequested: sidebar.collapsed = !sidebar.collapsed
        onToggleInspectorRequested: inspector.collapsed = !inspector.collapsed
        onSetViewModeRequested: (index) => viewSegment.currentIndex = index
        onMinimizeRequested: window.showMinimized()
        onOpenAboutRequested: aboutDialog.open()
        onOpenPreferencesRequested: prefsDialog.open()
    }

    // Native macOS / Platform folder picker
    Labs.FolderDialog {
        id: folderDialog
        title: "Select Document Directory to Watch"
        onAccepted: {
            var urlStr = folderDialog.folder.toString()
            var path = urlStr
            // Strip file:// prefix (on mac file:///Users/... -> /Users/...)
            if (urlStr.startsWith("file://")) {
                path = urlStr.substring(7)
                // Decode URL-encoded characters (like spaces)
                path = decodeURIComponent(path)
            }
            libraryController.addWatchedFolder(path)
        }
    }

    // Modal dialog instances
    AboutDialog {
        id: aboutDialog
    }

    PreferencesDialog {
        id: prefsDialog
    }

    KaakaoDialog {
        id: trashDialog
        objectName: "trashDialog"
        title: "Move to Trash"
        symbol: "🗑️"
        width: 380
        standardButtons: Dialog.Yes | Dialog.No
        
        x: parent ? (parent.width - width) / 2 : 100
        y: parent ? (parent.height - implicitHeight) / 2 : 100
        
        property int docId: -1
        property string filePath: ""
        
        text: "Are you sure you want to move '" + (filePath ? filePath.substring(filePath.lastIndexOf('/') + 1) : "") + "' to the Trash?"
        
        onAccepted: {
            libraryController.moveToTrash(docId, filePath)
        }
    }

    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (searchField.text !== "") {
                searchField.text = ""
            }
            window.contentItem.forceActiveFocus()
            canvasStack.clearSelections()
        }
    }

    KaakaoMenu {
        id: gearMenu
        
        KaakaoMenuItem {
            text: "Add Watched Folder..."
            onTriggered: folderDialog.open()
        }
        KaakaoMenuItem {
            text: "Refresh Library"
            onTriggered: documentModel.refresh()
        }
        KaakaoMenuSeparator {
            visible: sidebar.getSelectedFolder() !== ""
        }
        KaakaoMenuItem {
            text: "Open in Finder"
            visible: sidebar.getSelectedFolder() !== ""
            onTriggered: {
                var folderPath = sidebar.getSelectedFolder()
                if (folderPath !== "") {
                    Qt.openUrlExternally("file://" + folderPath)
                }
            }
        }
        KaakaoMenuItem {
            text: "Stop Watching Folder"
            visible: sidebar.getSelectedFolder() !== ""
            onTriggered: {
                var folderPath = sidebar.getSelectedFolder()
                if (folderPath !== "") {
                    libraryController.removeWatchedFolder(folderPath)
                }
            }
        }
    }

    ItemContextMenu {
        id: itemContextMenu
        objectName: "itemContextMenu"
        onMoveToTrashRequested: (docId, filePath) => {
            trashDialog.docId = docId
            trashDialog.filePath = filePath
            trashDialog.open()
        }
    }

    // Top Header ToolBar
    header: KaakaoToolBar {
        id: toolbar

        // View Mode segmented control centered exactly in the middle of the toolbar
        KaakaoSegmentedControl {
            id: viewSegment
            objectName: "viewSegment"
            model: ["Grid", "Table"]
            currentIndex: 1 // Table view by default
            implicitWidth: 120
            anchors.centerIn: parent
            z: 1 // Ensure it sits on top of layout if needed
        }
        
        RowLayout {
            anchors.fill: parent
            anchors.topMargin: Theme.paddingSmall / 2
            anchors.bottomMargin: Theme.paddingSmall / 2
            anchors.leftMargin: Theme.paddingSmall
            anchors.rightMargin: Theme.paddingSmall
            spacing: Theme.paddingSmall

            // Spacer to push the search and inspector controls to the far right
            Item { Layout.fillWidth: true }

            // Search box firmly on the right
            KaakaoSearchField {
                id: searchField
                objectName: "searchField"
                placeholderText: "Search documents..."
                implicitWidth: activeFocus ? 260 : 160
                Behavior on implicitWidth {
                    NumberAnimation { duration: 150; easing.type: Easing.OutQuad }
                }
                onTextChanged: {
                    proxyFilter.filterString = text
                }
            }

            // Toggle Right Inspector
            KaakaoToolButton {
                id: inspectorButton
                iconEmoji: "ⓘ"
                text: "Inspector"
                padding: 0
                topPadding: 0
                bottomPadding: 0
                ToolTip.text: "Toggle Inspector"
                ToolTip.visible: hovered
                onClicked: inspector.collapsed = !inspector.collapsed

                contentItem: Column {
                    spacing: 2
                    opacity: inspectorButton.enabled ? 1.0 : 0.4
                    
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: inspectorButton.iconEmoji
                        font.pixelSize: 20
                        renderType: Text.NativeRendering
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: inspectorButton.text
                        font: inspectorButton.font
                        color: Theme.primaryText
                        renderType: Text.NativeRendering
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }
        }
    }

    // Core SplitView Layout
    KaakaoSplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        // Left Navigation Sidebar Column
        Item {
            id: sidebarContainer
            visible: !sidebar.collapsed || width > 0
            
            states: [
                State {
                    name: "collapsed"
                    when: sidebar.collapsed
                    PropertyChanges { target: sidebarContainer; SplitView.minimumWidth: 0; SplitView.preferredWidth: 0; SplitView.maximumWidth: 0 }
                },
                State {
                    name: "expanded"
                    when: !sidebar.collapsed
                    PropertyChanges { target: sidebarContainer; SplitView.minimumWidth: 150; SplitView.preferredWidth: 200; SplitView.maximumWidth: 300 }
                }
            ]
            transitions: [
                Transition {
                    from: "expanded"; to: "collapsed"
                    NumberAnimation { properties: "SplitView.preferredWidth,SplitView.minimumWidth,SplitView.maximumWidth"; duration: 200; easing.type: Easing.InOutQuad }
                },
                Transition {
                    from: "collapsed"; to: "expanded"
                    NumberAnimation { properties: "SplitView.preferredWidth,SplitView.minimumWidth,SplitView.maximumWidth"; duration: 200; easing.type: Easing.InOutQuad }
                }
            ]

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Sidebar {
                    id: sidebar
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    onSectionSelected: (section) => {
                        proxyFilter.folderFilter = ""
                        proxyFilter.selectedTags = []
                        proxyFilter.categoryFilter = section
                        proxyFilter.scopeFilter = "All"
                        searchField.text = ""
                        canvasStack.clearSelections()
                    }
                    onFolderSelected: (path) => {
                        proxyFilter.categoryFilter = "All"
                        proxyFilter.selectedTags = []
                        proxyFilter.folderFilter = path
                        proxyFilter.scopeFilter = "All"
                        searchField.text = ""
                        canvasStack.clearSelections()
                    }
                    onTagSelected: (tag) => {
                        proxyFilter.categoryFilter = "All"
                        proxyFilter.folderFilter = ""
                        proxyFilter.selectedTags = [tag]
                        proxyFilter.scopeFilter = "All"
                        searchField.text = ""
                        canvasStack.clearSelections()
                    }
                }

                // Sidebar Footer Bar
                Rectangle {
                    id: sidebarFooter
                    Layout.fillWidth: true
                    implicitHeight: 28
                    color: Theme.sidebarBackground

                    Rectangle {
                        anchors.top: parent.top
                        width: parent.width
                        height: 1
                        color: Theme.sidebarBorder
                    }
                    Rectangle {
                        anchors.right: parent.right
                        width: 1
                        height: parent.height
                        color: Theme.sidebarBorder
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 6
                        anchors.rightMargin: 6
                        spacing: 4

                        KaakaoToolButton {
                            id: footerAddButton
                            text: "Add Folder"
                            iconEmoji: "➕"
                            implicitWidth: 22
                            implicitHeight: 22
                            Layout.alignment: Qt.AlignVCenter
                            padding: 0
                            topPadding: 0
                            bottomPadding: 0

                            contentItem: Text {
                                text: footerAddButton.iconEmoji
                                font.pixelSize: 14
                                color: Theme.primaryText
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            onClicked: folderDialog.open()

                            ToolTip.text: "Add Watched Folder"
                            ToolTip.visible: hovered
                        }

                        KaakaoToolButton {
                            id: footerGearButton
                            text: "Actions"
                            iconEmoji: "⚙️"
                            implicitWidth: 22
                            implicitHeight: 22
                            Layout.alignment: Qt.AlignVCenter
                            padding: 0
                            topPadding: 0
                            bottomPadding: 0
                            enabled: sidebar.getSelectedFolder() !== ""

                            contentItem: Text {
                                text: footerGearButton.iconEmoji
                                font.pixelSize: 12
                                color: Theme.primaryText
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                opacity: footerGearButton.enabled ? 1.0 : 0.4
                            }

                            onClicked: gearMenu.popup(footerGearButton.mapToItem(null, 0, footerGearButton.height))

                            ToolTip.text: "Folder / Library Actions"
                            ToolTip.visible: hovered
                        }

                        Item {
                            Layout.fillWidth: true
                        }
                    }
                }
            }
        }

        // Center Content Column (Canvas Stack + Main Status Bar)
        Item {
            SplitView.fillWidth: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                KaakaoScopeBar {
                    id: scopeBar
                    Layout.fillWidth: true
                    label: "Scope:"
                    model: proxyFilter.activeScopes
                    currentIndex: {
                        var idx = proxyFilter.activeScopes.indexOf(proxyFilter.scopeFilter)
                        return idx >= 0 ? idx : 0
                    }
                    onFilterSelected: (index, name) => {
                        proxyFilter.scopeFilter = name
                    }

                    Connections {
                        target: proxyFilter
                        ignoreUnknownSignals: true
                        function onScopeFilterChanged() {
                            var idx = proxyFilter.activeScopes.indexOf(proxyFilter.scopeFilter)
                            scopeBar.currentIndex = idx >= 0 ? idx : 0
                        }
                        function onActiveScopesChanged() {
                            var idx = proxyFilter.activeScopes.indexOf(proxyFilter.scopeFilter)
                            scopeBar.currentIndex = idx >= 0 ? idx : 0
                        }
                    }
                }

                Item {
                    id: canvasStack
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    function clearSelections() {
                        gridCanvas.clearSelection()
                        tableCanvas.clearSelection()
                    }

                    GridCanvas {
                        id: gridCanvas
                        anchors.fill: parent
                        scaleFactor: zoomSlider.value
                        onSelectionChanged: (ids) => inspector.selectedIds = ids
                        onDoubleClicked: (path) => {
                            Qt.openUrlExternally("file://" + path)
                            var docId = window.findDocIdByPath(path)
                            if (docId !== -1) {
                                libraryController.markDocumentOpened(docId)
                            }
                        }

                        opacity: viewSegment.currentIndex === 0 ? 1.0 : 0.0
                        visible: opacity > 0.0
                        Behavior on opacity {
                            NumberAnimation { duration: 200; easing.type: Easing.InOutQuad }
                        }
                    }

                    TableCanvas {
                        id: tableCanvas
                        anchors.fill: parent
                        onSelectionChanged: (ids) => inspector.selectedIds = ids
                        onDoubleClicked: (path) => {
                            Qt.openUrlExternally("file://" + path)
                            var docId = window.findDocIdByPath(path)
                            if (docId !== -1) {
                                libraryController.markDocumentOpened(docId)
                            }
                        }

                        opacity: viewSegment.currentIndex === 1 ? 1.0 : 0.0
                        visible: opacity > 0.0
                        Behavior on opacity {
                            NumberAnimation { duration: 200; easing.type: Easing.InOutQuad }
                        }
                    }
                }

                KaakaoStatusBar {
                    id: statusBar
                    objectName: "statusBar"
                    Layout.fillWidth: true
                    height: 24
                    leftPadding: 8
                    rightPadding: 8

                    // Left Area: holds status label (visible when not scanning)
                    Item {
                        id: leftArea
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        Layout.alignment: Qt.AlignVCenter
                        implicitHeight: 20

                        KaakaoLabel {
                            id: statusLabel
                            objectName: "statusLabel"
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            visible: !libraryController.isScanning
                            text: "Indexed: " + documentModel.totalCount + " items (" +
                                   documentModel.pdfCount + " PDFs, " +
                                   documentModel.imageCount + " Images, " +
                                   documentModel.textCount + " Text/Other)  |  " +
                                   documentModel.localCount + " Local, " +
                                   documentModel.unavailableCount + " Unavailable  |  Selected: " +
                                   inspector.selectedIds.length
                            role: KaakaoLabel.Role.Small
                            color: Theme.sidebarSectionText
                            opacity: 1.0
                        }
                    }

                    // Center Area: holds progress bar layout (visible when scanning)
                    RowLayout {
                        id: scanningProgressLayout
                        objectName: "scanningProgressLayout"
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 8
                        visible: libraryController.isScanning

                        Item { Layout.fillWidth: true } // Left spacer for centering progress bar inside center area

                        KaakaoLabel {
                            text: libraryController.scanStatusText
                            role: KaakaoLabel.Role.Small
                            color: Theme.sidebarSectionText
                            opacity: 1.0
                            Layout.alignment: Qt.AlignVCenter
                        }

                        KaakaoProgressBar {
                            Layout.preferredWidth: 120
                            Layout.preferredHeight: 8
                            Layout.alignment: Qt.AlignVCenter
                            value: libraryController.scanProgress
                        }

                        Item { Layout.fillWidth: true } // Right spacer for centering progress bar inside center area
                    }

                    // Right Area: holds the zoom slider (conditionally visible)
                    RowLayout {
                        id: zoomSliderLayout
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 4
                        visible: viewSegment.currentIndex === 0

                        Item { Layout.fillWidth: true } // Spacer to push slider to the right side of this area

                        KaakaoLabel {
                            text: "⚲"
                            font.pixelSize: 12
                            color: Theme.sidebarSectionText
                            opacity: 1.0
                            Layout.alignment: Qt.AlignVCenter
                        }

                        KaakaoSlider {
                            id: zoomSlider
                            from: 100
                            to: 240
                            value: 150
                            implicitWidth: 100
                            Layout.alignment: Qt.AlignVCenter
                        }

                        KaakaoLabel {
                            text: Math.round(zoomSlider.value) + "%"
                            font.pixelSize: 13
                            font.weight: Font.Bold
                            color: Theme.primaryText
                            opacity: (zoomSlider.hovered || zoomSlider.pressed) ? 1.0 : 0.6
                            Layout.alignment: Qt.AlignVCenter
                            Layout.preferredWidth: 48
                            horizontalAlignment: Text.AlignRight
                            
                            Behavior on opacity { NumberAnimation { duration: 150 } }
                        }
                    }
                }
            }
        }

        // Right Collage Inspector
        Inspector {
            id: inspector
            visible: !collapsed || width > 0
            selectedIds: []
            
            states: [
                State {
                    name: "collapsed"
                    when: inspector.collapsed
                    PropertyChanges { target: inspector; SplitView.minimumWidth: 0; SplitView.preferredWidth: 0; SplitView.maximumWidth: 0 }
                },
                State {
                    name: "expanded"
                    when: !inspector.collapsed
                    PropertyChanges { target: inspector; SplitView.minimumWidth: 220; SplitView.preferredWidth: 260; SplitView.maximumWidth: 350 }
                }
            ]
            transitions: [
                Transition {
                    from: "expanded"; to: "collapsed"
                    NumberAnimation { properties: "SplitView.preferredWidth,SplitView.minimumWidth,SplitView.maximumWidth"; duration: 200; easing.type: Easing.InOutQuad }
                },
                Transition {
                    from: "collapsed"; to: "expanded"
                    NumberAnimation { properties: "SplitView.preferredWidth,SplitView.minimumWidth,SplitView.maximumWidth"; duration: 200; easing.type: Easing.InOutQuad }
                }
            ]
        }
    }

    // Visual Drag & Drop Overlay
    DragOverlay {
        id: dragOverlay
        containsDrag: dropArea.containsDrag
    }

    DropArea {
        id: dropArea
        objectName: "dropArea"
        anchors.fill: parent
        keys: ["text/uri-list"]
        onDropped: (drop) => {
            if (drop.hasUrls) {
                var url = drop.urls[0].toString()
                var result = libraryController.handleDroppedUrl(url)
                if (result.status === "success") {
                    sidebar.selectFolder(result.watchedFolder)
                    if (!result.isFolder) {
                        if (result.docId !== -1) {
                            window.selectDocument(result.docId)
                        } else {
                            window.pendingSelectDocPath = result.docPath
                        }
                    }
                }
            }
        }
    }
}
