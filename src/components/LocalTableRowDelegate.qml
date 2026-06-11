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

ItemDelegate {
    id: control

    // Desktop rows are compact
    implicitHeight: 24
    implicitWidth: ListView.view ? ListView.view.width : 200

    property list<KaakaoTableColumn> columns
    readonly property int rowIndex: index
    property int modelUpdateCount: 0

    readonly property bool isEvenRow: index % 2 === 0
    readonly property bool isSelected: ListView.isCurrentItem
    readonly property var rowData: modelData
    readonly property var docIdValue: {
        let dummy = control.modelUpdateCount
        if (typeof model !== "undefined" && model.docId !== undefined) return model.docId
        if (control.rowData !== undefined && control.rowData.docId !== undefined) return control.rowData.docId
        if (control.ListView.view && control.ListView.view.model && typeof control.ListView.view.model.get === "function") {
            let val = control.ListView.view.model.get(control.rowIndex, "docId")
            return val !== undefined ? val : -1
        }
        return -1
    }
    readonly property string absolutePathValue: {
        let dummy = control.modelUpdateCount
        if (typeof model !== "undefined" && model.absolutePath !== undefined) return model.absolutePath
        if (control.rowData !== undefined && control.rowData.absolutePath !== undefined) return control.rowData.absolutePath
        if (control.ListView.view && control.ListView.view.model && typeof control.ListView.view.model.get === "function") {
            let val = control.ListView.view.model.get(control.rowIndex, "absolutePath")
            return val !== undefined ? val.toString() : ""
        }
        return ""
    }
    readonly property var starRatingValue: {
        let dummy = control.modelUpdateCount
        if (typeof model !== "undefined" && model.starRating !== undefined) return model.starRating
        if (control.rowData !== undefined && control.rowData.starRating !== undefined) return control.rowData.starRating
        if (control.ListView.view && control.ListView.view.model && typeof control.ListView.view.model.get === "function") {
            let val = control.ListView.view.model.get(control.rowIndex, "starRating")
            return val !== undefined ? val : 0
        }
        return 0
    }
    readonly property bool isOffline: {
        let dummy = control.modelUpdateCount
        if (control.ListView.view && control.ListView.view.model && typeof control.ListView.view.model.get === "function") {
            let val = control.ListView.view.model.get(control.rowIndex, "isOffline")
            return val !== undefined ? (val === true || val === "true" || val === 1 || val === "1") : false
        }
        return false
    }

    readonly property bool isFolderValue: {
        let dummy = control.modelUpdateCount
        if (control.ListView.view && control.ListView.view.model && typeof control.ListView.view.model.get === "function") {
            let val = control.ListView.view.model.get(control.rowIndex, "isFolder")
            return val !== undefined ? (val === true || val === "true" || val === 1 || val === "1") : false
        }
        return false
    }

    readonly property string itemCountStrValue: {
        let dummy = control.modelUpdateCount
        if (control.ListView.view && control.ListView.view.model && typeof control.ListView.view.model.get === "function") {
            let val = control.ListView.view.model.get(control.rowIndex, "itemCountStr")
            return val !== undefined ? val.toString() : ""
        }
        return ""
    }



    Connections {
        target: control.ListView.view ? control.ListView.view.model : null
        ignoreUnknownSignals: true
        function onLayoutChanged() {
            control.modelUpdateCount++
        }
        function onModelReset() {
            control.modelUpdateCount++
        }
        function onRowsInserted() {
            control.modelUpdateCount++
        }
        function onRowsRemoved() {
            control.modelUpdateCount++
        }
        function onRowsMoved() {
            control.modelUpdateCount++
        }
        function onDataChanged(topLeft, bottomRight) {
            if (control.rowIndex >= topLeft.row && control.rowIndex <= bottomRight.row) {
                control.modelUpdateCount++
            }
        }
    }

    background: Rectangle {
        anchors.fill: parent
        color: {
            if (control.isSelected) {
                if (control.ListView.view && control.ListView.view.activeFocus)
                    return Theme.selectionBackgroundActive;
                return Theme.selectionBackgroundInactive;
            }
            return control.isEvenRow ? Theme.alternatingRowBackgroundEven : Theme.alternatingRowBackgroundOdd;
        }
    }

    contentItem: Row {
        id: cellRow
        anchors.fill: parent
        spacing: 0
        z: 1

        Repeater {
            model: control.columns
            delegate: Item {
                id: cellItem
                width: modelData.width
                height: control.height
                clip: true

                readonly property string cellValue: {
                    let dummy = control.modelUpdateCount
                    let roleName = modelData.role
                    if (control.rowData !== undefined && control.rowData[roleName] !== undefined)
                        return control.rowData[roleName]
                    
                    if (control.ListView.view && control.ListView.view.model && typeof control.ListView.view.model.get === "function") {
                        let val = control.ListView.view.model.get(control.rowIndex, roleName)
                        return val !== undefined ? val.toString() : ""
                    }
                    
                    return ""
                }

                ToolTip.visible: cellMouse.containsMouse && ToolTip.text !== ""
                ToolTip.text: {
                    let dummy = control.modelUpdateCount
                    if (modelData.showAsIndicator) {
                        return cellItem.cellValue
                    }
                    let roleName = modelData.role
                    let tooltipRole = roleName + "Tooltip"
                    
                    let valTooltip = undefined
                    if (control.rowData !== undefined && control.rowData[tooltipRole] !== undefined) {
                        valTooltip = control.rowData[tooltipRole]
                    } else if (control.ListView.view && control.ListView.view.model && typeof control.ListView.view.model.get === "function") {
                        valTooltip = control.ListView.view.model.get(control.rowIndex, tooltipRole)
                    }
                    
                    if (valTooltip !== undefined && valTooltip !== "") {
                        return valTooltip
                    }
                    
                    if (cellLabel.truncated)
                        return cellLabel.text
                    return ""
                }

                MouseArea {
                    id: cellMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                }

                // Custom Status Indicator
                Rectangle {
                    anchors.centerIn: parent
                    width: 10
                    height: 10
                    radius: 5
                    visible: modelData.showAsIndicator
                    color: {
                        let dummy = control.modelUpdateCount
                        let colorRole = modelData.indicatorColorRole
                        let valColor = undefined
                        if (control.rowData !== undefined && control.rowData[colorRole] !== undefined) {
                            valColor = control.rowData[colorRole]
                        } else if (control.ListView.view && control.ListView.view.model && typeof control.ListView.view.model.get === "function") {
                            valColor = control.ListView.view.model.get(control.rowIndex, colorRole)
                        }
                        
                        if (valColor !== undefined && valColor !== "") {
                            if (valColor === "green") return Theme.colorSuccess || "#28a745"
                            if (valColor === "red") return Theme.colorError || "#ff3b30"
                            if (valColor === "orange") return "#ff9500"
                            if (valColor === "purple") return "#af52de"
                            return valColor
                        }
                        // Fallback to value if it's already a color string/HEX
                        let val = cellItem.cellValue
                        if (val.startsWith("#") || val === "red" || val === "green" || val === "blue" || val === "orange" || val === "yellow" || val === "gray" || val === "purple")
                            return val
                        return "gray"
                    }
                    border.width: 1
                    border.color: Theme.isDarkMode ? "rgba(255,255,255,0.2)" : "rgba(0,0,0,0.15)"
                }

                Label {
                    id: cellLabel
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    verticalAlignment: Text.AlignVCenter
                    visible: !modelData.showAsIndicator && modelData.role !== "starRatingStr"
                    text: {
                        if (control.isFolderValue) {
                            if (modelData.role === "fileName") return "📁 " + cellItem.cellValue
                            if (modelData.role === "fileSizeStr") return control.itemCountStrValue
                            if (modelData.role === "dateModifiedStr") return cellItem.cellValue
                            return ""
                        }
                        return cellItem.cellValue
                    }
                    font: Theme.defaultFont
                    elide: modelData.elide !== undefined ? modelData.elide : Text.ElideRight
                    renderType: Text.NativeRendering
                    color: {
                        if (control.isOffline && !control.isFolderValue) {
                            return "#8e8e93"
                        }
                        return (control.isSelected && control.ListView.view && control.ListView.view.activeFocus) 
                               ? Theme.selectionTextActive 
                               : Theme.selectionTextInactive
                    }
                }

                Row {
                    id: starRow
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    visible: modelData.role === "starRatingStr" && !control.isFolderValue
                    spacing: 2

                    property int hoveredIndex: -1

                    Repeater {
                        model: 5
                        Text {
                            text: {
                                if (starRow.hoveredIndex >= 0) {
                                    return index <= starRow.hoveredIndex ? "★" : "☆";
                                }
                                return index < control.starRatingValue ? "★" : "☆";
                            }
                            font.pixelSize: 14
                            color: {
                                if (control.isOffline) return "#8e8e93";
                                return (control.isSelected && control.ListView.view && control.ListView.view.activeFocus)
                                       ? Theme.selectionTextActive
                                       : (starRow.hoveredIndex >= 0 ? Theme.primaryAccent : Theme.primaryText);
                            }
                            verticalAlignment: Text.AlignVCenter
                            height: parent.height
                            renderType: Text.NativeRendering

                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.LeftButton
                                onEntered: starRow.hoveredIndex = index
                                onExited: starRow.hoveredIndex = -1
                                onClicked: {
                                    if (control.ListView.view) {
                                        control.ListView.view.currentIndex = control.rowIndex
                                        control.ListView.view.forceActiveFocus()
                                    }
                                    if (control.docIdValue !== -1) {
                                        libraryController.batchUpdateRating([control.docIdValue], index + 1);
                                    }
                                }
                            }
                        }
                    }
                }

                // Vertical divider line
                Rectangle {
                    anchors.right: parent.right
                    height: parent.height
                    width: 1
                    color: Theme.headerDivider
                    visible: index < control.columns.length - 1
                }
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: (mouse) => {
            if (control.ListView.view) {
                control.ListView.view.currentIndex = index
                control.ListView.view.forceActiveFocus()
            }
            if (mouse.button === Qt.RightButton) {
                var popupPos = mapToItem(itemContextMenu.parent, mouse.x, mouse.y)
                if (typeof itemContextMenu !== "undefined") {
                    itemContextMenu.targetDocId = control.docIdValue
                    itemContextMenu.targetPath = control.absolutePathValue
                    itemContextMenu.targetRating = control.starRatingValue
                    itemContextMenu.targetIsOffline = control.isOffline
                    itemContextMenu.popup(popupPos.x, popupPos.y)
                }
            }
        }
        onDoubleClicked: (mouse) => {
            if (mouse.button === Qt.LeftButton) {
                if (control.isFolderValue) {
                    proxyFilter.folderFilter = control.absolutePathValue
                } else if (typeof tableCanvas !== "undefined") {
                    tableCanvas.doubleClicked(control.absolutePathValue)
                }
            }
        }
    }
}
