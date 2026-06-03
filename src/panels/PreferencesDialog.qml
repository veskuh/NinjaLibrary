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

KaakaoDialog {
    id: prefsDialog
    title: "Preferences"
    modal: true
    width: 340
    height: 240
    
    x: parent ? (parent.width - width) / 2 : 100
    y: parent ? (parent.height - height) / 2 : 100

    contentItem: ColumnLayout {
        spacing: 12

        KaakaoLabel {
            text: "Application Settings"
            role: KaakaoLabel.Role.Header
        }

        KaakaoSeparator { Layout.fillWidth: true }

        KaakaoCheckBox {
            id: sidecarCheckbox
            text: "Enable Metadata Sidecars (.ninja)"
            checked: true
            Layout.fillWidth: true
        }

        KaakaoCheckBox {
            id: autoWatchCheckbox
            text: "Automatic Directory Watching"
            checked: true
            Layout.fillWidth: true
        }

        Item { Layout.fillHeight: true }
    }

    footer: DialogButtonBox {
        background: Item {}
        alignment: Qt.AlignRight
        padding: 16
        spacing: 12
        
        KaakaoButton {
            text: "Force Library Scan"
            DialogButtonBox.buttonRole: DialogButtonBox.ResetRole
            onClicked: {
                var folders = libraryController.watchedFolders
                for (var i = 0; i < folders.length; ++i) {
                    libraryController.scanRequested(folders[i])
                }
                prefsDialog.close()
            }
        }
        
        KaakaoButton {
            text: "Done"
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            highlighted: true
            onClicked: prefsDialog.accept()
        }
    }
}
