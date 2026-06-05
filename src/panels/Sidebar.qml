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

KaakaoSidebar {
    id: sidebar
    
    sidebarWidth: 200

    signal folderSelected(string path)
    signal sectionSelected(string section)
    signal tagSelected(string tag)

    model: ListModel {
        id: sidebarModel
    }

    property bool rebuilding: false

    function rebuildModel() {
        var prevIndex = sidebar.currentIndex
        var prevTarget = ""
        var prevType = ""
        
        if (prevIndex >= 0 && prevIndex < sidebarModel.count) {
            var prevItem = sidebarModel.get(prevIndex)
            prevTarget = prevItem.target
            prevType = prevItem.type
        }

        sidebar.rebuilding = true
        sidebarModel.clear()
        
        // Add Library items
        sidebarModel.append({ "name": "All Documents", "icon": "📄", "category": "Library", "type": "section", "target": "All" })
        sidebarModel.append({ "name": "Recent", "icon": "🕒", "category": "Library", "type": "section", "target": "Recent" })
        sidebarModel.append({ "name": "Favorites", "icon": "⭐", "category": "Library", "type": "section", "target": "Favorites" })
        sidebarModel.append({ "name": "Duplicates", "icon": "👥", "category": "Library", "type": "section", "target": "Duplicates" })
        sidebarModel.append({ "name": "Unavailable", "icon": "⚠️", "category": "Library", "type": "section", "target": "Unavailable" })
        
        // Add Watched Folders
        var folders = libraryController.watchedFolders
        for (var i = 0; i < folders.length; i++) {
            var path = folders[i]
            var name = path.substring(path.lastIndexOf('/') + 1)
            if (name === "") name = path
            sidebarModel.append({ "name": name, "icon": "📁", "category": "Folders", "type": "folder", "target": path })
        }

        // Add Tags
        var tags = libraryController.getUniqueTags()
        for (var k = 0; k < tags.length; k++) {
            var tag = tags[k]
            sidebarModel.append({ "name": tag, "icon": "🏷️", "category": "Tags", "type": "tag", "target": tag })
        }

        // Restore selection
        var restored = false
        if (prevTarget !== "") {
            for (var j = 0; j < sidebarModel.count; j++) {
                var item = sidebarModel.get(j)
                if (item.target === prevTarget && item.type === prevType) {
                    sidebar.currentIndex = j;
                    restored = true;
                    break;
                }
            }
        }
        
        // Fallback to select first item (All Documents)
        if (!restored && sidebarModel.count > 0) {
            sidebar.currentIndex = 0
        }

        sidebar.rebuilding = false

        // Only fire selection change signals if the target or type actually changed
        var currentItem = currentIndex >= 0 ? sidebarModel.get(currentIndex) : null
        if (currentItem) {
            if (currentItem.target !== prevTarget || currentItem.type !== prevType) {
                if (currentItem.type === "section") {
                    sidebar.sectionSelected(currentItem.target)
                } else if (currentItem.type === "folder") {
                    sidebar.folderSelected(currentItem.target)
                } else if (currentItem.type === "tag") {
                    sidebar.tagSelected(currentItem.target)
                }
            }
        }
    }

    Component.onCompleted: {
        rebuildModel()
        if (typeof libraryController !== "undefined" && libraryController) {
            if (libraryController.watchedFoldersChanged) {
                libraryController.watchedFoldersChanged.connect(rebuildModel)
            }
            if (libraryController.libraryChanged) {
                libraryController.libraryChanged.connect(rebuildModel)
            }
        }
    }

    onCurrentIndexChanged: {
        if (rebuilding) return
        if (currentIndex < 0 || currentIndex >= sidebarModel.count) return
        var item = sidebarModel.get(currentIndex)
        if (item.type === "section") {
            sidebar.sectionSelected(item.target)
        } else if (item.type === "folder") {
            sidebar.folderSelected(item.target)
        } else if (item.type === "tag") {
            sidebar.tagSelected(item.target)
        }
    }

    onContextMenu: (index, globalPos) => {
        if (index < 0 || index >= sidebarModel.count) return
        var item = sidebarModel.get(index)
        if (item.type === "folder") {
            folderContextMenu.targetPath = item.target
            folderContextMenu.popup(globalPos)
        }
    }

    onDoubleClicked: (index) => {
        if (index < 0 || index >= sidebarModel.count) return
        var item = sidebarModel.get(index)
        if (item.type === "folder" && item.target !== "") {
            Qt.openUrlExternally("file://" + item.target)
        }
    }

    KaakaoMenu {
        id: folderContextMenu
        property string targetPath: ""
        
        KaakaoMenuItem {
            text: "Stop Watching Folder"
            onTriggered: {
                libraryController.removeWatchedFolder(folderContextMenu.targetPath)
            }
        }
    }

    function getSelectedFolder() {
        if (currentIndex >= 0 && currentIndex < sidebarModel.count) {
            var item = sidebarModel.get(currentIndex)
            if (item && item.type === "folder") {
                return item.target
            }
        }
        return ""
    }

    function selectFolder(path) {
        for (var i = 0; i < sidebarModel.count; i++) {
            var item = sidebarModel.get(i)
            if (item && item.type === "folder" && item.target === path) {
                sidebar.currentIndex = i
                return true
            }
        }
        return false
    }
}
