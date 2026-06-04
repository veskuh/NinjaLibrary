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

MenuBar {
    id: appMenuBar

    signal openFolderRequested()
    signal focusSearchRequested()
    signal toggleSidebarRequested()
    signal toggleInspectorRequested()
    signal setViewModeRequested(int index)
    signal minimizeRequested()
    signal openAboutRequested()
    signal openPreferencesRequested()

    Menu {
        title: "File"
        MenuItem {
            action: Action {
                text: "Add Watched Folder..."
                shortcut: StandardKey.Open
                onTriggered: appMenuBar.openFolderRequested()
            }
        }
        MenuSeparator {}
        MenuItem {
            action: Action {
                text: "Quit"
                shortcut: StandardKey.Quit
                onTriggered: Qt.quit()
            }
        }
    }

    Menu {
        title: "Edit"
        MenuItem {
            action: Action {
                text: "Cut"
                shortcut: StandardKey.Cut
                enabled: {
                    var win = appMenuBar.Window.window;
                    var item = win ? win.activeFocusItem : null;
                    return item && (typeof item.cut === "function") && (typeof item.selectedText === "string") && (item.selectedText.length > 0);
                }
                onTriggered: {
                    var win = appMenuBar.Window.window;
                    if (win && win.activeFocusItem && typeof win.activeFocusItem.cut === "function") {
                        win.activeFocusItem.cut();
                    }
                }
            }
        }
        MenuItem {
            action: Action {
                text: "Copy"
                shortcut: StandardKey.Copy
                enabled: {
                    var win = appMenuBar.Window.window;
                    var item = win ? win.activeFocusItem : null;
                    return item && (typeof item.copy === "function") && (typeof item.selectedText === "string") && (item.selectedText.length > 0);
                }
                onTriggered: {
                    var win = appMenuBar.Window.window;
                    if (win && win.activeFocusItem && typeof win.activeFocusItem.copy === "function") {
                        win.activeFocusItem.copy();
                    }
                }
            }
        }
        MenuItem {
            action: Action {
                text: "Paste"
                shortcut: StandardKey.Paste
                enabled: {
                    var win = appMenuBar.Window.window;
                    var item = win ? win.activeFocusItem : null;
                    return item && (typeof item.paste === "function") && (item.canPaste === true);
                }
                onTriggered: {
                    var win = appMenuBar.Window.window;
                    if (win && win.activeFocusItem && typeof win.activeFocusItem.paste === "function") {
                        win.activeFocusItem.paste();
                    }
                }
            }
        }
        MenuItem {
            action: Action {
                text: "Select All"
                shortcut: StandardKey.SelectAll
                enabled: {
                    var win = appMenuBar.Window.window;
                    var item = win ? win.activeFocusItem : null;
                    return item && (typeof item.selectAll === "function") && (typeof item.length === "number") && (item.length > 0);
                }
                onTriggered: {
                    var win = appMenuBar.Window.window;
                    if (win && win.activeFocusItem && typeof win.activeFocusItem.selectAll === "function") {
                        win.activeFocusItem.selectAll();
                    }
                }
            }
        }
        MenuSeparator {}
        MenuItem {
            action: Action {
                text: "Find..."
                shortcut: StandardKey.Find
                onTriggered: appMenuBar.focusSearchRequested()
            }
        }
    }

    Menu {
        title: "View"
        MenuItem {
            action: Action {
                text: "Toggle Sidebar"
                shortcut: "Ctrl+Alt+S"
                onTriggered: appMenuBar.toggleSidebarRequested()
            }
        }
        MenuItem {
            action: Action {
                text: "Toggle Inspector"
                shortcut: "Ctrl+Alt+I"
                onTriggered: appMenuBar.toggleInspectorRequested()
            }
        }
        MenuSeparator {}
        MenuItem {
            action: Action {
                text: "Grid View Layout"
                shortcut: "Ctrl+1"
                onTriggered: appMenuBar.setViewModeRequested(0)
            }
        }
        MenuItem {
            action: Action {
                text: "Table View Layout"
                shortcut: "Ctrl+2"
                onTriggered: appMenuBar.setViewModeRequested(1)
            }
        }
    }

    Menu {
        title: "Window"
        MenuItem {
            action: Action {
                text: "Minimize"
                shortcut: "Ctrl+M"
                onTriggered: appMenuBar.minimizeRequested()
            }
        }
    }

    Menu {
        title: "Help"
        MenuItem {
            action: Action {
                text: "About NinjaLibrary"
                onTriggered: appMenuBar.openAboutRequested()
            }
        }
        MenuItem {
            action: Action {
                text: "Preferences..."
                shortcut: StandardKey.Preferences
                onTriggered: appMenuBar.openPreferencesRequested()
            }
        }
    }
}
