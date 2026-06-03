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
    id: aboutDialog
    title: "About NinjaLibrary"
    modal: true
    width: 380
    height: 480
    
    x: parent ? (parent.width - width) / 2 : 100
    y: parent ? (parent.height - height) / 2 : 100

    standardButtons: Dialog.Ok
    onAccepted: close()

    contentItem: ColumnLayout {
        spacing: 10

        KaakaoLabel {
            text: "🥷"
            font.pixelSize: 48
            Layout.alignment: Qt.AlignHCenter
        }

        KaakaoLabel {
            text: "NinjaLibrary"
            font.pixelSize: 20
            font.weight: Font.Bold
            Layout.alignment: Qt.AlignHCenter
        }

        KaakaoLabel {
            text: "Version 26.6.0 (June 2026 Release)"
            role: KaakaoLabel.Role.Small
            Layout.alignment: Qt.AlignHCenter
        }

        KaakaoLabel {
            text: "Copyright © 2026. Licensed under BSD-3-Clause."
            role: KaakaoLabel.Role.Small
            Layout.alignment: Qt.AlignHCenter
        }

        KaakaoSeparator { Layout.fillWidth: true }

        KaakaoLabel {
            text: "Attributions & Dependencies:"
            role: KaakaoLabel.Role.Small
            font.weight: Font.Bold
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            
            KaakaoLabel {
                width: availableWidth
                text: "• SQLite 3 - Local-first database indexing.\n" +
                      "• Tesseract OCR - Scanned document OCR fallback.\n" +
                      "• Kaakao QML Library - macOS-styled components.\n" +
                      "• Cocoa PDFKit / AppKit - Native macOS rendering.\n" +
                      "• Poppler-Qt6 - Native Linux rendering.\n" +
                      "• Catch2 & QTest - Automated test harnesses."
                role: KaakaoLabel.Role.Small
                wrapMode: Text.WordWrap
            }
        }
    }
}
