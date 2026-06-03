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
import "views"
import "panels"

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

    menuBar: MenuBar {
        id: mainMenuBar

        Menu {
            title: "File"
            MenuItem {
                action: Action {
                    text: "Add Watched Folder..."
                    shortcut: StandardKey.Open
                    onTriggered: folderDialog.open()
                }
            }
            MenuSeparator {}
            MenuItem {
                action: Action {
                    text: "Quit"
                    shortcut: StandardKey.Quit
                    onTriggered: Qt.quit()
                }
            }
        }

        Menu {
            title: "Edit"
            MenuItem {
                action: Action {
                    text: "Find..."
                    shortcut: StandardKey.Find
                    onTriggered: searchField.forceActiveFocus()
                }
            }
        }

        Menu {
            title: "View"
            MenuItem {
                action: Action {
                    text: "Toggle Sidebar"
                    shortcut: "Ctrl+Alt+S"
                    onTriggered: sidebar.collapsed = !sidebar.collapsed
                }
            }
            MenuItem {
                action: Action {
                    text: "Toggle Inspector"
                    shortcut: "Ctrl+Alt+I"
                    onTriggered: inspector.collapsed = !inspector.collapsed
                }
            }
            MenuSeparator {}
            MenuItem {
                action: Action {
                    text: "Grid View Layout"
                    shortcut: "Ctrl+1"
                    onTriggered: viewSegment.currentIndex = 0
                }
            }
            MenuItem {
                action: Action {
                    text: "Table View Layout"
                    shortcut: "Ctrl+2"
                    onTriggered: viewSegment.currentIndex = 1
                }
            }
        }

        Menu {
            title: "Window"
            MenuItem {
                action: Action {
                    text: "Minimize"
                    shortcut: "Ctrl+M"
                    onTriggered: window.showMinimized()
                }
            }
        }

        Menu {
            title: "Help"
            MenuItem {
                action: Action {
                    text: "About NinjaLibrary"
                    onTriggered: aboutDialog.open()
                }
            }
            MenuItem {
                action: Action {
                    text: "Preferences..."
                    shortcut: StandardKey.Preferences
                    onTriggered: prefsDialog.open()
                }
            }
        }
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

    // Top Header ToolBar
    header: KaakaoToolBar {
        id: toolbar

        // View Mode segmented control centered exactly in the middle of the toolbar
        KaakaoSegmentedControl {
            id: viewSegment
            model: ["Grid", "Table"]
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
                placeholderText: "Search documents..."
                implicitWidth: 260
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
            visible: !sidebar.collapsed
            SplitView.minimumWidth: sidebar.collapsed ? 0 : 150
            SplitView.preferredWidth: 200
            SplitView.maximumWidth: 300

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Sidebar {
                    id: sidebar
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    onSectionSelected: (section) => {
                        proxyFilter.folderFilter = ""
                        proxyFilter.categoryFilter = section
                        canvasStack.clearSelections()
                    }
                    onFolderSelected: (path) => {
                        proxyFilter.categoryFilter = "All"
                        proxyFilter.folderFilter = path
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
                }

                StackLayout {
                    id: canvasStack
                    currentIndex: viewSegment.currentIndex
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    function clearSelections() {
                        gridCanvas.clearSelection()
                        tableCanvas.clearSelection()
                    }

                    GridCanvas {
                        id: gridCanvas
                        scaleFactor: zoomSlider.value
                        onSelectionChanged: (ids) => inspector.selectedIds = ids
                        onDoubleClicked: (path) => Qt.openUrlExternally("file://" + path)
                    }

                    TableCanvas {
                        id: tableCanvas
                        onSelectionChanged: (ids) => inspector.selectedIds = ids
                        onDoubleClicked: (path) => Qt.openUrlExternally("file://" + path)
                    }
                }

                // Pinned status bar at the bottom of the content
                KaakaoStatusBar {
                    id: statusBar
                    Layout.fillWidth: true
                    height: 24
                    leftPadding: 8
                    rightPadding: 8

                    KaakaoLabel {
                        text: "Indexed Items: " + documentModel.rowCount() + "  |  Selected: " + inspector.selectedIds.length
                        role: KaakaoLabel.Role.Small
                        color: Theme.sidebarSectionText
                        opacity: 1.0
                        Layout.alignment: Qt.AlignVCenter
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    // Scaling Zoom Slider for Grid Canvas
                    RowLayout {
                        spacing: 4
                        visible: viewSegment.currentIndex === 0
                        Layout.alignment: Qt.AlignVCenter

                        KaakaoLabel {
                            text: "⚲"
                            font.pixelSize: 12
                            color: Theme.sidebarSectionText
                            opacity: 1.0
                        }

                        KaakaoSlider {
                            id: zoomSlider
                            from: 100
                            to: 240
                            value: 150
                            implicitWidth: 100
                        }
                    }
                }
            }
        }

        // Right Collage Inspector
        Inspector {
            id: inspector
            visible: !collapsed
            selectedIds: []
            SplitView.minimumWidth: collapsed ? 0 : 220
            SplitView.preferredWidth: 260
            SplitView.maximumWidth: 350
        }
    }
}
