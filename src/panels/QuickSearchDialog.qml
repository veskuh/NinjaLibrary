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

Dialog {
    id: quickSearchDialog
    modal: true
    padding: Theme.paddingSmall
    width: 760
    height: 480

    // Center in window
    x: parent ? (parent.width - width) / 2 : 100
    y: parent ? (parent.height - height) / 2 : 100

    property string activeQuery: ""
    property var matchedDocs: []
    property var selectedDoc: null
    property var matchedSnippets: []
    property bool wasOpenedBeforePreview: false
    property bool returningFromPreview: false

    signal documentSnippetClicked(var doc, int pageIndex)

    onActiveQueryChanged: {
        searchDebounceTimer.restart();
    }

    onSelectedDocChanged: {
        updateSnippets();
    }

    onMatchedSnippetsChanged: {
        snippetsListView.currentIndex = matchedSnippets.length > 0 ? 0 : -1;
    }

    function updateSnippets() {
        if (selectedDoc && activeQuery.trim() !== "") {
            matchedSnippets = libraryController.searchDocumentContent(selectedDoc.docId, selectedDoc.absolutePath, activeQuery);
        } else {
            matchedSnippets = [];
        }
    }

    function highlightQuery(text, query) {
        if (!text) return "";
        if (!query) return text;
        
        var escaped = text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
        var escapedQuery = query.replace(/[-\/\\^$*+?.()|[\]{}]/g, '\\$&');
        var regex = new RegExp("(" + escapedQuery + ")", "gi");
        return escaped.replace(regex, "<span style='background-color: #FFE100; color: #000000; font-weight: bold;'>$1</span>");
    }

    Timer {
        id: searchDebounceTimer
        interval: 150
        repeat: false
        onTriggered: {
            var results = libraryController.searchDocuments(activeQuery);
            quickSearchDialog.matchedDocs = results;
            if (results.length > 0) {
                docListView.currentIndex = 0;
                quickSearchDialog.selectedDoc = results[0];
            } else {
                quickSearchDialog.selectedDoc = null;
            }
        }
    }

    background: Rectangle {
        color: Theme.windowBackground
        border.color: Theme.buttonBorder
        border.width: 1
        radius: Theme.radiusLarge
    }

    contentItem: ColumnLayout {
        spacing: 8
        focus: true

        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Escape) {
                quickSearchDialog.close();
                event.accepted = true;
            } else if (event.key === Qt.Key_Up) {
                if (docListView.activeFocus || snippetsListView.activeFocus) {
                    // Handled by default list view key handling
                } else {
                    docListView.decrementCurrentIndex();
                    event.accepted = true;
                }
            } else if (event.key === Qt.Key_Down) {
                if (docListView.activeFocus || snippetsListView.activeFocus) {
                    // Handled by default list view key handling
                } else {
                    docListView.incrementCurrentIndex();
                    event.accepted = true;
                }
            }
        }

        // Header Title Bar
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 4
            Layout.rightMargin: 4
            
            KaakaoLabel {
                text: "Quick Search"
                font.pixelSize: 15
                font.weight: Font.Bold
                Layout.fillWidth: true
            }

            KaakaoControlButton {
                controlStyle: KaakaoControlButton.ControlStyle.Inline
                implicitWidth: 16
                implicitHeight: 16
                onClicked: quickSearchDialog.close()
            }
        }

        // Search Input Field
        KaakaoTextField {
            id: searchField
            objectName: "quickSearchField"
            placeholderText: "Search document contents..."
            Layout.fillWidth: true
            focus: true
            text: quickSearchDialog.activeQuery

            KeyNavigation.tab: docListView
            KeyNavigation.backtab: snippetsListView

            onTextChanged: {
                quickSearchDialog.activeQuery = text;
            }

            // Bind Escape key inside text field to close dialog
            Keys.onEscapePressed: {
                quickSearchDialog.close();
            }
        }

        // Main Split Pane Content
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Left Pane: Document list
            Rectangle {
                id: leftPane
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: Math.round(quickSearchDialog.width * 0.43)
                color: Theme.alternatingRowBackgroundOdd
                border.color: Theme.buttonBorder
                border.width: 1
                radius: Theme.radiusStandard
                clip: true

                ListView {
                    id: docListView
                    objectName: "quickDocListView"
                    anchors.fill: parent
                    model: quickSearchDialog.matchedDocs
                    currentIndex: -1
                    focus: true

                    KeyNavigation.tab: snippetsListView
                    KeyNavigation.backtab: searchField

                    Keys.onReturnPressed: {
                        if (currentIndex >= 0 && currentIndex < model.length) {
                            snippetsListView.forceActiveFocus();
                        }
                    }
                    Keys.onEnterPressed: {
                        if (currentIndex >= 0 && currentIndex < model.length) {
                            snippetsListView.forceActiveFocus();
                        }
                    }
                    Keys.onSpacePressed: {
                        if (currentIndex >= 0 && currentIndex < model.length) {
                            quickSearchDialog.close();
                            quickSearchDialog.documentSnippetClicked(quickSearchDialog.selectedDoc, 0);
                        }
                    }

                    delegate: ItemDelegate {
                        width: ListView.view.width
                        height: 38
                        highlighted: ListView.isCurrentItem

                        onDoubleClicked: {
                            quickSearchDialog.close();
                            quickSearchDialog.documentSnippetClicked(quickSearchDialog.selectedDoc, 0);
                        }

                        background: Rectangle {
                            color: highlighted ? Theme.selectionBackgroundActive : "transparent"
                        }

                        onClicked: {
                            docListView.currentIndex = index;
                            quickSearchDialog.selectedDoc = modelData;
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 8

                            // Simple file type indicator icon
                            KaakaoLabel {
                                text: modelData.fileName.toLowerCase().endsWith(".pdf") ? "📄" : "📝"
                                font.pixelSize: 16
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0

                                KaakaoLabel {
                                    text: modelData.fileName
                                    elide: Text.ElideRight
                                    font.weight: highlighted ? Font.Bold : Font.Normal
                                    color: highlighted ? Theme.selectionTextActive : Theme.primaryText
                                    Layout.fillWidth: true
                                }
                            }

                            // Match badge count
                            Rectangle {
                                implicitWidth: matchBadgeText.implicitWidth + 12
                                implicitHeight: 18
                                radius: 9
                                color: highlighted ? Theme.selectionTextActive : Theme.buttonBorder

                                KaakaoLabel {
                                    id: matchBadgeText
                                    text: modelData.matchCount + (modelData.matchCount === 1 ? " match" : " matches")
                                    font.pixelSize: 9
                                    color: highlighted ? Theme.selectionBackgroundActive : Theme.secondaryText
                                    anchors.centerIn: parent
                                }
                            }
                        }
                    }

                    onCurrentIndexChanged: {
                        if (currentIndex >= 0 && currentIndex < model.length) {
                            quickSearchDialog.selectedDoc = model[currentIndex];
                        }
                    }

                    KaakaoLabel {
                        text: quickSearchDialog.activeQuery.trim() === "" ? "Type to search..." : "No documents match search"
                        visible: docListView.count === 0
                        anchors.centerIn: parent
                        color: Theme.secondaryText
                    }
                }
            }

            // Right Pane: Excerpts list
            Rectangle {
                anchors.left: leftPane.right
                anchors.leftMargin: 8
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                color: Theme.alternatingRowBackgroundOdd
                border.color: Theme.buttonBorder
                border.width: 1
                radius: Theme.radiusStandard
                clip: true

                ListView {
                    id: snippetsListView
                    objectName: "quickSnippetsListView"
                    anchors.fill: parent
                    model: quickSearchDialog.matchedSnippets
                    focus: true

                    KeyNavigation.tab: searchField
                    KeyNavigation.backtab: docListView

                    Keys.onReturnPressed: {
                        if (currentIndex >= 0 && currentIndex < model.length) {
                            var snippet = model[currentIndex];
                            quickSearchDialog.close();
                            quickSearchDialog.documentSnippetClicked(quickSearchDialog.selectedDoc, snippet.pageIndex);
                        }
                    }
                    Keys.onEnterPressed: {
                        if (currentIndex >= 0 && currentIndex < model.length) {
                            var snippet = model[currentIndex];
                            quickSearchDialog.close();
                            quickSearchDialog.documentSnippetClicked(quickSearchDialog.selectedDoc, snippet.pageIndex);
                        }
                    }

                    delegate: ItemDelegate {
                        width: ListView.view.width
                        height: 80
                        highlighted: ListView.isCurrentItem

                        background: Rectangle {
                            color: highlighted ? Theme.selectionBackgroundActive : (hovered ? Theme.toolButtonHovered : "transparent")
                            border.color: Theme.buttonBorder
                            border.width: (highlighted || hovered) ? 1 : 0
                        }

                        onClicked: {
                            quickSearchDialog.close();
                            quickSearchDialog.documentSnippetClicked(quickSearchDialog.selectedDoc, modelData.pageIndex);
                        }

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 6
                            spacing: 2

                            KaakaoLabel {
                                text: quickSearchDialog.selectedDoc.fileName.toLowerCase().endsWith(".pdf") ? ("Page " + (modelData.pageIndex + 1)) : "Content Match"
                                font.weight: Font.Bold
                                font.pixelSize: 11
                                color: highlighted ? Theme.selectionTextActive : Theme.primaryAccent
                            }

                            KaakaoLabel {
                                text: quickSearchDialog.highlightQuery(modelData.context, quickSearchDialog.activeQuery)
                                textFormat: Text.RichText
                                font.pixelSize: 11
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                maximumLineCount: 3
                                color: highlighted ? Theme.selectionTextActive : Theme.primaryText
                            }
                        }
                    }

                    KaakaoLabel {
                        text: quickSearchDialog.selectedDoc ? "No text matches found inside document" : "Select a document to view matches"
                        visible: snippetsListView.count === 0
                        anchors.centerIn: parent
                        color: Theme.secondaryText
                    }
                }
            }
        }

        // Bottom Footer Action Bar
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 4
            Layout.rightMargin: 4
            Layout.topMargin: 4
            Layout.bottomMargin: 4
            spacing: 8

            Item {
                Layout.fillWidth: true
            }

            KaakaoButton {
                id: previewBtn
                objectName: "quickSearchPreviewButton"
                text: "Preview Document"
                enabled: quickSearchDialog.selectedDoc !== null
                onClicked: {
                    quickSearchDialog.close();
                    quickSearchDialog.documentSnippetClicked(quickSearchDialog.selectedDoc, 0);
                }
            }

            KaakaoButton {
                text: "Close"
                onClicked: quickSearchDialog.close()
            }
        }
    }

    onOpened: {
        if (!returningFromPreview) {
            searchField.text = "";
            matchedDocs = [];
            matchedSnippets = [];
            selectedDoc = null;
            searchField.forceActiveFocus();
        } else {
            snippetsListView.forceActiveFocus();
        }
        returningFromPreview = false;
    }
}
