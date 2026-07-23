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
import "../components"

DocumentationDialog {
    id: shortcutsDialog
    docTitle: "Keyboard Shortcuts"
    docText: "File & Application\n" +
             "• Ctrl+O / Cmd+O — Add Watched Folder\n" +
             "• Ctrl+Q / Cmd+Q — Quit\n" +
             "• Ctrl+M / Cmd+M — Minimize\n" +
             "• Ctrl+, / Cmd+, — Preferences\n\n" +
             "Edit\n" +
             "• Ctrl+X / Cmd+X — Cut\n" +
             "• Ctrl+C / Cmd+C — Copy\n" +
             "• Ctrl+V / Cmd+V — Paste\n" +
             "• Ctrl+A / Cmd+A — Select All\n" +
             "• Backspace / Delete — Move selected item to Trash\n\n" +
             "Search & View\n" +
             "• Ctrl+F / Cmd+F — Focus Search field\n" +
             "• Ctrl+Shift+F / Cmd+Shift+F — Quick Search dialog\n" +
             "• Ctrl+1 — Switch to Grid View\n" +
             "• Ctrl+2 — Switch to Table View\n" +
             "• Ctrl+Alt+S — Toggle Sidebar\n" +
             "• Ctrl+Alt+I — Toggle Inspector\n" +
             "• Space — Quick Look document preview\n" +
             "• Escape — Clear search / selection"
}
