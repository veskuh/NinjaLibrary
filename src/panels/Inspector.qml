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
import Kaakao 1.0
import "../components"

Rectangle {
    id: inspector
    
    property var selectedIds: []
    property int selectedId: selectedIds.length === 1 ? selectedIds[0] : -1
    property bool collapsed: false
    
    property var docData: null
    property var multiDocData: []

    property var allSelectedTags: {
        var tagsSet = {}
        for (var i = 0; i < multiDocData.length; i++) {
            var docTags = multiDocData[i].tags
            for (var j = 0; j < docTags.length; j++) {
                tagsSet[docTags[j]] = (tagsSet[docTags[j]] || 0) + 1
            }
        }
        return Object.keys(tagsSet).sort()
    }

    property var tagCounts: {
        var tagsSet = {}
        for (var i = 0; i < multiDocData.length; i++) {
            var docTags = multiDocData[i].tags
            for (var j = 0; j < docTags.length; j++) {
                tagsSet[docTags[j]] = (tagsSet[docTags[j]] || 0) + 1
            }
        }
        return tagsSet
    }

    function saveNotes() {
        if (notesArea && docData && notesArea.text !== docData.notes) {
            libraryController.updateNotes(docData.docId, notesArea.text)
        }
    }

    function updateDocData() {
        if (selectedId === -1) {
            docData = null
            if (notesArea) {
                notesArea.text = ""
            }
            return
        }
        for (var i = 0; i < documentModel.rowCount(); i++) {
            var idx = documentModel.index(i, 0)
            if (documentModel.data(idx, 257) === selectedId) {
                docData = {
                    docId: selectedId,
                    fileName: documentModel.data(idx, 259),
                    absolutePath: documentModel.data(idx, 260),
                    fileSizeStr: documentModel.data(idx, 273),
                    pageCount: documentModel.data(idx, 266),
                    starRating: documentModel.data(idx, 267),
                    isOffline: documentModel.data(idx, 268) === true || documentModel.data(idx, 268) === "true" || documentModel.data(idx, 268) === 1 || documentModel.data(idx, 268) === "1",
                    tags: documentModel.data(idx, 269) || [],
                    textSnippet: documentModel.data(idx, 270) || "",
                    notes: documentModel.data(idx, 271) || "",
                    thumbnailPath: documentModel.data(idx, 272) || "",
                    dateAdded: documentModel.data(idx, 265),
                    dateModified: documentModel.data(idx, 264),
                    lastOpened: documentModel.data(idx, 278)
                }
                if (notesArea) {
                    notesArea.text = docData.notes
                }
                return
            }
        }
        docData = null
        if (notesArea) {
            notesArea.text = ""
        }
    }

    function updateMultiDocData() {
        if (selectedIds.length <= 1) {
            multiDocData = []
            return
        }
        var temp = []
        for (var k = 0; k < selectedIds.length; k++) {
            var targetId = selectedIds[k]
            for (var i = 0; i < documentModel.rowCount(); i++) {
                var idx = documentModel.index(i, 0)
                if (documentModel.data(idx, 257) === targetId) {
                    temp.push({
                        docId: targetId,
                        fileName: documentModel.data(idx, 259),
                        tags: documentModel.data(idx, 269) || [],
                        notes: documentModel.data(idx, 271) || "",
                        starRating: documentModel.data(idx, 267) || 0
                    })
                    break
                }
            }
        }
        multiDocData = temp
    }

    function formatDate(val) {
        if (!val) return "";
        var date = new Date(val);
        if (isNaN(date.getTime())) return "";
        return Qt.formatDateTime(date, "yyyy-MM-dd hh:mm");
    }

    function formatLastOpened(timestamp) {
        if (!timestamp || timestamp === 0) return "Never";
        var date = new Date(timestamp * 1000); // timestamp in epoch seconds
        if (isNaN(date.getTime())) return "Never";
        var now = new Date();
        var diffMs = now - date;
        var diffSec = Math.floor(diffMs / 1000);
        var diffMin = Math.floor(diffSec / 60);
        var diffHr = Math.floor(diffMin / 60);
        var diffDays = Math.floor(diffHr / 24);

        if (diffSec < 60) return "Just now";
        if (diffMin < 60) return diffMin + "m ago";
        if (diffHr < 24) return diffHr + "h ago";
        if (diffDays === 1) return "Yesterday";
        if (diffDays < 7) return diffDays + " days ago";
        
        return Qt.formatDateTime(date, "yyyy-MM-dd");
    }

    onSelectedIdChanged: {
        saveNotes()
        updateDocData()
    }
    onSelectedIdsChanged: {
        updateMultiDocData()
    }
    onCollapsedChanged: {
        if (collapsed) {
            saveNotes()
        }
    }
    Component.onDestruction: {
        saveNotes()
    }
    Component.onCompleted: {
        updateDocData()
        updateMultiDocData()
    }

    Connections {
        target: documentModel
        ignoreUnknownSignals: true
        function onDataChanged(topLeft, bottomRight, roles) { updateDocData(); updateMultiDocData() }
        function onModelReset() { updateDocData(); updateMultiDocData() }
        function onRowsInserted(parent, first, last) { updateDocData(); updateMultiDocData() }
        function onRowsRemoved(parent, first, last) { updateDocData(); updateMultiDocData() }
    }

    implicitWidth: collapsed ? 0 : 260
    Behavior on implicitWidth { NumberAnimation { duration: 200 } }
    
    clip: true
    color: Theme.windowBackground
    
    // Left boundary line
    Rectangle {
        anchors.left: parent.left
        width: 1
        height: parent.height
        color: Theme.sidebarBorder
    }

    // Scrollable content
    ScrollView {
        anchors.fill: parent
        anchors.leftMargin: 1
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: parent.width - 20
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.margins: 10
            spacing: 12

            Item { height: 4 } // Spacer

            // 1. Title / Header
            KaakaoLabel {
                text: "INSPECTOR"
                role: KaakaoLabel.Role.Small
                font.weight: Font.Bold
                color: Theme.sidebarSectionText
                opacity: 1.0
                Layout.alignment: Qt.AlignLeft
            }

            // 2. Conditional states
            // STATE A: No selection
            ColumnLayout {
                Layout.fillWidth: true
                visible: inspector.selectedIds.length === 0
                spacing: 8
                
                KaakaoLabel {
                    text: "No items selected"
                    color: Theme.sidebarSectionText
                    Layout.alignment: Qt.AlignHCenter
                }
            }

            // STATE B: Multiple selection
            ColumnLayout {
                Layout.fillWidth: true
                visible: inspector.selectedIds.length > 1
                spacing: 14

                KaakaoLabel {
                    text: inspector.selectedIds.length + " Items Selected"
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    Layout.alignment: Qt.AlignHCenter
                }

                KaakaoSeparator { Layout.fillWidth: true }

                KaakaoLabel {
                    text: "Batch Set Rating"
                    role: KaakaoLabel.Role.Small
                    font.weight: Font.DemiBold
                    color: Theme.sidebarSectionText
                    opacity: 1.0
                }

                Row {
                    spacing: 4
                    Layout.alignment: Qt.AlignHCenter
                    Repeater {
                        model: 5
                        KaakaoLabel {
                            text: "★"
                            font.pixelSize: 22
                            color: hoverArea.containsMouse ? "#f1c40f" : "#bdc3c7"
                            opacity: 1.0
                            
                            MouseArea {
                                id: hoverArea
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    libraryController.batchUpdateRating(inspector.selectedIds, index + 1)
                                }
                            }
                        }
                    }
                }

                KaakaoSeparator { Layout.fillWidth: true }

                KaakaoLabel {
                    text: "Tags"
                    role: KaakaoLabel.Role.Small
                    font.weight: Font.DemiBold
                    color: Theme.sidebarSectionText
                    opacity: 1.0
                }

                // Tags Flow layout list for multiple selection
                Flow {
                    Layout.fillWidth: true
                    spacing: 4
                    Repeater {
                        model: inspector.allSelectedTags
                        TagPill {
                            text: modelData
                            isMixed: (inspector.tagCounts[modelData] || 0) < inspector.selectedIds.length
                            showDelete: true
                            onRemoveRequested: {
                                libraryController.batchRemoveTags(inspector.selectedIds, [modelData])
                            }
                        }
                    }
                }

                KaakaoTextField {
                    id: batchTagField
                    placeholderText: "Add tag to all selected..."
                    Layout.fillWidth: true
                    onAccepted: {
                        var rawText = text.trim()
                        if (rawText !== "") {
                            var parts = rawText.split(",")
                            var tagsToAdd = []
                            for (var i = 0; i < parts.length; i++) {
                                var part = parts[i].trim()
                                if (part !== "" && tagsToAdd.indexOf(part) === -1) {
                                    tagsToAdd.push(part)
                                }
                            }
                            if (tagsToAdd.length > 0) {
                                libraryController.batchAddTags(inspector.selectedIds, tagsToAdd)
                            }
                            text = ""
                        }
                    }
                }
            }

            // STATE C: Single selection
            ColumnLayout {
                Layout.fillWidth: true
                visible: inspector.selectedId !== -1 && inspector.docData !== null
                spacing: 10

                // Preview Box
                Rectangle {
                    width: 140
                    height: 140
                    color: Theme.isDarkMode ? "#121212" : "#f0f0f0"
                    radius: 4
                    border.color: Theme.buttonBorder
                    Layout.alignment: Qt.AlignHCenter

                    Image {
                        id: previewImage
                        anchors.fill: parent
                        anchors.margins: 4
                        source: (inspector.docData && inspector.docData.thumbnailPath) || ""
                        fillMode: Image.PreserveAspectFit
                        visible: source.toString() !== "" && status === Image.Ready
                    }

                    KaakaoLabel {
                        text: inspector.docData && inspector.docData.isOffline ? "OFFLINE" : "NO PREVIEW"
                        role: KaakaoLabel.Role.Small
                        font.weight: Font.Bold
                        color: inspector.docData && inspector.docData.isOffline ? Theme.colorError : Theme.sidebarSectionText
                        opacity: 1.0
                        anchors.centerIn: parent
                        visible: previewImage.source.toString() === "" || previewImage.status !== Image.Ready
                    }
                }

                // File Name
                KaakaoLabel {
                    text: (inspector.docData && inspector.docData.fileName) || ""
                    font.pixelSize: 14
                    font.weight: Font.Bold
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter

                    ToolTip.visible: filenameMouse.containsMouse && text !== ""
                    ToolTip.text: text

                    MouseArea {
                        id: filenameMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.NoButton
                    }
                }

                KaakaoLabel {
                    text: "Offline"
                    visible: !!(inspector.docData && inspector.docData.isOffline)
                    role: KaakaoLabel.Role.Small
                    color: Theme.colorError
                    opacity: 1.0
                    Layout.alignment: Qt.AlignHCenter
                }

                KaakaoDisclosureTriangle {
                    id: previewDisclosure
                    text: "Extracted Text Preview"
                    expanded: false
                    visible: inspector.docData && inspector.docData.textSnippet !== ""
                    Layout.fillWidth: true
                }

                ScrollView {
                    Layout.fillWidth: true
                    height: 100
                    clip: true
                    visible: inspector.docData && inspector.docData.textSnippet !== "" && previewDisclosure.expanded
                    
                    KaakaoTextArea {
                        text: inspector.docData ? inspector.docData.textSnippet : ""
                        readOnly: true
                        placeholderText: "No text content extracted."
                        placeholderTextColor: Theme.sidebarSectionText
                        font.pixelSize: 11
                    }
                }

                KaakaoSeparator { Layout.fillWidth: true }

                // Metadata Fields
                GridLayout {
                    columns: 2
                    rowSpacing: 4
                    columnSpacing: 8
                    Layout.fillWidth: true

                    KaakaoLabel { text: "Size:"; role: KaakaoLabel.Role.Small; color: Theme.sidebarSectionText; opacity: 1.0 }
                    KaakaoLabel { text: (inspector.docData && inspector.docData.fileSizeStr) || ""; role: KaakaoLabel.Role.Small; color: Theme.primaryText; opacity: 1.0 }

                    KaakaoLabel { text: "Pages:"; role: KaakaoLabel.Role.Small; color: Theme.sidebarSectionText; opacity: 1.0 }
                    KaakaoLabel { text: (inspector.docData && inspector.docData.pageCount !== undefined) ? inspector.docData.pageCount : ""; role: KaakaoLabel.Role.Small; color: Theme.primaryText; opacity: 1.0 }

                    KaakaoLabel { text: "Type:"; role: KaakaoLabel.Role.Small; color: Theme.sidebarSectionText; opacity: 1.0 }
                    KaakaoLabel {
                        text: {
                            if (!inspector.docData || !inspector.docData.fileName) return ""
                            var parts = inspector.docData.fileName.split('.')
                            if (parts.length <= 1) return "Unknown File"
                            var ext = parts[parts.length - 1].toLowerCase()
                            var types = {
                                "pdf": "PDF Document",
                                "doc": "Word Document",
                                "docx": "Word Document",
                                "xls": "Excel Spreadsheet",
                                "xlsx": "Excel Spreadsheet",
                                "ppt": "PowerPoint Presentation",
                                "pptx": "PowerPoint Presentation",
                                "txt": "Plain Text File",
                                "md": "Markdown Document",
                                "png": "PNG Image",
                                "jpg": "JPEG Image",
                                "jpeg": "JPEG Image",
                                "gif": "GIF Image",
                                "bmp": "BMP Image",
                                "tiff": "TIFF Image",
                                "tif": "TIFF Image",
                                "rtf": "Rich Text Format",
                                "html": "HTML Document",
                                "htm": "HTML Document",
                                "json": "JSON Document",
                                "csv": "Comma-Separated Values File",
                                "xml": "XML Document",
                                "zip": "ZIP Archive",
                                "tar": "TAR Archive",
                                "gz": "GZIP Archive"
                            }
                            return types[ext] || (ext.toUpperCase() + " File")
                        }
                        role: KaakaoLabel.Role.Small
                        color: Theme.primaryText
                        opacity: 1.0
                    }

                    KaakaoLabel { text: "Path:"; role: KaakaoLabel.Role.Small; color: Theme.sidebarSectionText; opacity: 1.0 }
                    KaakaoLabel {
                        text: (inspector.docData && inspector.docData.absolutePath) || ""
                        role: KaakaoLabel.Role.Small
                        color: Theme.primaryText
                        opacity: 1.0
                        elide: Text.ElideLeft
                        Layout.fillWidth: true

                        ToolTip.visible: pathMouse.containsMouse && text !== ""
                        ToolTip.text: text

                        MouseArea {
                            id: pathMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.NoButton
                        }
                    }

                    KaakaoLabel { text: "Modified:"; role: KaakaoLabel.Role.Small; color: Theme.sidebarSectionText; opacity: 1.0 }
                    KaakaoLabel { text: (inspector.docData && inspector.docData.dateModified) ? inspector.formatDate(inspector.docData.dateModified) : ""; role: KaakaoLabel.Role.Small; color: Theme.primaryText; opacity: 1.0 }

                    KaakaoLabel { text: "Imported:"; role: KaakaoLabel.Role.Small; color: Theme.sidebarSectionText; opacity: 1.0 }
                    KaakaoLabel { text: (inspector.docData && inspector.docData.dateAdded) ? inspector.formatDate(inspector.docData.dateAdded) : ""; role: KaakaoLabel.Role.Small; color: Theme.primaryText; opacity: 1.0 }

                    KaakaoLabel { text: "Opened:"; role: KaakaoLabel.Role.Small; color: Theme.sidebarSectionText; opacity: 1.0 }
                    KaakaoLabel { text: (inspector.docData && inspector.docData.lastOpened !== undefined) ? inspector.formatLastOpened(inspector.docData.lastOpened) : "Never"; role: KaakaoLabel.Role.Small; color: Theme.primaryText; opacity: 1.0 }
                }

                KaakaoSeparator { Layout.fillWidth: true }

                // Clickable Star Rating
                KaakaoLabel {
                    text: "Rating"
                    role: KaakaoLabel.Role.Small
                    font.weight: Font.DemiBold
                    color: Theme.sidebarSectionText
                    opacity: 1.0
                }

                Row {
                    spacing: 4
                    Layout.alignment: Qt.AlignHCenter
                    Repeater {
                        model: 5
                        KaakaoLabel {
                            text: "★"
                            font.pixelSize: 22
                            color: index < (inspector.docData ? inspector.docData.starRating : 0) ? "#f1c40f" : (Theme.isDarkMode ? "#333" : "#ccc")
                            opacity: 1.0
                            
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    libraryController.batchUpdateRating([inspector.selectedId], index + 1)
                                }
                            }
                        }
                    }
                }

                KaakaoSeparator { Layout.fillWidth: true }

                // Tags Section
                KaakaoLabel {
                    text: "Tags"
                    role: KaakaoLabel.Role.Small
                    font.weight: Font.DemiBold
                    color: Theme.sidebarSectionText
                    opacity: 1.0
                }

                // Tags Flow layout list
                Flow {
                    Layout.fillWidth: true
                    spacing: 4
                    Repeater {
                        model: inspector.docData ? inspector.docData.tags : 0
                        TagPill {
                            text: modelData
                            showDelete: true
                            onRemoveRequested: {
                                var currentTags = inspector.docData.tags.slice()
                                var idx = currentTags.indexOf(modelData)
                                if (idx >= 0) {
                                    currentTags.splice(idx, 1)
                                    libraryController.batchUpdateTags([inspector.selectedId], currentTags)
                                }
                            }
                        }
                    }
                }

                // Add tag input
                KaakaoTextField {
                    id: addTagField
                    placeholderText: "Add tag..."
                    Layout.fillWidth: true
                    onAccepted: {
                        var rawText = text.trim()
                        if (rawText !== "" && inspector.docData) {
                            var currentTags = inspector.docData.tags.slice()
                            var parts = rawText.split(",")
                            var addedAny = false
                            for (var i = 0; i < parts.length; i++) {
                                var part = parts[i].trim()
                                if (part !== "" && currentTags.indexOf(part) === -1) {
                                    currentTags.push(part)
                                    addedAny = true
                                }
                            }
                            if (addedAny) {
                                libraryController.batchUpdateTags([inspector.selectedId], currentTags)
                            }
                            text = ""
                        }
                    }
                }

                KaakaoSeparator { Layout.fillWidth: true }

                // Notes Section
                KaakaoLabel {
                    text: "Notes"
                    role: KaakaoLabel.Role.Small
                    font.weight: Font.DemiBold
                    color: Theme.sidebarSectionText
                    opacity: 1.0
                }

                ScrollView {
                    Layout.fillWidth: true
                    height: 80
                    clip: true
                    
                    KaakaoTextArea {
                        id: notesArea
                        objectName: "notesArea"
                        text: inspector.docData ? inspector.docData.notes : ""
                        placeholderText: "Write details or thoughts..."
                        placeholderTextColor: Theme.sidebarSectionText

                        // Save notes when leaving focus
                        onActiveFocusChanged: {
                            if (!activeFocus) {
                                inspector.saveNotes()
                            }
                        }
                    }
                }
            }
        }
    }
}
