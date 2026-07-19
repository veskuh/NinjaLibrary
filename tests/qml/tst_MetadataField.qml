/*
 * Copyright (c) 2026, NinjaLibrary
 * All rights reserved.
 */

import QtQuick
import QtQuick.Layouts
import QtTest
import NinjaLibrary
import Kaakao 1.0

TestCase {
    name: "tst_MetadataField"
    width: 640
    height: 480
    visible: true

    // -----------------------------------------------------------------------
    // Helper: wrap a MetadataField inside a 2-column GridLayout, as it would
    // appear in Inspector.qml.
    // -----------------------------------------------------------------------
    Component {
        id: testGridComponent
        GridLayout {
            columns: 2
            MetadataField {
                id: field
                label: "Size:"
                value: "3.2 MB"
            }
        }
    }

    // -----------------------------------------------------------------------
    // Test 1 — visibility logic driven by visibleWhenFolder / isFolder
    // -----------------------------------------------------------------------
    function test_visibility_logic() {
        let field = createTemporaryQmlObject(
            "import NinjaLibrary; MetadataField {}",
            this
        );
        verify(field !== null, "MetadataField should be created");

        // Default — undefined visibleWhenFolder → always visible
        compare(field.actualVisible, true, "undefined visibleWhenFolder → always visible");

        // visibleWhenFolder: true  (folder-only row)
        field.visibleWhenFolder = true;
        field.isFolder = false;
        compare(field.actualVisible, false, "visibleWhenFolder=true, isFolder=false → hidden");
        field.isFolder = true;
        compare(field.actualVisible, true,  "visibleWhenFolder=true, isFolder=true  → visible");

        // visibleWhenFolder: false  (file-only row)
        field.visibleWhenFolder = false;
        field.isFolder = true;
        compare(field.actualVisible, false, "visibleWhenFolder=false, isFolder=true  → hidden");
        field.isFolder = false;
        compare(field.actualVisible, true,  "visibleWhenFolder=false, isFolder=false → visible");

        field.destroy();
    }

    // -----------------------------------------------------------------------
    // Test 2 — reparenting: after onCompleted the GridLayout should have
    //          gained the label and value children.
    // -----------------------------------------------------------------------
    function test_reparenting_into_grid_layout() {
        let grid = testGridComponent.createObject(this);
        verify(grid !== null, "GridLayout should be created");

        // Allow Component.onCompleted to execute
        wait(50);

        // The MetadataField wrapper itself sits in grid.children too (index 0),
        // but it has Layout.ignored=true and visible=false.
        // The label and value items are reparented alongside it.
        // Expect at least 3 children: field wrapper + labelItem + valueItem.
        verify(grid.children.length >= 3,
            "GridLayout should have at least 3 children after reparenting, got " + grid.children.length);

        // Confirm the label and value texts are reachable somewhere in the grid
        let foundLabel = false;
        let foundValue = false;
        for (let i = 0; i < grid.children.length; ++i) {
            let child = grid.children[i];
            if (typeof child.text !== "undefined") {
                if (child.text === "Size:")    foundLabel = true;
                if (child.text === "3.2 MB")  foundValue = true;
            }
        }
        verify(foundLabel, "Label 'Size:' should be in GridLayout children");
        verify(foundValue, "Value '3.2 MB' should be in GridLayout children");

        grid.destroy();
    }

    // -----------------------------------------------------------------------
    // Test 3 — label/value property bindings are reactive
    // -----------------------------------------------------------------------
    function test_property_bindings() {
        let field = createTemporaryQmlObject(
            "import NinjaLibrary; MetadataField { label: 'Orig:'; value: 'orig' }",
            this
        );
        verify(field !== null);

        compare(field.label, "Orig:",  "Initial label");
        compare(field.value, "orig",   "Initial value");

        field.label = "New:";
        field.value = "updated";
        compare(field.label, "New:",    "Updated label");
        compare(field.value, "updated", "Updated value");

        field.destroy();
    }

    // -----------------------------------------------------------------------
    // Test 4 — toolTip and fillWidth are plain properties (no crash)
    // -----------------------------------------------------------------------
    function test_plain_properties() {
        let field = createTemporaryQmlObject(
            "import NinjaLibrary; MetadataField { label: 'Path:'; value: '/a/b'; fillWidth: true; toolTip: '/a/b'; elide: 1 }",
            this
        );
        verify(field !== null, "MetadataField with fillWidth/toolTip/elide should be created");
        compare(field.fillWidth, true,  "fillWidth should be true");
        compare(field.toolTip,   "/a/b", "toolTip should be set");
        compare(field.elide,     1,      "elide should be 1 (ElideRight)");
        field.destroy();
    }
}
