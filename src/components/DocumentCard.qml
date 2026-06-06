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

Rectangle {
    id: card
    
    property string fileName: ""
    property string absolutePath: ""
    property int fileSize: 0
    property string thumbnailPath: ""
    property int starRating: 0
    property bool isOffline: false
    property bool isSelected: false

    signal clicked(var event)
    signal doubleClicked()

    implicitWidth: 160
    implicitHeight: 200
    radius: 6
    color: isSelected ? (Theme.isDarkMode ? "#2d3748" : "#e1f0ff") 
                      : (cardMouseArea.containsMouse ? (Theme.isDarkMode ? "#2d2d2d" : "#f3f8fe") : Theme.contentBackground)
    border.color: isSelected ? Theme.primaryAccent 
                            : (cardMouseArea.containsMouse ? (Theme.isDarkMode ? "#666666" : "#a8cbf7") : Theme.buttonBorder)
    border.width: isSelected ? 2 : 1

    scale: cardMouseArea.containsMouse ? 1.025 : 1.0

    Behavior on scale {
        NumberAnimation { duration: 150; easing.type: Easing.OutQuad }
    }
    Behavior on color { ColorAnimation { duration: 120 } }
    Behavior on border.color { ColorAnimation { duration: 120 } }

    // Soft drop shadow (standard Yosemite card look)
    layer.enabled: !isSelected
    layer.effect: ShaderEffect {
        // Simple fallback shadow simulated by card border, layer keeps layout clean
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        // Thumbnail Area
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.isDarkMode ? "#181818" : "#f0f0f0"
            radius: 4
            clip: true

            DocumentPreview {
                anchors.fill: parent
                thumbnailPath: card.thumbnailPath
                fileName: card.fileName
                isOffline: card.isOffline
                fontPixelSize: 24
            }

            // Offline indicator badge
            Rectangle {
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: 4
                width: 14
                height: 14
                radius: 7
                color: Theme.colorError
                visible: card.isOffline
                
                KaakaoLabel {
                    text: "!"
                    color: "white"
                    font.pixelSize: 10
                    font.weight: Font.Bold
                    opacity: 1.0
                    anchors.centerIn: parent
                }
            }
        }

        // Info Area
        KaakaoLabel {
            text: card.fileName
            font.pixelSize: 12
            font.weight: Font.Medium
            elide: Text.ElideMiddle
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 2
            
            // Stars Rating display
            Row {
                Layout.alignment: Qt.AlignLeft
                spacing: 1
                Repeater {
                    model: 5
                    KaakaoLabel {
                        text: "★"
                        font.pixelSize: 11
                        color: index < card.starRating ? "#f39c12" : (Theme.isDarkMode ? "#444" : "#ccc")
                        opacity: 1.0
                    }
                }
            }

            // Size Display
            KaakaoLabel {
                text: {
                    var kb = card.fileSize / 1024;
                    if (kb > 1024) {
                        return (kb / 1024).toFixed(1) + " MB";
                    }
                    return Math.round(kb) + " KB";
                }
                role: KaakaoLabel.Role.Small
                font.pixelSize: 10
                color: Theme.sidebarSectionText
                opacity: 1.0
                Layout.alignment: Qt.AlignRight
            }
        }
    }

    MouseArea {
        id: cardMouseArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: (mouse) => {
            card.clicked(mouse)
        }
        onDoubleClicked: {
            card.doubleClicked()
        }
    }
}
