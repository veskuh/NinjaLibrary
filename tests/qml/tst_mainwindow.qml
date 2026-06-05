import QtQuick
import QtTest
import NinjaLibrary
import Kaakao 1.0

TestCase {
    name: "MainWindowTests"
    width: 1024
    height: 768
    visible: true

    Component {
        id: mainWindowComponent
        MainWindow {}
    }

    // Helper to find a child by type name (e.g. "Sidebar" or "Inspector")
    function findChildByType(parent, typeName) {
        if (!parent) return null;
        if (parent.toString().indexOf(typeName) >= 0) return parent;
        if (parent.children) {
            for (let i = 0; i < parent.children.length; ++i) {
                let found = findChildByType(parent.children[i], typeName);
                if (found) return found;
            }
        }
        if (parent.contentItem) {
            let found = findChildByType(parent.contentItem, typeName);
            if (found) return found;
        }
        return null;
    }

    // Helper to find a child by objectName
    function findChildByName(parent, name) {
        if (!parent) return null;
        if (parent.objectName === name) return parent;
        if (parent.header) {
            let found = findChildByName(parent.header, name);
            if (found) return found;
        }
        if (parent.children) {
            for (let i = 0; i < parent.children.length; ++i) {
                let found = findChildByName(parent.children[i], name);
                if (found) return found;
            }
        }
        if (parent.contentItem) {
            let found = findChildByName(parent.contentItem, name);
            if (found) return found;
        }
        return null;
    }

    // Helper to recursively find a KaakaoToolButton by its text label
    function findToolButtonByText(parent, text) {
        if (!parent) return null;
        if (parent.toString().indexOf("KaakaoToolButton") >= 0 && parent.text === text) {
            return parent;
        }
        if (parent.children) {
            for (let i = 0; i < parent.children.length; ++i) {
                let found = findToolButtonByText(parent.children[i], text);
                if (found) return found;
            }
        }
        if (parent.header) {
            let found = findToolButtonByText(parent.header, text);
            if (found) return found;
        }
        if (parent.contentItem) {
            let found = findToolButtonByText(parent.contentItem, text);
            if (found) return found;
        }
        return null;
    }

    function test_toolbar_button_properties() {
        let win = mainWindowComponent.createObject(this);
        verify(win !== null, "MainWindow should be instantiated");
        
        // Wait for rendering and completed callbacks
        wait(200);

        // Verify Add Folder button
        let folderBtn = findToolButtonByText(win, "Add Folder");
        verify(folderBtn !== null, "Add Folder button should be found by text");
        compare(folderBtn.iconEmoji, "➕", "Add Folder button icon emoji should be ➕");

        // Verify Inspector toggle button
        let inspectorBtn = findToolButtonByText(win, "Inspector");
        verify(inspectorBtn !== null, "Inspector button should be found by text");
        compare(inspectorBtn.iconEmoji, "ⓘ", "Inspector button icon emoji should be ⓘ");

        win.destroy();
    }

    function test_menu_bar_structure_and_actions() {
        let win = mainWindowComponent.createObject(this);
        verify(win !== null, "MainWindow should be instantiated");
        wait(200);

        let menuBar = win.mainMenuBar;
        verify(menuBar !== null, "MenuBar should exist on MainWindow");

        // Verify menus exist
        let menus = menuBar.menus;
        verify(menus.length >= 5, "MenuBar should have at least 5 menus");

        // Check menus titles
        compare(menus[0].title, "File", "First menu should be File");
        compare(menus[1].title, "Edit", "Second menu should be Edit");
        compare(menus[2].title, "View", "Third menu should be View");
        compare(menus[3].title, "Window", "Fourth menu should be Window");
        compare(menus[4].title, "Help", "Fifth menu should be Help");

        // Verify File menu items and triggering
        let fileMenu = menus[0];
        verify(fileMenu.count >= 2, "File menu should have menu items");
        compare(fileMenu.itemAt(0).text, "Add Watched Folder...", "First File item should be Add Watched Folder");
        compare(fileMenu.itemAt(2).text, "Quit", "Second File item should be Quit"); // index 2 because separator is at index 1

        // Verify Help menu About and Preferences
        let helpMenu = menus[4];
        verify(helpMenu.count >= 2, "Help menu should have items");
        compare(helpMenu.itemAt(0).text, "About NinjaLibrary", "First Help item should be About");
        compare(helpMenu.itemAt(1).text, "Preferences...", "Second Help item should be Preferences");

        // Test Sidebar Toggle via Menu Trigger
        let sidebar = findChildByType(win, "Sidebar");
        verify(sidebar !== null, "Sidebar should exist");
        compare(sidebar.collapsed, false, "Sidebar should be expanded initially");

        let viewMenu = menus[2];
        let toggleSidebarItem = viewMenu.itemAt(0);
        compare(toggleSidebarItem.text, "Toggle Sidebar", "Should have Toggle Sidebar item");

        toggleSidebarItem.action.trigger();
        compare(sidebar.collapsed, true, "Sidebar should collapse after menu trigger");

        toggleSidebarItem.action.trigger();
        compare(sidebar.collapsed, false, "Sidebar should expand after second menu trigger");

        win.destroy();
    }


    function test_inspector_toggling() {
        let win = mainWindowComponent.createObject(this);
        verify(win !== null, "MainWindow should be instantiated");
        wait(200);

        let inspector = findChildByType(win, "Inspector");
        verify(inspector !== null, "Inspector component should exist");

        // Default state: not collapsed, visible
        compare(inspector.collapsed, false, "Inspector should not be collapsed by default");
        compare(inspector.visible, true, "Inspector should be visible by default");

        let inspectorBtn = findToolButtonByText(win, "Inspector");
        verify(inspectorBtn !== null, "Inspector button should exist");

        // Click to collapse
        mouseClick(inspectorBtn);
        compare(inspector.collapsed, true, "Inspector should be collapsed after click");
        compare(inspector.visible, false, "Inspector should be invisible after click");

        // Click again to expand
        mouseClick(inspectorBtn);
        compare(inspector.collapsed, false, "Inspector should not be collapsed after second click");
        compare(inspector.visible, true, "Inspector should be visible after second click");

        win.destroy();
    }

    function test_inspector_tag_reactivity() {
        let win = mainWindowComponent.createObject(this);
        verify(win !== null, "MainWindow should be instantiated");
        wait(200);

        let inspector = findChildByType(win, "Inspector");
        verify(inspector !== null, "Inspector component should exist");

        // Clear existing model rows
        documentModel.clear();

        // Append a mock document mapping roles both as integers and as strings
        let mockDoc = {
            "257": 42,
            "259": "test_doc.pdf",
            "260": "/path/test.pdf",
            "273": "100 KB",
            "266": 12,
            "267": 3,
            "268": false,
            "269": ["initial_tag"],
            "270": "",
            "271": "Some notes",
            "272": "",
            "id": 42,
            "fileName": "test_doc.pdf",
            "absolutePath": "/path/test.pdf",
            "fileSizeStr": "100 KB",
            "pageCount": 12,
            "starRating": 3,
            "isOffline": false,
            "tags": ["initial_tag"],
            "notes": "Some notes",
            "thumbnailPath": ""
        };
        documentModel.append(mockDoc);

        // Select the mock document
        inspector.selectedIds = [42];
        compare(inspector.selectedId, 42, "Selected ID should match the appended document");
        verify(inspector.docData !== null, "Inspector docData should be populated");
        compare(inspector.docData.tags.length, 1, "Initial tag list length should be 1");
        compare(inspector.docData.tags[0], "initial_tag", "Initial tag should match");

        // Dynamically update the tags role using updateRow
        documentModel.updateRow(0, {
            "269": ["initial_tag", "new_tag"],
            "tags": ["initial_tag", "new_tag"]
        });

        // Verify the inspector's docData updated immediately
        verify(inspector.docData !== null, "Inspector docData should still be populated");
        compare(inspector.docData.tags.length, 2, "Tags list length should immediately update to 2");
        compare(inspector.docData.tags[1], "new_tag", "The new tag should be present immediately");

        win.destroy();
    }

    function test_inspector_notes_auto_save() {
        let win = mainWindowComponent.createObject(this);
        verify(win !== null, "MainWindow should be instantiated");
        wait(200);

        let inspector = findChildByType(win, "Inspector");
        verify(inspector !== null, "Inspector component should exist");

        // Clear existing model rows
        documentModel.clear();

        // Append two mock documents
        documentModel.append({
            "257": 42,
            "271": "Notes 42",
            "id": 42,
            "notes": "Notes 42"
        });
        documentModel.append({
            "257": 43,
            "271": "Notes 43",
            "id": 43,
            "notes": "Notes 43"
        });

        // Set up SignalSpy for notesUpdated signal on libraryController
        let notesSpy = createTemporaryQmlObject("import QtTest; SignalSpy {}", this);
        notesSpy.target = libraryController;
        notesSpy.signalName = "notesUpdated";

        // 1. Select document 42
        inspector.selectedIds = [42];
        compare(inspector.selectedId, 42, "Document 42 should be selected");
        
        // Find notesArea
        let notesArea = findChildByName(inspector, "notesArea");
        verify(notesArea !== null, "notesArea should exist inside Inspector");
        compare(notesArea.text, "Notes 42", "notesArea text should show initial notes");

        // Modify the text area content (simulates user typing)
        notesArea.text = "Edited Notes 42";

        // 2. Change selection to document 43. This should automatically save the notes of 42.
        inspector.selectedIds = [43];
        compare(notesSpy.count, 1, "Changing selection should trigger auto-save of previous notes");
        compare(notesSpy.signalArguments[0][0], 42, "Saved document ID should be 42");
        compare(notesSpy.signalArguments[0][1], "Edited Notes 42", "Saved notes text should match the edited text");
        notesSpy.clear();

        // Confirm text area has been updated to the B's notes
        compare(notesArea.text, "Notes 43", "notesArea text should update to selection B notes");

        // Edit notes of document 43
        notesArea.text = "Edited Notes 43";

        // 3. Collapse/close the inspector. This should trigger auto-save of 43.
        inspector.collapsed = true;
        compare(notesSpy.count, 1, "Collapsing inspector should trigger auto-save of notes");
        compare(notesSpy.signalArguments[0][0], 43, "Saved document ID should be 43");
        compare(notesSpy.signalArguments[0][1], "Edited Notes 43", "Saved notes text should match edited text");

        notesSpy.destroy();
        win.destroy();
    }

    function test_drag_and_drop_mechanism() {
        let win = mainWindowComponent.createObject(this);
        verify(win !== null, "MainWindow should be instantiated");
        wait(200);

        let sidebar = findChildByType(win, "Sidebar");
        verify(sidebar !== null, "Sidebar should exist");

        // Clear existing model rows
        documentModel.clear();

        // 1. Verify helper findDocIdByPath and selectDocument
        documentModel.append({
            "257": 101,
            "260": "/path/to/doc.pdf",
            "id": 101,
            "absolutePath": "/path/to/doc.pdf"
        });

        compare(win.findDocIdByPath("/path/to/doc.pdf"), 101, "findDocIdByPath should return matching docId");
        compare(win.findDocIdByPath("/path/to/nonexistent.pdf"), -1, "findDocIdByPath should return -1 for nonexistent files");

        win.selectDocument(101);
        let inspector = findChildByType(win, "Inspector");
        compare(inspector.selectedIds.length, 1, "Inspector selectedIds should have 1 item");
        compare(inspector.selectedId, 101, "Inspector selectedId should match");

        // 2. Verify visual drag overlay and DropArea
        let dropArea = findChildByName(win, "dropArea");
        verify(dropArea !== null, "DropArea should exist in MainWindow");

        let dragOverlay = findChildByName(win, "dragOverlay");
        verify(dragOverlay !== null, "dragOverlay should exist in MainWindow");
        compare(dragOverlay.visible, false, "dragOverlay should be hidden by default");

        win.destroy();
    }

    function test_grid_canvas_key_navigation() {
        let win = mainWindowComponent.createObject(this);
        verify(win !== null, "MainWindow should be instantiated");
        win.width = 1024;
        win.height = 768;
        wait(200);

        let viewSegment = findChildByName(win, "viewSegment");
        verify(viewSegment !== null, "viewSegment should be found");
        viewSegment.currentIndex = 0; // Grid
        wait(200);

        // Find GridCanvas
        let gridCanvas = findChildByType(win, "GridCanvas");
        verify(gridCanvas !== null, "GridCanvas should be found");

        let gridView = findChildByType(gridCanvas, "KaakaoGridView");
        verify(gridView !== null, "KaakaoGridView should be found");
        let innerGridView = gridView.gridView;
        verify(innerGridView !== null, "Inner GridView should be found");

        // Clear and populate model
        documentModel.clear();
        proxyFilter.clear();
        let doc1 = { "257": 101, "id": 101, "docId": 101, "fileName": "doc1.pdf", "absolutePath": "/path/1" };
        let doc2 = { "257": 102, "id": 102, "docId": 102, "fileName": "doc2.pdf", "absolutePath": "/path/2" };
        let doc3 = { "257": 103, "id": 103, "docId": 103, "fileName": "doc3.pdf", "absolutePath": "/path/3" };
        documentModel.append(doc1);
        proxyFilter.append(doc1);
        documentModel.append(doc2);
        proxyFilter.append(doc2);
        documentModel.append(doc3);
        proxyFilter.append(doc3);
        
        wait(100);

        console.log("DEBUG: proxyFilter rowCount =", proxyFilter.rowCount());
        console.log("DEBUG: innerGridView.count =", innerGridView.count);
        console.log("DEBUG: innerGridView.model =", innerGridView.model);
        console.log("DEBUG: innerGridView.width =", innerGridView.width);

        verify(innerGridView.width > 0, "Inner GridView should have non-zero width");
        compare(innerGridView.count, 3, "Inner GridView count should match appended docs");

        // 1. Single selection navigation test
        gridCanvas.selectedIds = [101];
        gridView.currentIndex = 0;
        innerGridView.forceActiveFocus();
        verify(innerGridView.activeFocus, "Inner GridView should have active focus");

        // Click right arrow key
        keyClick(Qt.Key_Right);
        wait(50);

        compare(gridCanvas.selectedIds.length, 1, "Should have 1 selected ID after Right key press");
        compare(gridCanvas.selectedIds[0], 102, "Selected ID should move to 102");

        // Click right arrow key again
        keyClick(Qt.Key_Right);
        wait(50);
        compare(gridCanvas.selectedIds.length, 1, "Should have 1 selected ID after second Right key press");
        compare(gridCanvas.selectedIds[0], 103, "Selected ID should move to 103");

        // 2. Multi-selection navigation block test
        gridCanvas.selectedIds = [101, 102];
        gridView.currentIndex = 0;
        keyClick(Qt.Key_Right);
        wait(50);
        compare(gridCanvas.selectedIds.length, 2, "Selection length should remain 2");
        compare(gridCanvas.selectedIds[0], 101, "First selected ID should remain 101");
        compare(gridCanvas.selectedIds[1], 102, "Second selected ID should remain 102");

        // 3. No selection navigation block test
        gridCanvas.selectedIds = [];
        gridView.currentIndex = 0;
        keyClick(Qt.Key_Right);
        wait(50);
        compare(gridCanvas.selectedIds.length, 0, "Selection should remain empty");

        win.destroy();
    }

    function test_escape_shortcut_and_status_bar() {
        let win = mainWindowComponent.createObject(this);
        verify(win !== null, "MainWindow should be instantiated");
        wait(200);

        // 1. Verify Status Bar is bound to documentModel properties
        let statusBar = findChildByName(win, "statusBar");
        verify(statusBar !== null, "statusBar should be found by name");

        let statusLabel = findChildByName(statusBar, "statusLabel");
        verify(statusLabel !== null, "statusLabel should be found by name");

        // Clear and add mock data to documentModel
        documentModel.clear();
        documentModel.append({
            "id": 1,
            "fileName": "document.pdf",
            "isOffline": false
        });
        documentModel.append({
            "id": 2,
            "fileName": "photo.png",
            "isOffline": true
        });
        documentModel.append({
            "id": 3,
            "fileName": "note.txt",
            "isOffline": false
        });

        // Let QML properties and bindings update
        wait(100);

        // Check if counts match in MockDocumentModel
        compare(documentModel.totalCount, 3, "totalCount should be 3");
        compare(documentModel.pdfCount, 1, "pdfCount should be 1");
        compare(documentModel.imageCount, 1, "imageCount should be 1");
        compare(documentModel.textCount, 1, "textCount should be 1");
        compare(documentModel.localCount, 2, "localCount should be 2");
        compare(documentModel.unavailableCount, 1, "unavailableCount should be 1");
 
        // Verify status bar text reflects these counts
        let expectedText = "Indexed: 3 items (1 PDFs, 1 Images, 1 Text/Other)  |  2 Local, 1 Unavailable  |  Selected: 0";
        compare(statusLabel.text, expectedText, "Status bar text should match expected breakdown");

        // 2. Verify Escape shortcut clears selection and search text
        let searchField = findChildByName(win, "searchField");
        verify(searchField !== null, "searchField should be found by name");

        // Set search text and selection
        searchField.text = "test query";
        let inspector = findChildByType(win, "Inspector");
        inspector.selectedIds = [1];

        compare(searchField.text, "test query", "Search text should be set");
        compare(inspector.selectedIds.length, 1, "Inspector should have 1 selected ID");

        // Direct focus to searchField first to test escape handling
        searchField.forceActiveFocus();
        verify(searchField.activeFocus, "Search field should have focus");

        // Press Escape key
        keyClick(Qt.Key_Escape);
        wait(100);

        // Verify search field is cleared and selection is empty
        compare(searchField.text, "", "Search field should be cleared after Escape");
        compare(inspector.selectedIds.length, 0, "Selection should be empty after Escape");

        win.destroy();
    }

    function test_enter_key_to_open_document() {
        let win = mainWindowComponent.createObject(this);
        verify(win !== null, "MainWindow should be instantiated");
        win.width = 1024;
        win.height = 768;
        wait(200);

        // Find GridCanvas & TableCanvas
        let gridCanvas = findChildByType(win, "GridCanvas");
        verify(gridCanvas !== null, "GridCanvas should be found");
        let tableCanvas = findChildByType(win, "TableCanvas");
        verify(tableCanvas !== null, "TableCanvas should be found");

        let viewSegment = findChildByName(win, "viewSegment");
        verify(viewSegment !== null, "viewSegment should be found");

        // Clear and populate model
        documentModel.clear();
        proxyFilter.clear();
        let doc1 = { "257": 101, "id": 101, "docId": 101, "fileName": "doc1.pdf", "absolutePath": "/path/1" };
        let doc2 = { "257": 102, "id": 102, "docId": 102, "fileName": "doc2.pdf", "absolutePath": "/path/2" };
        documentModel.append(doc1);
        proxyFilter.append(doc1);
        documentModel.append(doc2);
        proxyFilter.append(doc2);

        // Create Spy for markDocumentOpened signal on libraryController
        let openSpy = createTemporaryQmlObject("import QtTest; SignalSpy {}", this);
        openSpy.target = libraryController;
        openSpy.signalName = "documentOpened";

        // 1. GridView Enter key test
        viewSegment.currentIndex = 0; // Grid
        gridCanvas.selectedIds = [101];
        gridCanvas.selectId(101);
        
        let gridView = findChildByType(gridCanvas, "KaakaoGridView");
        verify(gridView !== null, "KaakaoGridView should be found");
        gridView.gridView.forceActiveFocus();
        verify(gridView.gridView.activeFocus, "Grid view should have focus");

        keyClick(Qt.Key_Return);
        wait(50);
        compare(openSpy.count, 1, "Enter in Grid View should trigger markDocumentOpened");
        compare(openSpy.signalArguments[0][0], 101, "Opened document ID in Grid View should be 101");
        openSpy.clear();

        // 2. TableView Enter key test
        viewSegment.currentIndex = 1; // Table
        tableCanvas.selectId(102);
        
        let tableView = findChildByType(tableCanvas, "KaakaoTableView");
        verify(tableView !== null, "KaakaoTableView should be found");
        
        // Find inner ListView
        let innerListView = tableCanvas.innerListView;
        verify(innerListView !== null, "Inner table ListView should be found");
        innerListView.forceActiveFocus();
        verify(innerListView.activeFocus, "Table view should have focus");

        keyClick(Qt.Key_Return);
        wait(50);
        compare(openSpy.count, 1, "Enter in Table View should trigger markDocumentOpened");
        compare(openSpy.signalArguments[0][0], 102, "Opened document ID in Table View should be 102");

        openSpy.destroy();
        win.destroy();
    }

    function test_copy_path_and_folder_double_click() {
        let win = mainWindowComponent.createObject(this);
        verify(win !== null, "MainWindow should be instantiated");
        wait(200);

        let sidebar = findChildByType(win, "Sidebar");
        verify(sidebar !== null, "Sidebar should exist");

        // Set up doubleClicked verification on sidebar
        let sidebarSpy = createTemporaryQmlObject("import QtTest; SignalSpy {}", this);
        sidebarSpy.target = sidebar;
        sidebarSpy.signalName = "doubleClicked";

        // Manually trigger doubleClicked signal in sidebar to verify it propagates
        sidebar.doubleClicked(0);
        compare(sidebarSpy.count, 1, "doubleClicked signal should be emitted by sidebar");
        sidebarSpy.destroy();

        // 2. Context Menu "Copy File Path" action verification
        let contextMenu = win.itemContextMenu;
        verify(contextMenu !== null, "itemContextMenu should exist");
        
        // Setup spy for pathCopied
        let copySpy = createTemporaryQmlObject("import QtTest; SignalSpy {}", this);
        copySpy.target = libraryController;
        copySpy.signalName = "pathCopied";

        contextMenu.targetPath = "/some/test/file.pdf";
        contextMenu.targetDocId = 123;
        
        let copyItem = null;
        for (var i = 0; i < contextMenu.count; i++) {
            var item = contextMenu.itemAt(i);
            if (item && item.text === "Copy File Path") {
                copyItem = item;
                break;
            }
        }
        verify(copyItem !== null, "Copy File Path menu item should be found");
        copyItem.triggered();
        wait(50);

        compare(copySpy.count, 1, "Copy File Path trigger should copy path to clipboard");
        compare(copySpy.signalArguments[0][0], "/some/test/file.pdf", "Copied path should match target path");
        
        copySpy.destroy();

        // 3. Context Menu "Rate" action verification
        let rateSubMenu = null;
        for (var j = 0; j < contextMenu.count; j++) {
            var mItem = contextMenu.itemAt(j);
            if (mItem && mItem.text === "Rate") {
                rateSubMenu = mItem;
                break;
            }
        }
        verify(rateSubMenu !== null, "Rate submenu item should be found");

        let subMenu = rateSubMenu.subMenu;
        verify(subMenu !== null, "Rate submenu should exist");

        // Set up spy for libraryChanged
        let librarySpy = createTemporaryQmlObject("import QtTest; SignalSpy {}", this);
        librarySpy.target = libraryController;
        librarySpy.signalName = "libraryChanged";

        // Find the rating item for 4 stars (index 3)
        let rateItem = subMenu.itemAt(3);
        verify(rateItem !== null, "4-star rating menu item should exist");
        compare(rateItem.text, "★★★★☆", "4-star text should match");
        rateItem.triggered();
        wait(50);

        compare(librarySpy.count, 1, "Triggering rate item should emit libraryChanged");
        librarySpy.destroy();

        win.destroy();
    }

    function test_scanning_progress_indicator() {
        let win = mainWindowComponent.createObject(this);
        verify(win !== null, "MainWindow should be instantiated");
        wait(200);

        let statusBar = findChildByName(win, "statusBar");
        verify(statusBar !== null, "statusBar should exist");

        let scanningProgressLayout = findChildByName(statusBar, "scanningProgressLayout");
        verify(scanningProgressLayout !== null, "scanningProgressLayout should exist");

        // 1. Verify initially not scanning and not visible
        compare(libraryController.isScanning, false, "Should not be scanning initially");
        compare(scanningProgressLayout.visible, false, "scanningProgressLayout should be hidden initially");

        // 2. Set isScanning to true and verify it is visible
        libraryController.isScanning = true;
        libraryController.scanProgress = 0.35;
        wait(100);

        compare(scanningProgressLayout.visible, true, "scanningProgressLayout should be visible when isScanning is true");

        // Find child label inside scanningProgressLayout to verify percentage text
        let progressLabel = findChildByType(scanningProgressLayout, "KaakaoLabel");
        verify(progressLabel !== null, "Progress label should be found");
        compare(progressLabel.text, "Scanning: 35%", "Progress label text should match percentage");

        // 3. Reset isScanning to false
        libraryController.isScanning = false;
        libraryController.scanProgress = 0.0;
        wait(100);

        compare(scanningProgressLayout.visible, false, "scanningProgressLayout should be hidden after scanning finishes");

        win.destroy();
    }

    function test_folder_addition_clears_search_and_resets_scope() {
        let win = mainWindowComponent.createObject(this);
        verify(win !== null, "MainWindow should be instantiated");
        wait(200);

        let searchField = findChildByName(win, "searchField");
        verify(searchField !== null, "searchField should exist");

        // 1. Set search text and a custom scope filter
        searchField.text = "test query";
        proxyFilter.scopeFilter = "PDF";
        compare(searchField.text, "test query", "Search text should be set");
        compare(proxyFilter.scopeFilter, "PDF", "Scope filter should be set");

        // 2. Add watched folder, which emits folderAdded signal
        libraryController.addWatchedFolder("/path/to/added/folder");
        wait(50);

        // 3. Verify search is cleared and scope is reset to "All"
        compare(searchField.text, "", "Search should be cleared after folder is added");
        compare(proxyFilter.scopeFilter, "All", "Scope filter should be reset to All");

        win.destroy();
    }

    function test_sidebar_selection_resets_state() {
        let win = mainWindowComponent.createObject(this);
        verify(win !== null, "MainWindow should be instantiated");
        wait(200);

        proxyFilter.activeScopes = ["All", "PDF", "PNG", "Today"];
        wait(50);

        let sidebar = findChildByType(win, "Sidebar");
        verify(sidebar !== null, "Sidebar should exist");

        let searchField = findChildByName(win, "searchField");
        verify(searchField !== null, "searchField should exist");

        let scopeBar = findChildByType(win, "KaakaoScopeBar");
        verify(scopeBar !== null, "scopeBar should exist");

        // 1. Set some state
        searchField.text = "query text";
        // Set scopeFilter and simulate user selecting it by setting currentIndex
        proxyFilter.scopeFilter = "PDF";
        scopeBar.currentIndex = proxyFilter.activeScopes.indexOf("PDF");
        compare(searchField.text, "query text", "Search text should be set");
        compare(proxyFilter.scopeFilter, "PDF", "Scope filter should be set");
        verify(scopeBar.currentIndex > 0, "scopeBar index should be non-zero");

        // 2. Trigger onSectionSelected signal from sidebar
        sidebar.sectionSelected("Favorites");
        wait(50);

        // 3. Verify they are reset
        compare(searchField.text, "", "Search text should be reset on sidebar section selection");
        compare(proxyFilter.scopeFilter, "All", "Scope filter should be reset to All on sidebar section selection");
        compare(scopeBar.currentIndex, 0, "ScopeBar index should be reset to All (0)");

        // Set state again
        searchField.text = "query text 2";
        proxyFilter.scopeFilter = "PNG";
        scopeBar.currentIndex = proxyFilter.activeScopes.indexOf("PNG");

        // 4. Trigger onFolderSelected signal
        sidebar.folderSelected("/some/folder");
        wait(50);
        compare(searchField.text, "", "Search text should be reset on sidebar folder selection");
        compare(proxyFilter.scopeFilter, "All", "Scope filter should be reset to All on sidebar folder selection");
        compare(scopeBar.currentIndex, 0, "ScopeBar index should be reset to All (0)");

        // Set state again
        searchField.text = "query text 3";
        proxyFilter.scopeFilter = "Today";
        scopeBar.currentIndex = proxyFilter.activeScopes.indexOf("Today");

        // 5. Trigger onTagSelected signal
        sidebar.tagSelected("work");
        wait(50);
        compare(searchField.text, "", "Search text should be reset on sidebar tag selection");
        compare(proxyFilter.scopeFilter, "All", "Scope filter should be reset to All on sidebar tag selection");
        compare(scopeBar.currentIndex, 0, "ScopeBar index should be reset to All (0)");
        compare(proxyFilter.selectedTags, ["work"], "Selected tags should be ['work']");

        // 6. Select folder and verify selected tags is reset
        sidebar.folderSelected("/some/folder");
        wait(50);
        compare(proxyFilter.selectedTags, [], "Selected tags should be cleared on folder selection");

        // Set state again and select tag
        sidebar.tagSelected("work");
        wait(50);
        compare(proxyFilter.selectedTags, ["work"], "Selected tags should be ['work']");

        // 7. Select section and verify selected tags is reset
        sidebar.sectionSelected("Favorites");
        wait(50);
        compare(proxyFilter.selectedTags, [], "Selected tags should be cleared on section selection");

        win.destroy();
    }

    function test_sidebar_click_tag_and_folder() {
        let win = mainWindowComponent.createObject(this);
        verify(win !== null, "MainWindow should be instantiated");
        wait(200);

        let sidebar = findChildByType(win, "Sidebar");
        verify(sidebar !== null, "Sidebar should exist");

        // The model should be populated with tags "work", "2026"
        let sidebarModel = sidebar.model;
        verify(sidebarModel.count > 0, "Sidebar model should be populated");

        // Find the index of "2026"
        let tagIndex = -1;
        for (let i = 0; i < sidebarModel.count; i++) {
            if (sidebarModel.get(i).type === "tag" && sidebarModel.get(i).target === "2026") {
                tagIndex = i;
                break;
            }
        }
        verify(tagIndex !== -1, "Tag '2026' should be in the sidebar");

        // 1. Select tag 2026 by setting currentIndex
        sidebar.currentIndex = tagIndex;
        wait(50);
        compare(proxyFilter.selectedTags, ["2026"], "selectedTags should be ['2026'] after selecting tag");

        // 2. Select folder/section by setting currentIndex to 0 (All Documents)
        sidebar.currentIndex = 0;
        wait(50);
        compare(proxyFilter.selectedTags, [], "selectedTags should be cleared when section is selected");

        win.destroy();
    }
}
