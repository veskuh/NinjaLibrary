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
import Kaakao 1.0
import "views"
import "panels"

KaakaoWindow {
    id: window
    title: "NinjaLibrary - Local-First Document Gallery"
    
    width: 1024
    height: 700
    minimumWidth: 800
    minimumHeight: 500
    
    visible: true

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

    // Top Header ToolBar
    header: KaakaoToolBar {
        id: toolbar
        
        RowLayout {
            anchors.fill: parent
            anchors.topMargin: Theme.paddingSmall / 2
            anchors.bottomMargin: Theme.paddingSmall / 2
            anchors.leftMargin: Theme.paddingSmall
            anchors.rightMargin: Theme.paddingSmall
            spacing: Theme.paddingSmall

            // View Mode segmented control
            KaakaoSegmentedControl {
                id: viewSegment
                model: ["Grid", "Table"]
                implicitWidth: 120
            }

            // Watch Folder button
            KaakaoToolButton {
                id: addFolderButton
                iconEmoji: "➕"
                text: "Add Folder"
                padding: 0
                topPadding: 0
                bottomPadding: 0
                onClicked: folderDialog.open()

                contentItem: Column {
                    spacing: 2
                    opacity: addFolderButton.enabled ? 1.0 : 0.4
                    
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: addFolderButton.iconEmoji
                        font.pixelSize: 20
                        renderType: Text.NativeRendering
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: addFolderButton.text
                        font: addFolderButton.font
                        color: Theme.primaryText
                        renderType: Text.NativeRendering
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }

            Item { Layout.fillWidth: true }

            // Search box
            KaakaoSearchField {
                id: searchField
                placeholderText: "Search documents..."
                implicitWidth: 260
                Layout.alignment: Qt.AlignHCenter
                onTextChanged: {
                    proxyFilter.filterString = text
                }
            }

            Item { Layout.fillWidth: true }

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

        // Left Navigation Sidebar
        Sidebar {
            id: sidebar
            visible: !collapsed
            SplitView.minimumWidth: collapsed ? 0 : 150
            SplitView.preferredWidth: 200
            SplitView.maximumWidth: 300

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

        // Center Content Stack
        StackLayout {
            id: canvasStack
            currentIndex: viewSegment.currentIndex
            SplitView.fillWidth: true

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

    // Bottom Status Indicator Frame
    footer: KaakaoStatusBar {
        id: statusBar
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
            visible: viewSegment.currentIndex === 0 // Grid only
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
