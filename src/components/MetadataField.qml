/*
 * Copyright (c) 2026, NinjaLibrary
 * All rights reserved.
 */

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Kaakao 1.0

/*!
    \qmltype MetadataField
    \brief One label+value row inside a two-column GridLayout.

    Instantiate inside a GridLayout with columns: 2. On completion the
    component reparents its internal MetadataLabel and MetadataValue
    siblings into the parent GridLayout, so they participate in the
    shared column layout like any other direct children would.

    Usage:
    \code
    GridLayout {
        columns: 2
        MetadataField { label: "Size:"; value: "3.2 MB"; isFolder: false; visibleWhenFolder: false }
        MetadataField { label: "Path:"; value: "/some/path"; fillWidth: true; toolTip: "/some/path" }
    }
    \endcode
*/
Item {
    id: root

    // ----- public API --------------------------------------------------------
    property string label: ""
    property string value: ""

    /*!
        Set to \c true to show only when isFolder==true,
        \c false to show only when isFolder==false,
        or leave unset (undefined) to always show.
    */
    property var    visibleWhenFolder: undefined

    /*! Must be bound to the data-source isFolder flag so visibility is reactive. */
    property bool   isFolder: false

    /*! When true, sets Layout.fillWidth on the value cell. */
    property bool   fillWidth: false

    /*! Forwarded as elide on the value Text item. */
    property int    elide: Text.ElideNone

    /*!
        When non-empty the value cell shows a ToolTip with this text on hover.
        Defaults to empty (no tooltip).
    */
    property string toolTip: ""

    // ----- computed visibility -----------------------------------------------
    readonly property bool actualVisible: {
        if (visibleWhenFolder === true)  return isFolder;
        if (visibleWhenFolder === false) return !isFolder;
        return true;
    }

    // ----- tell the layout to skip this invisible wrapper ------------------
    // visible: false is sufficient — GridLayout does not lay out invisible items.
    visible: false

    // ----- internal cells (reparented in onCompleted) -----------------------
    MetadataLabel {
        id: labelItem
        text:    root.label
        visible: root.actualVisible
    }

    MetadataValue {
        id: valueItem
        text:    root.value
        elide:   root.elide
        visible: root.actualVisible

        ToolTip.visible: toolTipArea.containsMouse && root.toolTip !== ""
        ToolTip.text:    root.toolTip
        ToolTip.delay:   500

        MouseArea {
            id: toolTipArea
            anchors.fill:   parent
            hoverEnabled:   root.toolTip !== ""
            acceptedButtons: Qt.NoButton
        }
    }

    // ----- reparent into containing GridLayout ------------------------------
    Component.onCompleted: {
        if (!parent) return;
        labelItem.parent = parent;
        valueItem.parent = parent;
        if (fillWidth) {
            valueItem.Layout.fillWidth = true;
        }
    }
}
