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

    property bool showSavedIndicator: false

    Timer {
        id: savedTimer
        interval: 1000
        onTriggered: inspector.showSavedIndicator = false
    }

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
            inspector.showSavedIndicator = true
            savedTimer.restart()
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
                    lastOpened: documentModel.data(idx, 278),
                    isFolder: documentModel.data(idx, 279) === true || documentModel.data(idx, 279) === "true" || documentModel.data(idx, 279) === 1 || documentModel.data(idx, 279) === "1",
                    itemCount: documentModel.data(idx, 280) || 0,
                    itemCountStr: documentModel.data(idx, 281) || ""
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
                    id: batchRatingRow
                    spacing: 4
                    Layout.alignment: Qt.AlignHCenter

                    property int hoveredIndex: -1
                    property int localRating: 0

                    Connections {
                        target: inspector
                        ignoreUnknownSignals: true
                        function onSelectedIdsChanged() {
                            batchRatingRow.localRating = 0
                        }
                    }

                    Repeater {
                        model: 5
                        KaakaoLabel {
                            text: "★"
                            font.pixelSize: 22
                            color: {
                                var active = false
                                if (batchRatingRow.hoveredIndex !== -1) {
                                    active = index <= batchRatingRow.hoveredIndex
                                } else {
                                    active = index < batchRatingRow.localRating
                                }
                                return active ? "#f1c40f" : (Theme.isDarkMode ? "#333" : "#ccc")
                            }
                            opacity: 1.0
                            scale: batchStarMouseArea.containsMouse ? 1.2 : 1.0

                            Behavior on scale {
                                NumberAnimation { duration: 100 }
                            }
                            Behavior on color {
                                ColorAnimation { duration: 100 }
                            }

                            MouseArea {
                                id: batchStarMouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                onEntered: batchRatingRow.hoveredIndex = index
                                onExited: batchRatingRow.hoveredIndex = -1
                                onClicked: {
                                    var newRating = index + 1
                                    if (batchRatingRow.localRating === newRating) {
                                        newRating = 0
                                    }
                                    batchRatingRow.localRating = newRating
                                    libraryController.batchUpdateRating(inspector.selectedIds, newRating)
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
                
                Rectangle {
                    width: 140
                    height: 140
                    color: (inspector.docData && inspector.docData.isFolder) ? "transparent" : (Theme.isDarkMode ? "#121212" : "#f0f0f0")
                    radius: 4
                    border.color: (inspector.docData && inspector.docData.isFolder) ? "transparent" : Theme.buttonBorder
                    Layout.alignment: Qt.AlignHCenter

                    DocumentPreview {
                        anchors.fill: parent
                        anchors.margins: 4
                        thumbnailPath: (inspector.docData && inspector.docData.thumbnailPath) || ""
                        absolutePath: (inspector.docData && inspector.docData.absolutePath) || ""
                        fileName: (inspector.docData && inspector.docData.fileName) || ""
                        isOffline: !!(inspector.docData && inspector.docData.isOffline)
                        fontPixelSize: 32
                        visible: !(inspector.docData && inspector.docData.isFolder)
                    }

                    // Folder representation
                    Rectangle {
                        anchors.fill: parent
                        color: "transparent"
                        visible: !!(inspector.docData && inspector.docData.isFolder)

                        KaakaoLabel {
                            anchors.centerIn: parent
                            text: "📁"
                            font.pixelSize: 64
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
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
                    text: "Unavailable"
                    visible: !!(inspector.docData && inspector.docData.isOffline) && !(inspector.docData && inspector.docData.isFolder)
                    role: KaakaoLabel.Role.Small
                    color: Theme.colorError
                    opacity: 1.0
                    Layout.alignment: Qt.AlignHCenter
                }

                KaakaoDisclosureTriangle {
                    id: previewDisclosure
                    text: "Extracted Text Preview"
                    expanded: false
                    visible: inspector.docData && inspector.docData.textSnippet !== "" && !inspector.docData.isFolder
                    Layout.fillWidth: true
                }

                Item {
                    id: previewContainer
                    Layout.fillWidth: true
                    Layout.preferredHeight: (previewDisclosure.expanded && inspector.docData && inspector.docData.textSnippet !== "" && !inspector.docData.isFolder) ? 110 : 0
                    clip: true
                    visible: (Layout.preferredHeight > 0) || (previewDisclosure.expanded && inspector.docData && inspector.docData.textSnippet !== "" && !inspector.docData.isFolder)

                    Behavior on Layout.preferredHeight {
                        NumberAnimation {
                            duration: 250
                            easing.type: Easing.InOutQuad
                        }
                    }

                    Rectangle {
                        anchors.fill: parent
                        anchors.bottomMargin: 8
                        color: Theme.isDarkMode ? "rgba(255, 255, 255, 0.03)" : "rgba(0, 0, 0, 0.02)"
                        border.color: Theme.isDarkMode ? "rgba(255, 255, 255, 0.08)" : "rgba(0, 0, 0, 0.08)"
                        border.width: 1
                        radius: 6

                        ScrollView {
                            id: previewScroll
                            anchors.fill: parent
                            anchors.margins: 4
                            clip: true

                            Text {
                                width: previewContainer.width - 16
                                text: inspector.docData ? inspector.docData.textSnippet : ""
                                font.family: Theme.defaultFont.family
                                font.pixelSize: 11
                                color: Theme.primaryText
                                wrapMode: Text.Wrap
                            }
                        }
                    }
                }

                KaakaoSeparator { Layout.fillWidth: true }

                // Metadata Fields
                GridLayout {
                    columns: 2
                    rowSpacing: 4
                    columnSpacing: 8
                    Layout.fillWidth: true

                    KaakaoLabel { text: "Contents:"; role: KaakaoLabel.Role.Small; color: Theme.sidebarSectionText; opacity: 1.0; visible: !!(inspector.docData && inspector.docData.isFolder) }
                    KaakaoLabel { text: (inspector.docData && inspector.docData.itemCountStr) || ""; role: KaakaoLabel.Role.Small; color: Theme.primaryText; opacity: 1.0; visible: !!(inspector.docData && inspector.docData.isFolder) }

                    KaakaoLabel { text: "Size:"; role: KaakaoLabel.Role.Small; color: Theme.sidebarSectionText; opacity: 1.0; visible: !(inspector.docData && inspector.docData.isFolder) }
                    KaakaoLabel { text: (inspector.docData && inspector.docData.fileSizeStr) || ""; role: KaakaoLabel.Role.Small; color: Theme.primaryText; opacity: 1.0; visible: !(inspector.docData && inspector.docData.isFolder) }

                    KaakaoLabel { text: "Pages:"; role: KaakaoLabel.Role.Small; color: Theme.sidebarSectionText; opacity: 1.0; visible: !(inspector.docData && inspector.docData.isFolder) }
                    KaakaoLabel { text: (inspector.docData && inspector.docData.pageCount !== undefined) ? inspector.docData.pageCount : ""; role: KaakaoLabel.Role.Small; color: Theme.primaryText; opacity: 1.0; visible: !(inspector.docData && inspector.docData.isFolder) }

                    KaakaoLabel { text: "Type:"; role: KaakaoLabel.Role.Small; color: Theme.sidebarSectionText; opacity: 1.0 }
                    KaakaoLabel {
                        text: {
                            if (!inspector.docData) return ""
                            if (inspector.docData.isFolder) return "Folder"
                            if (!inspector.docData.fileName) return ""
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

                    KaakaoLabel { text: "Imported:"; role: KaakaoLabel.Role.Small; color: Theme.sidebarSectionText; opacity: 1.0; visible: !(inspector.docData && inspector.docData.isFolder) }
                    KaakaoLabel { text: (inspector.docData && inspector.docData.dateAdded) ? inspector.formatDate(inspector.docData.dateAdded) : ""; role: KaakaoLabel.Role.Small; color: Theme.primaryText; opacity: 1.0; visible: !(inspector.docData && inspector.docData.isFolder) }

                    KaakaoLabel { text: "Opened:"; role: KaakaoLabel.Role.Small; color: Theme.sidebarSectionText; opacity: 1.0; visible: !(inspector.docData && inspector.docData.isFolder) }
                    KaakaoLabel { text: (inspector.docData && inspector.docData.lastOpened !== undefined) ? inspector.formatLastOpened(inspector.docData.lastOpened) : "Never"; role: KaakaoLabel.Role.Small; color: Theme.primaryText; opacity: 1.0; visible: !(inspector.docData && inspector.docData.isFolder) }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    visible: !(inspector.docData && inspector.docData.isFolder)

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
                        id: singleRatingRow
                        spacing: 4
                        Layout.alignment: Qt.AlignHCenter

                        property int hoveredIndex: -1
                        property int localRating: inspector.docData ? inspector.docData.starRating : 0

                        Connections {
                            target: inspector
                            ignoreUnknownSignals: true
                            function onDocDataChanged() {
                                singleRatingRow.localRating = inspector.docData ? inspector.docData.starRating : 0
                            }
                        }

                        Repeater {
                            model: 5
                            KaakaoLabel {
                                text: "★"
                                font.pixelSize: 22
                                color: {
                                    var active = false
                                    if (singleRatingRow.hoveredIndex !== -1) {
                                        active = index <= singleRatingRow.hoveredIndex
                                    } else {
                                        active = index < singleRatingRow.localRating
                                    }
                                    return active ? "#f1c40f" : (Theme.isDarkMode ? "#333" : "#ccc")
                                }
                                opacity: 1.0
                                scale: starMouseArea.containsMouse ? 1.2 : 1.0

                                Behavior on scale {
                                    NumberAnimation { duration: 100 }
                                }
                                Behavior on color {
                                    ColorAnimation { duration: 100 }
                                }

                                MouseArea {
                                    id: starMouseArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onEntered: singleRatingRow.hoveredIndex = index
                                    onExited: singleRatingRow.hoveredIndex = -1
                                    onClicked: {
                                        var newRating = index + 1
                                        if (singleRatingRow.localRating === newRating) {
                                            newRating = 0
                                        }
                                        singleRatingRow.localRating = newRating
                                        libraryController.batchUpdateRating([inspector.selectedId], newRating)
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
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        KaakaoLabel {
                            text: "Notes"
                            role: KaakaoLabel.Role.Small
                            font.weight: Font.DemiBold
                            color: Theme.sidebarSectionText
                            opacity: 1.0
                        }

                        Item { Layout.fillWidth: true } // Spacer

                        KaakaoLabel {
                            text: "Saved"
                            role: KaakaoLabel.Role.Small
                            font.pixelSize: 10
                            font.italic: true
                            color: Theme.isDarkMode ? "#2ecc71" : "#27ae60"
                            opacity: inspector.showSavedIndicator ? 1.0 : 0.0

                            Behavior on opacity {
                                NumberAnimation { duration: 200 }
                            }
                        }
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
}
