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
    id: tableCanvas

    signal selectionChanged(var ids)
    signal doubleClicked(string path)

    property var innerListView: null

    Component.onCompleted: {
        let lv = findListView(tableView);
        if (lv) {
            lv.delegate = rowDelegateComponent;
            innerListView = lv;
        }
    }

    function findListView(parent) {
        if (!parent)
            return null;
        if (parent.toString().indexOf("ListView") >= 0)
            return parent;
        if (parent.children) {
            for (let i = 0; i < parent.children.length; ++i) {
                let found = findListView(parent.children[i]);
                if (found)
                    return found;
            }
        }
        return null;
    }

    Component {
        id: rowDelegateComponent
        LocalTableRowDelegate {
            columns: tableView.columns
        }
    }

    function selectId(docId) {
        var row = proxyFilter.rowOfDocId(docId);
        if (row !== -1) {
            tableView.currentIndex = row;
        }
    }

    function clearSelection() {
        tableView.currentIndex = -1;
        selectionChanged([]);
    }

    function moveUp() {
        if (innerListView) {
            innerListView.decrementCurrentIndex();
        }
    }

    function moveDown() {
        if (innerListView) {
            innerListView.incrementCurrentIndex();
        }
    }

    function moveLeft() {}
    function moveRight() {}

    KaakaoTableView {
        id: tableView
        anchors.fill: parent
        model: proxyFilter

        columns: [
            KaakaoTableColumn {
                role: "fileName"
                title: "Name"
                width: 320
                sortable: true
            },
            KaakaoTableColumn {
                role: "fileSizeStr"
                title: "Size"
                width: 100
                sortable: true
            },
            KaakaoTableColumn {
                role: "pageCount"
                title: "Pages"
                width: 80
                sortable: true
            },
            KaakaoTableColumn {
                role: "starRatingStr"
                title: "Rating"
                width: 110
                sortable: true
            },
            KaakaoTableColumn {
                role: "dateModifiedStr"
                title: "Last Modified"
                width: 130
                sortable: true
            },
            KaakaoTableColumn {
                role: "tagsStr"
                title: "Tags"
                width: 150
                sortable: false
            },
            KaakaoTableColumn {
                role: "notes"
                title: "Notes"
                width: Math.max(150, tableView.width - 890)
                sortable: false
            }
        ]

        onCurrentIndexChanged: {
            if (currentIndex >= 0 && currentIndex < proxyFilter.rowCount()) {
                var idx = proxyFilter.index(currentIndex, 0);
                var docId = proxyFilter.data(idx, 257); // 257 is IdRole
                tableCanvas.selectionChanged([docId]);
            } else {
                tableCanvas.selectionChanged([]);
            }
        }

        // Handle sorting requests from table columns
        onSortRequested: (role, order) => {
            if (role === "fileName") {
                proxyFilter.setSortRole(259); // FileNameRole
            } else if (role === "fileSizeStr") {
                proxyFilter.setSortRole(261); // FileSizeRole (sorts numerically!)
            } else if (role === "pageCount") {
                proxyFilter.setSortRole(266); // PageCountRole
            } else if (role === "starRatingStr") {
                proxyFilter.setSortRole(267); // StarRatingRole
            } else if (role === "dateModifiedStr") {
                proxyFilter.setSortRole(264); // DateModifiedRole
            }
            proxyFilter.sort(0, order);
        }
    }

    Connections {
        target: tableCanvas.innerListView ? tableCanvas.innerListView.Keys : null
        ignoreUnknownSignals: true
        function onPressed(event) {
            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                if (tableView.currentIndex >= 0 && tableView.currentIndex < proxyFilter.rowCount()) {
                    var isFolder = proxyFilter.get(tableView.currentIndex, "isFolder");
                    var path = proxyFilter.get(tableView.currentIndex, "absolutePath");
                    if (isFolder) {
                        proxyFilter.folderFilter = path;
                    } else {
                        tableCanvas.doubleClicked(path);
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
            }
        }
    }
}
