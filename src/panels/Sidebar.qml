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

    model: ListModel {
        id: sidebarModel
    }

    function rebuildModel() {
        var prevIndex = sidebar.currentIndex
        var prevTarget = ""
        var prevType = ""
        
        if (prevIndex >= 0 && prevIndex < sidebarModel.count) {
            var prevItem = sidebarModel.get(prevIndex)
            prevTarget = prevItem.target
            prevType = prevItem.type
        }

        sidebarModel.clear()
        
        // Add Library items
        sidebarModel.append({ "name": "All Documents", "icon": "📄", "category": "Library", "type": "section", "target": "All" })
        sidebarModel.append({ "name": "Recent", "icon": "🕒", "category": "Library", "type": "section", "target": "Recent" })
        sidebarModel.append({ "name": "Favorites", "icon": "⭐", "category": "Library", "type": "section", "target": "Favorites" })
        sidebarModel.append({ "name": "Duplicates", "icon": "👥", "category": "Library", "type": "section", "target": "Duplicates" })
        sidebarModel.append({ "name": "Offline", "icon": "⚠️", "category": "Library", "type": "section", "target": "Offline" })
        
        // Add Watched Folders
        var folders = libraryController.watchedFolders
        for (var i = 0; i < folders.length; i++) {
            var path = folders[i]
            var name = path.substring(path.lastIndexOf('/') + 1)
            if (name === "") name = path
            sidebarModel.append({ "name": name, "icon": "📁", "category": "Folders", "type": "folder", "target": path })
        }

        // Restore selection
        if (prevTarget !== "") {
            for (var j = 0; j < sidebarModel.count; j++) {
                var item = sidebarModel.get(j)
                if (item.target === prevTarget && item.type === prevType) {
                    sidebar.currentIndex = j;
                    return;
                }
            }
        }
        
        // Fallback to select first item (All Documents)
        if (sidebarModel.count > 0) {
            sidebar.currentIndex = 0
        }
    }

    Component.onCompleted: {
        rebuildModel()
        libraryController.watchedFoldersChanged.connect(rebuildModel)
    }

    onCurrentIndexChanged: {
        if (currentIndex < 0 || currentIndex >= sidebarModel.count) return
        var item = sidebarModel.get(currentIndex)
        if (item.type === "section") {
            sidebar.sectionSelected(item.target)
        } else if (item.type === "folder") {
            sidebar.folderSelected(item.target)
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
}
