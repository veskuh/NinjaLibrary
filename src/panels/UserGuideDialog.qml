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
    id: userGuideDialog
    docTitle: "User Guide"
    docText: "Welcome to NinjaLibrary, a fast, local-first document indexing and search tool.\n\n" +
             "📁 Adding Watched Folders\n" +
             "Use File > Add Watched Folder... (Ctrl+O / Cmd+O) or click the '+' button in the sidebar footer to select a local directory. NinjaLibrary automatically scans and indexes files within watched folders.\n\n" +
             "🖼️ Browsing & View Toggle\n" +
             "Switch between Grid View (Ctrl+1) and Table View (Ctrl+2) using the segmented control in the top toolbar. Use the zoom slider in the status bar to adjust thumbnail sizes in Grid View.\n\n" +
             "🔍 Searching & Quick Search\n" +
             "• Search Field (Ctrl+F): Filter documents instantly by title, path, or text.\n" +
             "• Quick Search (Ctrl+Shift+F / Cmd+Shift+F): Perform full-text search queries across document contents and OCR snippets, with direct preview navigation.\n\n" +
             "🏷️ Tagging Documents\n" +
             "Select one or more documents and use the Inspector panel on the right to view, add, or remove tags. Click any tag in the sidebar to filter the library by that tag.\n\n" +
             "📌 Sidebar Navigation & Folder Browsing Modes\n" +
             "Navigate between All Documents, Favorites, Trash, Watched Folders, and Tags in the left sidebar. When browsing a folder, choose your viewing mode via the View menu or folder action gear menu:\n" +
             "• Browse Folders Hierarchically (subfolders as clickable items)\n" +
             "• Only Show Files at Current Level\n" +
             "• Include All Subfolder Contents (recursive)\n\n" +
             "ℹ️ Inspector Panel\n" +
             "Toggle the Inspector with Ctrl+Alt+I or the 'i' toolbar button. View rich metadata, file size, page count, OCR status, star ratings, and custom tags for selected items.\n\n" +
             "🖐️ Drag-and-Drop Support\n" +
             "Drag files or folders directly into the main window to add them to your library or watched folders."
}
