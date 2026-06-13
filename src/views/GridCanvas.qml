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
import Kaakao 1.0
import "../components"

Item {
    id: gridCanvas

    property real scaleFactor: 160 // Bound to bottom toolbar zoom slider
    property var selectedIds: []

    signal selectionChanged(var ids)
    signal doubleClicked(string path)

    function selectId(docId) {
        selectedIds = [docId];
        selectionChanged(selectedIds);
        for (var i = 0; i < proxyFilter.rowCount(); i++) {
            var idx = proxyFilter.index(i, 0);
            if (proxyFilter.data(idx, 257) === docId) {
                gridView.currentIndex = i;
                break;
            }
        }
    }

    function clearSelection() {
        selectedIds = [];
        selectionChanged(selectedIds);
        gridView.currentIndex = -1;
    }

    function moveUp() {
        gridView.gridView.moveCurrentIndexUp();
    }

    function moveDown() {
        gridView.gridView.moveCurrentIndexDown();
    }

    function moveLeft() {
        gridView.gridView.moveCurrentIndexLeft();
    }

    function moveRight() {
        gridView.gridView.moveCurrentIndexRight();
    }

    KaakaoGridView {
        id: gridView
        anchors.fill: parent

        gridView.topMargin: Theme.paddingMedium
        gridView.leftMargin: Theme.paddingMedium
        gridView.rightMargin: Theme.paddingMedium
        gridView.bottomMargin: Theme.paddingMedium

        model: proxyFilter
        cellWidth: scaleFactor + Theme.paddingMedium
        cellHeight: scaleFactor * 1.25 + Theme.paddingMedium

        Connections {
            target: gridView.gridView.Keys
            function onPressed(event) {
                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    var idx = gridView.currentIndex;
                    if (idx >= 0 && idx < proxyFilter.rowCount()) {
                        var isFolder = proxyFilter.get(idx, "isFolder");
                        var path = proxyFilter.get(idx, "absolutePath");
                        if (isFolder) {
                            proxyFilter.folderFilter = path;
                        } else {
                            gridCanvas.doubleClicked(path);
                        }
                        event.accepted = true;
                    }
                } else if (event.key === Qt.Key_Backspace || (event.key === Qt.Key_Up && (event.modifiers & Qt.MetaModifier || event.modifiers & Qt.ControlModifier))) {
                    var current = proxyFilter.folderFilter;
                    var selectedRoot = sidebar.getSelectedFolder();
                    if (current !== "" && current !== selectedRoot) {
                        var lastSlash = current.lastIndexOf('/');
                        if (lastSlash > 0) {
                            var parentPath = current.substring(0, lastSlash);
                            if (parentPath.length >= selectedRoot.length) {
                                proxyFilter.folderFilter = parentPath;
                                event.accepted = true;
                            }
                        }
                    }
                } else if (gridCanvas.selectedIds.length !== 1) {
                    if (event.key === Qt.Key_Up || event.key === Qt.Key_Down || event.key === Qt.Key_Left || event.key === Qt.Key_Right) {
                        event.accepted = true;
                    }
                }
            }
        }

        onCurrentIndexChanged: {
            if (gridCanvas.selectedIds.length === 1 && currentIndex >= 0 && currentIndex < gridView.gridView.count) {
                var docId = model.get(currentIndex, "docId");
                if (docId !== undefined) {
                    gridCanvas.selectedIds = [docId];
                    gridCanvas.selectionChanged(gridCanvas.selectedIds);
                }
            }
        }

        delegate: Item {
            width: gridView.cellWidth
            height: gridView.cellHeight

            DocumentCard {
                id: docCard
                anchors.centerIn: parent
                fileName: model.fileName
                absolutePath: model.absolutePath
                fileSize: model.fileSize
                thumbnailPath: model.thumbnailPath || ""
                starRating: model.starRating
                isOffline: model.isOffline
                isFolder: model.isFolder !== undefined ? model.isFolder : false
                itemCount: model.itemCount !== undefined ? model.itemCount : 0
                itemCountStr: model.itemCountStr !== undefined ? model.itemCountStr : ""

                width: scaleFactor
                height: scaleFactor * 1.25

                isSelected: gridCanvas.selectedIds.indexOf(model.docId) >= 0

                // Request thumbnail if not loaded yet
                Component.onCompleted: {
                    if (!model.isFolder && model.thumbnailPath === "" && !model.isOffline) {
                        var ext = model.fileName.substring(model.fileName.lastIndexOf('.') + 1).toLowerCase();
                        var isTextDoc = (ext === "txt" || ext === "md" || ext === "doc" || ext === "docx" || ext === "xls" || ext === "xlsx" || ext === "ppt" || ext === "pptx");
                        if (!isTextDoc) {
                            libraryController.requestThumbnail(model.docId, model.absolutePath, false);
                        }
                    }
                }

                onClicked: event => {
                    var docId = model.docId;
                    var isCtrl = (event.modifiers & Qt.ControlModifier) || (event.modifiers & Qt.MetaModifier);

                    if (event.button === Qt.RightButton) {
                        if (gridCanvas.selectedIds.indexOf(docId) < 0) {
                            gridCanvas.selectedIds = [docId];
                            gridView.currentIndex = index;
                            gridView.gridView.forceActiveFocus();
                            gridCanvas.selectionChanged(gridCanvas.selectedIds);
                        }
                        var popupPos = docCard.mapToItem(itemContextMenu.parent, event.x, event.y);
                        if (typeof itemContextMenu !== "undefined") {
                            itemContextMenu.targetDocId = docId;
                            itemContextMenu.targetPath = model.absolutePath;
                            itemContextMenu.targetRating = model.starRating;
                            itemContextMenu.targetIsOffline = model.isOffline;
                            itemContextMenu.targetIsFolder = model.isFolder !== undefined ? model.isFolder : false;
                            itemContextMenu.popup(popupPos.x, popupPos.y);
                        }
                        return;
                    }

                    if (isCtrl) {
                        var idx = gridCanvas.selectedIds.indexOf(docId);
                        var currentList = gridCanvas.selectedIds.slice(); // copy array
                        if (idx >= 0) {
                            currentList.splice(idx, 1);
                        } else {
                            currentList.push(docId);
                        }
                        gridCanvas.selectedIds = currentList;
                    } else {
                        gridCanvas.selectedIds = [docId];
                    }

                    gridView.currentIndex = index;
                    gridView.gridView.forceActiveFocus();
                    gridCanvas.selectionChanged(gridCanvas.selectedIds);
                }

                onDoubleClicked: {
                    if (model.isFolder) {
                        proxyFilter.folderFilter = model.absolutePath;
                    } else {
                        gridCanvas.doubleClicked(model.absolutePath);
                    }
                }
            }
        }
    }

    // Deselect when clicking empty space
    MouseArea {
        anchors.fill: parent
        z: -1 // place behind gridView items
        onClicked: {
            gridCanvas.clearSelection();
        }
    }
}
