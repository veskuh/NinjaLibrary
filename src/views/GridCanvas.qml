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
        selectedIds = [docId]
        selectionChanged(selectedIds)
    }

    function clearSelection() {
        selectedIds = []
        selectionChanged(selectedIds)
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

        delegate: Item {
            width: gridView.cellWidth
            height: gridView.cellHeight

            DocumentCard {
                anchors.centerIn: parent
                fileName: model.fileName
                absolutePath: model.absolutePath
                fileSize: model.fileSize
                thumbnailPath: model.thumbnailPath || ""
                starRating: model.starRating
                isOffline: model.isOffline
                
                width: scaleFactor
                height: scaleFactor * 1.25

                isSelected: gridCanvas.selectedIds.indexOf(model.docId) >= 0

                // Request thumbnail if not loaded yet
                Component.onCompleted: {
                    if (model.thumbnailPath === "" && !model.isOffline) {
                        var ext = model.fileName.substring(model.fileName.lastIndexOf('.') + 1).toLowerCase();
                        var isTextDoc = (ext === "txt" || ext === "md" || ext === "doc" || ext === "docx" ||
                                         ext === "xls" || ext === "xlsx" || ext === "ppt" || ext === "pptx");
                        if (!isTextDoc) {
                            libraryController.requestThumbnail(model.docId, model.absolutePath, false)
                        }
                    }
                }

                onClicked: (event) => {
                    var docId = model.docId
                    var isCtrl = (event.modifiers & Qt.ControlModifier) || (event.modifiers & Qt.MetaModifier)
                    
                    if (isCtrl) {
                        var idx = gridCanvas.selectedIds.indexOf(docId)
                        var currentList = gridCanvas.selectedIds.slice() // copy array
                        if (idx >= 0) {
                            currentList.splice(idx, 1)
                        } else {
                            currentList.push(docId)
                        }
                        gridCanvas.selectedIds = currentList
                    } else {
                        gridCanvas.selectedIds = [docId]
                    }
                    
                    gridView.currentIndex = index
                    gridView.forceActiveFocus()
                    gridCanvas.selectionChanged(gridCanvas.selectedIds)
                }

                onDoubleClicked: {
                    gridCanvas.doubleClicked(model.absolutePath)
                }
            }
        }
    }

    // Deselect when clicking empty space
    MouseArea {
        anchors.fill: parent
        z: -1 // place behind gridView items
        onClicked: {
            gridCanvas.clearSelection()
        }
    }
}
