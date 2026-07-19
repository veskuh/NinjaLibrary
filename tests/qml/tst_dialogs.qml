import QtQuick
import QtTest
import NinjaLibrary
import Kaakao 1.0

TestCase {
    name: "DialogTests"
    width: 640
    height: 480
    visible: true

    // Helper to find a child by type
    function findChildByType(parent, typeName) {
        if (!parent)
            return null;
        if (parent.toString().indexOf(typeName) >= 0)
            return parent;
        if (parent.children) {
            for (let i = 0; i < parent.children.length; ++i) {
                let found = findChildByType(parent.children[i], typeName);
                if (found)
                    return found;
            }
        }
        return null;
    }

    // Helper to recursively find all children of a given type
    function findChildrenByType(parent, typeName, list) {
        if (!parent)
            return;
        if (parent.toString().indexOf(typeName) >= 0) {
            if (list.indexOf(parent) === -1) {
                list.push(parent);
            }
        }
        if (parent.children) {
            for (let i = 0; i < parent.children.length; ++i) {
                findChildrenByType(parent.children[i], typeName, list);
            }
        }
        if (parent.contentItem) {
            findChildrenByType(parent.contentItem, typeName, list);
        }
        if (parent.footer) {
            findChildrenByType(parent.footer, typeName, list);
        }
        if (parent.header) {
            findChildrenByType(parent.header, typeName, list);
        }
    }

    function test_about_dialog() {
        let dialog = createTemporaryQmlObject("import NinjaLibrary; AboutDialog {}", this);
        verify(dialog !== null, "AboutDialog should be created");

        // Verify title
        compare(dialog.title, "About NinjaLibrary");

        // Verify it inherits/is a KaakaoDialog (has unique symbol and text properties)
        verify(dialog.symbol !== undefined, "AboutDialog should have symbol property from KaakaoDialog");
        verify(dialog.text !== undefined, "AboutDialog should have text property from KaakaoDialog");

        dialog.destroy();
    }

    function test_preferences_dialog() {
        let dialog = createTemporaryQmlObject("import NinjaLibrary; PreferencesDialog {}", this);
        verify(dialog !== null, "PreferencesDialog should be created");

        compare(dialog.title, "Preferences");
        verify(dialog.symbol !== undefined, "PreferencesDialog should have symbol property from KaakaoDialog");
        verify(dialog.text !== undefined, "PreferencesDialog should have text property from KaakaoDialog");

        // Find checkbox children
        let checkboxes = [];
        findChildrenByType(dialog, "KaakaoCheckBox", checkboxes);
        verify(checkboxes.length >= 2, "PreferencesDialog should have at least 2 KaakaoCheckBoxes");

        // Find button children
        let buttons = [];
        findChildrenByType(dialog, "KaakaoButton", buttons);
        verify(buttons.length >= 2, "PreferencesDialog should have KaakaoButtons in the footer");

        dialog.destroy();
    }

    function test_trash_confirm_dialog() {
        let dialog = createTemporaryQmlObject("import NinjaLibrary; TrashConfirmDialog {}", this);
        verify(dialog !== null, "TrashConfirmDialog should be created");

        // Verify default property values
        compare(dialog.title, "Move to Trash");
        compare(dialog.symbol, "🗑️");

        // Test filename extraction Unix path
        dialog.filePath = "/Users/test/Documents/my_awesome_file.pdf";
        compare(dialog.fileName, "my_awesome_file.pdf");
        compare(dialog.text, "Are you sure you want to move 'my_awesome_file.pdf' to the Trash?");

        // Test filename extraction Windows path
        dialog.filePath = "C:\\Users\\test\\Documents\\another_file.png";
        compare(dialog.fileName, "another_file.png");
        compare(dialog.text, "Are you sure you want to move 'another_file.png' to the Trash?");

        // Test filename extraction without directory
        dialog.filePath = "simple_file.txt";
        compare(dialog.fileName, "simple_file.txt");

        // Test signal emission
        dialog.docId = 123;
        dialog.filePath = "/Users/test/Documents/my_awesome_file.pdf";

        let acceptedSpy = createTemporaryQmlObject("import QtTest; SignalSpy {}", this);
        acceptedSpy.target = dialog;
        acceptedSpy.signalName = "acceptedWithData";

        // Trigger accept
        dialog.accept();

        compare(acceptedSpy.count, 1, "acceptedWithData signal should be emitted once");
        let signalArgs = acceptedSpy.signalArguments[0];
        compare(signalArgs[0], 123, "First argument should be docId");
        compare(signalArgs[1], "/Users/test/Documents/my_awesome_file.pdf", "Second argument should be filePath");

        dialog.destroy();
        acceptedSpy.destroy();
    }
}
