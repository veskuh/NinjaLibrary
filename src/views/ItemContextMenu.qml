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

KaakaoMenu {
    id: menu
    
    property int targetDocId: -1
    property string targetPath: ""
    property int targetRating: 0
    
    KaakaoMenuItem {
        text: "Open"
        onTriggered: {
            if (menu.targetPath !== "") {
                Qt.openUrlExternally("file://" + menu.targetPath)
                if (menu.targetDocId !== -1) {
                    libraryController.markDocumentOpened(menu.targetDocId)
                }
            }
        }
    }
    
    KaakaoMenuItem {
        text: "Show in Finder"
        onTriggered: {
            if (menu.targetPath !== "") {
                libraryController.showInFinder(menu.targetPath)
            }
        }
    }
    
    KaakaoMenuSeparator {}
    
    KaakaoMenu {
        id: rateMenu
        title: "Rate"
        
        onAboutToShow: {
            for (var i = 0; i < 5; i++) {
                var item = rateRepeater.itemAt(i)
                if (item) {
                    item.checked = (i + 1) === menu.targetRating
                }
            }
        }
        
        Repeater {
            id: rateRepeater
            model: 5
            KaakaoMenuItem {
                text: {
                    var stars = ""
                    for (var i = 0; i <= index; i++) stars += "★"
                    for (var j = index + 1; j < 5; j++) stars += "☆"
                    return stars
                }
                checkable: true
                checked: (index + 1) === menu.targetRating
                onTriggered: {
                    libraryController.batchUpdateRating([menu.targetDocId], index + 1)
                }
            }
        }
    }
}
