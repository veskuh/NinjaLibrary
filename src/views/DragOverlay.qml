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
import QtQuick.Layouts
import Kaakao 1.0

Rectangle {
    id: overlay
    objectName: "dragOverlay"
    
    property bool containsDrag: false
    
    anchors.fill: parent
    color: Theme.isDarkMode ? Qt.rgba(Theme.primaryAccent.r, Theme.primaryAccent.g, Theme.primaryAccent.b, 0.15) : Qt.rgba(Theme.primaryAccent.r, Theme.primaryAccent.g, Theme.primaryAccent.b, 0.04)
    border.color: Theme.primaryAccent
    border.width: 3
    z: 9999

    // Smooth opacity fade-in / fade-out
    opacity: containsDrag ? 1.0 : 0.0
    visible: opacity > 0.0
    Behavior on opacity {
        NumberAnimation { duration: 150; easing.type: Easing.OutQuad }
    }

    Rectangle {
        id: dropCard
        anchors.centerIn: parent
        width: 300
        height: 180
        color: Theme.isDarkMode ? Qt.rgba(0.12, 0.12, 0.12, 0.9) : Qt.rgba(1, 1, 1, 0.95)
        radius: 12
        border.color: Theme.buttonBorder
        border.width: 1

        // Smooth scale and subtle bounce transition for the drop card
        transform: Scale {
            id: cardScale
            origin.x: dropCard.width / 2
            origin.y: dropCard.height / 2
            xScale: overlay.containsDrag ? 1.0 : 0.9
            yScale: overlay.containsDrag ? 1.0 : 0.9
            
            Behavior on xScale { NumberAnimation { duration: 250; easing.type: Easing.OutBack } }
            Behavior on yScale { NumberAnimation { duration: 250; easing.type: Easing.OutBack } }
        }

        ColumnLayout {
            anchors.centerIn: parent
            spacing: 12

            Text {
                text: "📥"
                font.pixelSize: 48
                Layout.alignment: Qt.AlignHCenter
            }

            KaakaoLabel {
                text: "Drop File or Folder"
                font.pixelSize: 18
                font.weight: Font.Bold
                color: Theme.primaryText
                Layout.alignment: Qt.AlignHCenter
            }

            KaakaoLabel {
                text: "Add to watched folders and select"
                font.pixelSize: 12
                color: Theme.sidebarSectionText
                Layout.alignment: Qt.AlignHCenter
            }
        }
    }
}
