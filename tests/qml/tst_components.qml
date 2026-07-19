import QtQuick
import QtQuick.Controls
import QtTest
import NinjaLibrary
import Kaakao 1.0
import "../../src/panels/TagUtils.js" as TagUtils

TestCase {
    name: "ComponentTests"
    width: 640
    height: 480
    visible: true

    Component {
        id: documentCardComponent
        DocumentCard {}
    }

    Component {
        id: tagPillComponent
        TagPill {}
    }

    function init() {
    }

    function cleanup() {
    }

    // Helper to recursively check for raw Text elements
    function hasRawTextElement(item) {
        if (!item)
            return false;

        let str = item.toString();
        // A raw Text element starts with QQuickText.
        // A Label starts with QQuickLabel or KaakaoLabel.
        if (str.indexOf("QQuickText") === 0) {
            // It is a raw Text element!
            return true;
        }

        // Recursively check children
        if (item.children) {
            for (let i = 0; i < item.children.length; ++i) {
                if (hasRawTextElement(item.children[i])) {
                    return true;
                }
            }
        }
        // Also check contentItem/background if they are not in children
        if (item.contentItem && hasRawTextElement(item.contentItem)) {
            return true;
        }
        if (item.background && hasRawTextElement(item.background)) {
            return true;
        }
        return false;
    }

    function test_document_card() {
        let card = createTemporaryQmlObject("import NinjaLibrary; DocumentCard {}", this);
        verify(card !== null, "DocumentCard should be created");

        // Verify property propagation
        card.fileName = "test_document.pdf";
        card.absolutePath = "/path/to/test_document.pdf";
        card.fileSize = 1048576; // 1MB
        card.starRating = 3;
        card.isOffline = false;
        card.isSelected = true;
        wait(500);

        // Verify Theme colors are applied (e.g. selection/highlight state)
        tryCompare(card, "color", Theme.isDarkMode ? "#2d3748" : "#e1f0ff", 5000, "Card background color should match selection theme color");

        // Verify we can update and get correct values
        card.isOffline = true;
        compare(card.isOffline, true);

        // Helper to find status label inside DocumentCard (identified by font.pixelSize === 9)
        function findStatusLabel(parent) {
            if (!parent)
                return null;
            if (parent.toString().indexOf("Label") >= 0 && parent.font.pixelSize === 9) {
                return parent;
            }
            if (parent.children) {
                for (let i = 0; i < parent.children.length; ++i) {
                    let found = findStatusLabel(parent.children[i]);
                    if (found)
                        return found;
                }
            }
            return null;
        }

        let statusLabel = findStatusLabel(card);
        verify(statusLabel !== null, "Status label should exist");

        // Set card online and empty thumbnail path (simulating a text file card)
        card.isOffline = false;
        card.thumbnailPath = "";
        compare(statusLabel.visible, false, "Status label should be hidden when online and thumbnailPath is empty");
        compare(statusLabel.text, "", "Status label text should be empty when online and thumbnailPath is empty");

        // Set card offline
        card.isOffline = true;
        compare(statusLabel.text, "UNAVAILABLE", "Status label text should be UNAVAILABLE when offline");
        compare(statusLabel.visible, true, "Status label should be visible when offline");

        // Set card online but with a thumbnail path (simulating loading a PDF thumbnail)
        card.isOffline = false;
        card.thumbnailPath = "file:///tmp/dummy.png";
        compare(statusLabel.text, "LOADING...", "Status label text should be LOADING... when thumbnail is set but not ready");
        compare(statusLabel.visible, true, "Status label should be visible when loading thumbnail");

        // Wait for it to fail because /tmp/dummy.png does not exist
        tryCompare(statusLabel, "text", "", 2000, "Status label text should become empty when loading fails");
        tryCompare(statusLabel, "visible", false, 2000, "Status label should be hidden when loading fails");

        // Set card online but with a failed/empty thumbnail path ("file://")
        card.thumbnailPath = "file://";
        compare(statusLabel.text, "", "Status label text should be empty when thumbnail path is 'file://'");
        compare(statusLabel.visible, false, "Status label should be hidden when thumbnail path is 'file://'");

        // Verify there are ZERO raw Text elements (only KaakaoLabel/Label should be used)
        let hasRaw = hasRawTextElement(card);
        verify(!hasRaw, "DocumentCard must not contain raw Text elements");

        card.destroy();
    }

    function test_tag_pill() {
        let pill = createTemporaryQmlObject("import NinjaLibrary; TagPill {}", this);
        verify(pill !== null, "TagPill should be created");

        pill.text = "Important";
        pill.showDelete = true;
        pill.isSelected = false;

        // Check signals
        let clickSpy = createTemporaryQmlObject("import QtTest; SignalSpy {}", this);
        clickSpy.target = pill;
        clickSpy.signalName = "clicked";

        let removeSpy = createTemporaryQmlObject("import QtTest; SignalSpy {}", this);
        removeSpy.target = pill;
        removeSpy.signalName = "removeRequested";

        // Helper to find visual child with specific text
        function findChildByText(parent, targetText) {
            if (!parent)
                return null;
            if (parent.text === targetText)
                return parent;
            if (parent.children) {
                for (let i = 0; i < parent.children.length; ++i) {
                    let found = findChildByText(parent.children[i], targetText);
                    if (found)
                        return found;
                }
            }
            return null;
        }

        // Wait for layout flow pass
        wait(200);

        // Simulate click on the main pill area
        mouseClick(pill);
        compare(clickSpy.count, 1, "Click signal should be emitted");

        // Simulate click on the delete "×" button
        let deleteBtn = findChildByText(pill, "×");
        verify(deleteBtn !== null, "Delete button '×' should be found inside TagPill");
        mouseClick(deleteBtn);
        compare(removeSpy.count, 1, "Remove requested signal should be emitted when clicking '×'");

        // Verify no raw Text elements
        let hasRaw = hasRawTextElement(pill);
        verify(!hasRaw, "TagPill must not contain raw Text elements");

        pill.destroy();
        clickSpy.destroy();
        removeSpy.destroy();
    }

    Component {
        id: collapsibleSplitPaneComponent
        SplitView {
            CollapsibleSplitPane {
                id: testPane
            }
        }
    }

    function test_collapsible_split_pane() {
        let splitView = collapsibleSplitPaneComponent.createObject(this);
        verify(splitView !== null, "SplitView should be created");
        
        // Find the CollapsibleSplitPane child safely
        function findPane(item) {
            if (!item) return null;
            if (item.toString().indexOf("CollapsibleSplitPane") >= 0) return item;
            if (item.children) {
                for (let i = 0; i < item.children.length; ++i) {
                    let found = findPane(item.children[i]);
                    if (found) return found;
                }
            }
            return null;
        }
        let pane = findPane(splitView);
        verify(pane !== null, "CollapsibleSplitPane should be found");

        pane.collapsed = false;
        pane.minWidth = 100;
        pane.preferredWidth = 200;
        pane.maxWidth = 300;

        // Verify default non-collapsed states
        compare(pane.collapsed, false);
        compare(pane.minWidth, 100);
        compare(pane.preferredWidth, 200);
        compare(pane.maxWidth, 300);

        // Check state transitions
        pane.collapsed = true;
        compare(pane.collapsed, true);
        
        // Wait for the collapse transition animation to complete
        tryCompare(pane.SplitView, "minimumWidth", 0, 2000);
        compare(pane.SplitView.preferredWidth, 0);
        compare(pane.SplitView.maximumWidth, 0);

        pane.collapsed = false;
        compare(pane.collapsed, false);
        
        // Wait for the expand transition animation to complete
        tryCompare(pane.SplitView, "minimumWidth", 100, 2000);
        compare(pane.SplitView.preferredWidth, 200);
        compare(pane.SplitView.maximumWidth, 300);

        splitView.destroy();
    }

    function test_parse_tags_text_edge_cases() {
        // Test TagUtils.parseTagsText directly — no Inspector context required.
        function check(input, expected) {
            let result = TagUtils.parseTagsText(input);
            compare(result.length, expected.length, "Lengths should match for input: '" + input + "'");
            for (let i = 0; i < expected.length; ++i) {
                compare(result[i], expected[i], "Element " + i + " should match for input: '" + input + "'");
            }
        }

        // Empty string
        check("", []);

        // All-whitespace
        check("   ", []);

        // Multiple blank comma-separated entries
        check(" ,   ,  ", []);

        // Duplicates — first occurrence wins
        check("work, work, school, work", ["work", "school"]);

        // Leading/trailing commas
        check(",work,school,", ["work", "school"]);
        check(",,,work,,,school,,,", ["work", "school"]);

        // Mixed spacing — trimmed but NOT lower-cased (tag names are case-sensitive)
        check("  work  ,  School , WORK ", ["work", "School", "WORK"]);

        // Single tag, no commas
        check("onlyone", ["onlyone"]);

        // All-duplicate input
        check("dup,dup,dup,dup", ["dup"]);
    }
}
