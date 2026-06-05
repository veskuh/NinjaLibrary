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

Rectangle {
    id: pill
    property string text: ""
    property bool isSelected: false
    property bool showDelete: false
    property bool isMixed: false
    property color tagColor: "#3498db"

    signal clicked()
    signal removeRequested()

    implicitWidth: labelRow.width + 12
    implicitHeight: 20
    radius: 4
    
    opacity: 0.0
    color: isSelected ? Theme.primaryAccent : (hoverArea.containsMouse ? Theme.toolButtonHovered : Theme.contentBackground)
    border.color: isSelected ? Theme.accentBorder : (isMixed ? Theme.sidebarSectionText : Theme.buttonBorder)
    border.width: 1

    scale: 0.0

    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutQuad } }
    Behavior on opacity { NumberAnimation { duration: 150 } }
    Behavior on color { ColorAnimation { duration: 120 } }
    Behavior on border.color { ColorAnimation { duration: 120 } }

    Component.onCompleted: {
        scale = 1.0
        opacity = Qt.binding(function() { return isMixed ? 0.7 : 1.0 })
    }

    MouseArea {
        id: hoverArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        onClicked: pill.clicked()
    }

    Row {
        id: labelRow
        anchors.centerIn: parent
        spacing: 6

        Rectangle {
            width: 8
            height: 8
            radius: 4
            color: pill.tagColor
            anchors.verticalCenter: parent.verticalCenter
            visible: !pill.isSelected
        }

        KaakaoLabel {
            id: label
            text: pill.text
            color: pill.isSelected ? Theme.selectionTextActive : Theme.primaryText
            role: KaakaoLabel.Role.Small
            font.weight: Font.Medium
            font.italic: pill.isMixed
            anchors.verticalCenter: parent.verticalCenter
        }

        KaakaoLabel {
            text: "×"
            color: pill.isSelected ? Theme.selectionTextActive : Theme.sidebarSectionText
            font.pixelSize: 13
            font.weight: Font.Bold
            opacity: deleteMouse.containsMouse ? 1.0 : 0.6
            visible: pill.showDelete
            anchors.verticalCenter: parent.verticalCenter
            
            Behavior on opacity { NumberAnimation { duration: 100 } }

            MouseArea {
                id: deleteMouse
                anchors.fill: parent
                anchors.margins: -4
                hoverEnabled: true
                onClicked: {
                    pill.removeRequested()
                }
            }
        }
    }
}
