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
import "../components"

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
    property var folderNavigationMap: ({})

    property bool isModelEmpty: true
    property bool inspectorAnimating: false

    function updateEmptyState() {
        isModelEmpty = (proxyFilter.rowCount() === 0);
    }

    Component.onCompleted: {
        updateEmptyState();
    }

    function getRootFolderName(rootPath) {
        if (!rootPath)
            return "";
        var lastSlash = rootPath.lastIndexOf('/');
        if (lastSlash >= 0) {
            return rootPath.substring(lastSlash + 1);
        }
        return rootPath;
    }

    function getRelativeSubfolderPath(currentPath, rootPath) {
        if (!currentPath || !rootPath)
            return "";
        if (currentPath === rootPath)
            return "";
        if (currentPath.startsWith(rootPath + "/")) {
            return currentPath.substring(rootPath.length + 1);
        }
        if (currentPath.startsWith(rootPath)) {
            return currentPath.substring(rootPath.length);
        }
        return "";
    }

    function selectDocument(docId) {
        inspector.selectedIds = [docId];
        gridCanvas.selectId(docId);
        tableCanvas.selectId(docId);
    }

    function findDocIdByPath(path) {
        return documentModel.findDocIdByPath(path);
    }

    function openDocument(path) {
        Qt.openUrlExternally("file://" + path);
        var docId = findDocIdByPath(path);
        if (docId !== -1) {
            libraryController.markDocumentOpened(docId);
        }
    }

    function setFolderViewMode(mode) {
        if (mode === "hierarchical") {
            proxyFilter.includeSubfolderContents = false;
            proxyFilter.showSubfolderIcons = true;
        } else if (mode === "direct") {
            proxyFilter.includeSubfolderContents = false;
            proxyFilter.showSubfolderIcons = false;
        } else if (mode === "recursive") {
            proxyFilter.includeSubfolderContents = true;
            proxyFilter.showSubfolderIcons = false;
        }
    }

    function checkPendingSelection() {
        if (pendingSelectDocPath !== "") {
            var docId = findDocIdByPath(pendingSelectDocPath);
            if (docId !== -1) {
                selectDocument(docId);
                pendingSelectDocPath = "";
            }
        }
    }

    Connections {
        target: documentModel
        ignoreUnknownSignals: true
        function onDataChanged(topLeft, bottomRight, roles) {
            window.checkPendingSelection();
        }
        function onModelReset() {
            window.checkPendingSelection();
        }
        function onRowsInserted(parent, first, last) {
            window.checkPendingSelection();
        }
    }

    Connections {
        target: libraryController
        ignoreUnknownSignals: true
        function onFolderAdded(folderPath) {
            if (searchField.text !== "") {
                searchField.text = "";
            }
            proxyFilter.scopeFilter = "All";
        }
        function onFolderConflictDetected(message) {
            conflictDialog.message = message;
            conflictDialog.open();
        }
    }

    Connections {
        target: proxyFilter
        ignoreUnknownSignals: true
        function onFolderFilterChanged() {
            canvasStack.clearSelections();
            var selectedRoot = sidebar.getSelectedFolder();
            if (selectedRoot !== "") {
                var currentPath = proxyFilter.folderFilter;
                if (currentPath.startsWith(selectedRoot)) {
                    window.folderNavigationMap[selectedRoot] = currentPath;
                }
            }
            window.updateEmptyState();
        }
        function onModelReset() {
            window.updateEmptyState();
        }
        function onRowsInserted(parent, first, last) {
            window.updateEmptyState();
        }
        function onRowsRemoved(parent, first, last) {
            window.updateEmptyState();
        }
        function onLayoutChanged() {
            window.updateEmptyState();
        }
    }

    function syncGeometry() {
        if (window.visibility === Window.Windowed) {
            winSettings.x = window.x;
            winSettings.y = window.y;
            winSettings.width = window.width;
            winSettings.height = window.height;
        }
    }
    onXChanged: syncGeometry()
    onYChanged: syncGeometry()
    onWidthChanged: syncGeometry()
    onHeightChanged: syncGeometry()

    property alias mainMenuBar: mainMenuBar
    property alias itemContextMenu: itemContextMenu
    property alias trashDialog: trashDialog
    property alias quickLookDialog: quickLookDialog
    property alias quickSearchDialog: quickSearchDialog

    menuBar: AppMenuBar {
        id: mainMenuBar
        includeSubfolderContentsVal: proxyFilter.includeSubfolderContents
        showSubfolderIconsVal: proxyFilter.showSubfolderIcons
        onOpenFolderRequested: folderDialog.open()
        onFocusSearchRequested: searchField.forceActiveFocus()
        onToggleSidebarRequested: sidebar.collapsed = !sidebar.collapsed
        onToggleInspectorRequested: inspector.collapsed = !inspector.collapsed
        onSetViewModeRequested: index => viewSegment.currentIndex = index
        onSetFolderViewModeRequested: mode => window.setFolderViewMode(mode)
        onMinimizeRequested: window.showMinimized()
        onOpenAboutRequested: aboutDialog.open()
        onOpenPreferencesRequested: prefsDialog.open()
    }

    // Native macOS / Platform folder picker
    Labs.FolderDialog {
        id: folderDialog
        title: "Select Document Directory to Watch"
        onAccepted: {
            var urlStr = folderDialog.folder.toString();
            var path = urlStr;
            // Strip file:// prefix (on mac file:///Users/... -> /Users/...)
            if (urlStr.startsWith("file://")) {
                path = urlStr.substring(7);
                // Decode URL-encoded characters (like spaces)
                path = decodeURIComponent(path);
            }
            libraryController.addWatchedFolder(path);
        }
    }

    // Modal dialog instances
    AboutDialog {
        id: aboutDialog
    }

    PreferencesDialog {
        id: prefsDialog
    }

    QuickSearchDialog {
        id: quickSearchDialog
        objectName: "quickSearchDialog"
        onDocumentSnippetClicked: (doc, pageIndex) => {
            quickSearchDialog.wasOpenedBeforePreview = true;
            window.selectDocument(doc.docId);
            quickLookDialog.open();
            quickLookDialog.docPreview.currentPage = pageIndex;
        }
    }

    TrashConfirmDialog {
        id: trashDialog
        objectName: "trashDialog"
        parent: window.contentItem
        onAcceptedWithData: (id, path) => {
            libraryController.moveToTrash(id, path);
        }
    }

    KaakaoDialog {
        id: conflictDialog
        objectName: "conflictDialog"
        title: "Folder Conflict"
        symbol: "⚠️"
        width: 380
        standardButtons: Dialog.Ok

        x: parent ? (parent.width - width) / 2 : 100
        y: parent ? (parent.height - implicitHeight) / 2 : 100

        property string message: ""
        text: message
    }

    QuickLookDialog {
        id: quickLookDialog
        docData: inspector.docData
        onNavigateRequested: (direction) => {
            var activeCanvas = viewSegment.currentIndex === 0 ? gridCanvas : tableCanvas;
            if (direction === "up") {
                activeCanvas.moveUp();
            } else if (direction === "down") {
                activeCanvas.moveDown();
            } else if (direction === "left") {
                activeCanvas.moveLeft();
            } else if (direction === "right") {
                activeCanvas.moveRight();
            }
        }
        onClosed: {
            if (quickSearchDialog.wasOpenedBeforePreview) {
                quickSearchDialog.returningFromPreview = true;
                quickSearchDialog.open();
                quickSearchDialog.wasOpenedBeforePreview = false;
            }
        }
    }

    Shortcut {
        id: spaceShortcut
        objectName: "spaceShortcut"
        sequence: "Space"
        enabled: !searchField.activeFocus && inspector.selectedId !== -1
        onActivated: {
            if (quickLookDialog.opened) {
                quickLookDialog.close();
            } else {
                quickLookDialog.open();
            }
        }
    }

    Shortcut {
        id: quickSearchShortcut
        objectName: "quickSearchShortcut"
        sequence: Qt.platform.os === "osx" ? "Cmd+Shift+F" : "Ctrl+Shift+F"
        onActivated: {
            quickSearchDialog.open();
        }
    }

    Shortcut {
        sequence: "Escape"
        enabled: !quickLookDialog.opened
        onActivated: {
            if (searchField.text !== "") {
                searchField.text = "";
            }
            window.contentItem.forceActiveFocus();
            canvasStack.clearSelections();
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
            text: "Browse Folders Hierarchically"
            visible: sidebar.getSelectedFolder() !== ""
            checkable: true
            checked: !proxyFilter.includeSubfolderContents && proxyFilter.showSubfolderIcons
            onTriggered: window.setFolderViewMode("hierarchical")
        }
        KaakaoMenuItem {
            text: "Only Show Files at Current Level"
            visible: sidebar.getSelectedFolder() !== ""
            checkable: true
            checked: !proxyFilter.includeSubfolderContents && !proxyFilter.showSubfolderIcons
            onTriggered: window.setFolderViewMode("direct")
        }
        KaakaoMenuItem {
            text: "Include All Subfolder Contents"
            visible: sidebar.getSelectedFolder() !== ""
            checkable: true
            checked: proxyFilter.includeSubfolderContents
            onTriggered: window.setFolderViewMode("recursive")
        }
        KaakaoMenuSeparator {
            visible: sidebar.getSelectedFolder() !== ""
        }
        KaakaoMenuItem {
            text: "Open in Finder"
            visible: sidebar.getSelectedFolder() !== ""
            onTriggered: {
                var folderPath = sidebar.getSelectedFolder();
                if (folderPath !== "") {
                    Qt.openUrlExternally("file://" + folderPath);
                }
            }
        }
        KaakaoMenuItem {
            text: "Stop Watching Folder"
            visible: sidebar.getSelectedFolder() !== ""
            onTriggered: {
                var folderPath = sidebar.getSelectedFolder();
                if (folderPath !== "") {
                    libraryController.removeWatchedFolder(folderPath);
                }
            }
        }
    }

    ItemContextMenu {
        id: itemContextMenu
        objectName: "itemContextMenu"
        onMoveToTrashRequested: (docId, filePath) => {
            trashDialog.docId = docId;
            trashDialog.filePath = filePath;
            trashDialog.open();
        }
        onPreviewRequested: {
            quickLookDialog.open();
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
            Item {
                Layout.fillWidth: true
            }

            // Search box firmly on the right
            KaakaoSearchField {
                id: searchField
                objectName: "searchField"
                placeholderText: "Search documents..."
                implicitWidth: activeFocus ? 260 : 160
                Behavior on implicitWidth {
                    NumberAnimation {
                        duration: 150
                        easing.type: Easing.OutQuad
                    }
                }
                onTextChanged: {
                    if (text === "") {
                        searchDebounceTimer.stop();
                        proxyFilter.filterString = "";
                    } else {
                        searchDebounceTimer.restart();
                    }
                }

                Timer {
                    id: searchDebounceTimer
                    interval: 150
                    repeat: false
                    onTriggered: {
                        proxyFilter.filterString = searchField.text;
                    }
                }
            }

            // Quick Search Button
            EmojiToolButton {
                id: quickSearchButton
                objectName: "quickSearchButton"
                iconEmoji: "🔍"
                text: "Quick Search"
                ToolTip.text: "Quick Search"
                ToolTip.visible: hovered
                onClicked: quickSearchDialog.open()
            }

            // Toggle Right Inspector
            EmojiToolButton {
                id: inspectorButton
                iconEmoji: "ⓘ"
                text: "Inspector"
                ToolTip.text: "Toggle Inspector"
                ToolTip.visible: hovered
                onClicked: inspector.collapsed = !inspector.collapsed
            }
        }
    }

    // Core SplitView Layout
    KaakaoSplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        // Left Navigation Sidebar Column
        CollapsibleSplitPane {
            id: sidebarContainer
            collapsed: sidebar.collapsed
            minWidth: 150
            preferredWidth: 200
            maxWidth: 300
            visible: !collapsed || width > 0

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Sidebar {
                    id: sidebar
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    onSectionSelected: section => {
                        searchField.text = "";
                        canvasStack.clearSelections();
                        proxyFilter.setFilters(section, "", [], "All");
                    }
                    onFolderSelected: path => {
                        searchField.text = "";
                        canvasStack.clearSelections();
                        var cached = window.folderNavigationMap[path];
                        var targetFolder = (cached !== undefined) ? cached : path;
                        proxyFilter.setFilters("All", targetFolder, [], "All");
                    }
                    onTagSelected: tag => {
                        searchField.text = "";
                        canvasStack.clearSelections();
                        proxyFilter.setFilters("All", "", [tag], "All");
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
                        var idx = proxyFilter.activeScopes.indexOf(proxyFilter.scopeFilter);
                        return idx >= 0 ? idx : 0;
                    }
                    onFilterSelected: (index, name) => {
                        proxyFilter.scopeFilter = name;
                    }

                    Connections {
                        target: proxyFilter
                        ignoreUnknownSignals: true
                        function onScopeFilterChanged() {
                            var idx = proxyFilter.activeScopes.indexOf(proxyFilter.scopeFilter);
                            scopeBar.currentIndex = idx >= 0 ? idx : 0;
                        }
                        function onActiveScopesChanged() {
                            var idx = proxyFilter.activeScopes.indexOf(proxyFilter.scopeFilter);
                            scopeBar.currentIndex = idx >= 0 ? idx : 0;
                        }
                    }
                }

                // Clickable path breadcrumbs (only in hierarchical/direct browse views when browsing a watched folder)
                Rectangle {
                    id: pathBarContainer
                    Layout.fillWidth: true
                    height: 32
                    visible: !proxyFilter.includeSubfolderContents && sidebar.getSelectedFolder() !== ""
                    color: Theme.sidebarBackground

                    KaakaoPathControl {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        rootLabel: window.getRootFolderName(sidebar.getSelectedFolder())
                        path: window.getRelativeSubfolderPath(proxyFilter.folderFilter, sidebar.getSelectedFolder())
                        onPathClicked: targetPath => {
                            proxyFilter.folderFilter = sidebar.getSelectedFolder() + (targetPath !== "" ? "/" + targetPath : "");
                        }
                    }

                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: 1
                        color: Theme.sidebarBorder
                    }
                }

                Item {
                    id: canvasStack
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    function clearSelections() {
                        gridCanvas.clearSelection();
                        tableCanvas.clearSelection();
                    }

                    GridCanvas {
                        id: gridCanvas
                        anchors.fill: parent
                        scaleFactor: zoomSlider.value
                        onSelectionChanged: ids => inspector.selectedIds = ids
                        onDoubleClicked: path => window.openDocument(path)

                        opacity: viewSegment.currentIndex === 0 ? 1.0 : 0.0
                        visible: opacity > 0.0
                        Behavior on opacity {
                            NumberAnimation {
                                duration: 200
                                easing.type: Easing.InOutQuad
                            }
                        }
                    }

                    TableCanvas {
                        id: tableCanvas
                        anchors.fill: parent
                        onSelectionChanged: ids => inspector.selectedIds = ids
                        onDoubleClicked: path => window.openDocument(path)

                        opacity: viewSegment.currentIndex === 1 ? 1.0 : 0.0
                        visible: opacity > 0.0
                        Behavior on opacity {
                            NumberAnimation {
                                duration: 200
                                easing.type: Easing.InOutQuad
                            }
                        }
                    }

                    // Centered Placeholder View for empty states (empty folders or no search matches)
                    ColumnLayout {
                        id: placeholderView
                        objectName: "placeholderView"
                        anchors.centerIn: parent
                        spacing: 16
                        visible: window.isModelEmpty
                        z: 10

                        KaakaoLabel {
                            objectName: "placeholderEmojiLabel"
                            text: searchField.text !== "" ? "🔍" : "📁"
                            font.pixelSize: 64
                            Layout.alignment: Qt.AlignHCenter
                            opacity: 0.6
                        }

                        ColumnLayout {
                            spacing: 4
                            Layout.alignment: Qt.AlignHCenter

                            KaakaoLabel {
                                objectName: "placeholderTitleLabel"
                                text: searchField.text !== "" ? "No Search Results" : "This Folder is Empty"
                                font.weight: Font.Bold
                                font.pixelSize: 16
                                Layout.alignment: Qt.AlignHCenter
                                color: Theme.primaryText
                            }

                            KaakaoLabel {
                                objectName: "placeholderSubtitleLabel"
                                text: searchField.text !== "" 
                                    ? "No documents match '" + searchField.text + "'" 
                                    : "Drag and drop files here to add them to your library"
                                font.pixelSize: 13
                                Layout.alignment: Qt.AlignHCenter
                                color: Theme.secondaryText
                                opacity: 0.8
                            }
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
                            text: "Indexed: " + documentModel.totalCount + " items (" + documentModel.pdfCount + " PDFs, " + documentModel.imageCount + " Images, " + documentModel.textCount + " Text/Other)  |  " + documentModel.localCount + " Local, " + documentModel.unavailableCount + " Unavailable  |  Selected: " + inspector.selectedIds.length
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

                        Item {
                            Layout.fillWidth: true
                        } // Left spacer for centering progress bar inside center area

                        KaakaoLabel {
                            text: libraryController.scanStatusText
                            role: KaakaoLabel.Role.Small
                            color: Theme.sidebarSectionText
                            opacity: libraryController.isScanPaused ? 0.6 : 1.0
                            Layout.alignment: Qt.AlignVCenter

                            Behavior on opacity {
                                NumberAnimation {
                                    duration: 150
                                }
                            }
                        }

                        KaakaoProgressBar {
                            Layout.preferredWidth: 120
                            Layout.preferredHeight: 8
                            Layout.alignment: Qt.AlignVCenter
                            value: libraryController.scanProgress
                            opacity: libraryController.isScanPaused ? 0.5 : 1.0

                            Behavior on opacity {
                                NumberAnimation {
                                    duration: 150
                                }
                            }
                        }

                        KaakaoButton {
                            id: pauseResumeButton
                            Layout.alignment: Qt.AlignVCenter
                            implicitWidth: 24
                            implicitHeight: 18
                            leftPadding: 2
                            rightPadding: 2
                            font.pixelSize: 10
                            text: libraryController.isScanPaused ? "▶" : "⏸"
                            onClicked: libraryController.toggleScanPause()

                            KaakaoToolTip {
                                visible: parent.hovered
                                text: libraryController.isScanPaused ? "Resume Scanning" : "Pause Scanning"
                            }
                        }

                        Item {
                            Layout.fillWidth: true
                        } // Right spacer for centering progress bar inside center area
                    }

                    // Right Area: holds the zoom slider (conditionally visible)
                    RowLayout {
                        id: zoomSliderLayout
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 4
                        visible: viewSegment.currentIndex === 0

                        Item {
                            Layout.fillWidth: true
                        } // Spacer to push slider to the right side of this area

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

                            Behavior on opacity {
                                NumberAnimation {
                                    duration: 150
                                }
                            }
                        }
                    }
                }
            }
        }

        // Right Collage Inspector
        CollapsibleSplitPane {
            id: inspectorContainer
            // Note: inspector.collapsed is the canonical owner of the collapsed state,
            // as tests and external QML triggers directly modify inspector.collapsed.
            // Under normal circumstances, you should set inspector.collapsed rather than
            // inspectorContainer.collapsed to avoid breaking this unidirectional binding.
            collapsed: inspector.collapsed
            minWidth: 220
            preferredWidth: 260
            maxWidth: 350
            visible: !collapsed || width > 0
            onIsAnimatingChanged: window.inspectorAnimating = isAnimating

            Inspector {
                id: inspector
                anchors.fill: parent
                selectedIds: []
            }
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
        onDropped: drop => {
            if (drop.hasUrls) {
                var url = drop.urls[0].toString();
                var result = libraryController.handleDroppedUrl(url);
                if (result.status === "success") {
                    sidebar.selectFolder(result.watchedFolder);
                    if (!result.isFolder) {
                        if (result.docId !== -1) {
                            window.selectDocument(result.docId);
                        } else {
                            window.pendingSelectDocPath = result.docPath;
                        }
                    }
                }
            }
        }
    }
}
