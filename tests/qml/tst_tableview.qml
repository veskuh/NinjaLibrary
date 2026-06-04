import QtQuick
import QtTest
import NinjaLibrary
import Kaakao 1.0
import "../../src/components"

TestCase {
    name: "TableViewTests"
    width: 640
    height: 480
    visible: true

    ListModel {
        id: dataModel
        ListElement { fileName: "fileA.pdf"; fileSizeStr: "100 KB"; pageCount: 12; starRatingStr: "★★★☆☆"; isOffline: false }
        ListElement { fileName: "fileB.png"; fileSizeStr: "200 KB"; pageCount: 1; starRatingStr: "★☆☆☆☆"; isOffline: true }
        ListElement { fileName: "fileC.pdf"; fileSizeStr: "300 KB"; pageCount: 45; starRatingStr: "★★★★★"; isOffline: false }
    }

    ListModel {
        id: mockModel
        ListElement { fileName: "fileA.pdf"; fileSizeStr: "100 KB"; pageCount: 12; starRatingStr: "★★★☆☆"; isOffline: false }
        ListElement { fileName: "fileB.png"; fileSizeStr: "200 KB"; pageCount: 1; starRatingStr: "★☆☆☆☆"; isOffline: true }
        ListElement { fileName: "fileC.pdf"; fileSizeStr: "300 KB"; pageCount: 45; starRatingStr: "★★★★★"; isOffline: false }

        function get(row, role) {
            if (row >= 0 && row < dataModel.count) {
                let item = dataModel.get(row)
                if (item) {
                    if (role === "fileName") return item.fileName
                    if (role === "fileSizeStr") return item.fileSizeStr
                    if (role === "pageCount") return item.pageCount
                    if (role === "starRatingStr") return item.starRatingStr
                    if (role === "isOffline") return item.isOffline
                }
            }
            return ""
        }
    }

    Component {
        id: tableViewComponent
        KaakaoTableView {
            id: tableView
            width: 400
            height: 300
            
            Component.onCompleted: {
                let lv = findListView(tableView)
                if (lv) {
                    lv.delegate = testRowDelegateComponent
                }
            }

            function findListView(parent) {
                if (!parent) return null;
                if (parent.toString().indexOf("ListView") >= 0) return parent;
                if (parent.children) {
                    for (let i = 0; i < parent.children.length; ++i) {
                        let found = findListView(parent.children[i]);
                        if (found) return found;
                    }
                }
                return null;
            }

            Component {
                id: testRowDelegateComponent
                LocalTableRowDelegate {
                    columns: tableView.columns
                }
            }
            
            columns: [
                KaakaoTableColumn {
                    role: "fileName"
                    title: "Name"
                    width: 150
                },
                KaakaoTableColumn {
                    role: "fileSizeStr"
                    title: "Size"
                    width: 80
                }
            ]
        }
    }

    // Helper to find a Label inside table rows
    function findLabelByText(parent, text) {
        if (!parent) return null;
        if (parent.toString().indexOf("Label") >= 0 && parent.text === text) return parent;
        if (parent.children) {
            for (let i = 0; i < parent.children.length; ++i) {
                let found = findLabelByText(parent.children[i], text);
                if (found) return found;
            }
        }
        return null;
    }

    function test_table_view_different_rows() {
        let tableView = tableViewComponent.createObject(this)
        verify(tableView !== null, "TableView should be created")

        tableView.model = mockModel

        // Wait for ListView to lay out and create delegates
        wait(200)

        // Find the inner ListView inside KaakaoTableView
        let listView = null
        for (let i = 0; i < tableView.children.length; ++i) {
            let child = tableView.children[i];
            // Find the Column layout
            if (child.children) {
                for (let j = 0; j < child.children.length; ++j) {
                    let inner = child.children[j];
                    if (inner.toString().indexOf("ListView") >= 0) {
                        listView = inner;
                        break;
                    }
                }
            }
        }

        verify(listView !== null, "Inner ListView should be found")
        compare(listView.count, 3, "ListView should have 3 items")

        // Wait a bit more for rendering
        wait(100)

        // Verify that the first delegate displays "fileA.pdf" and the second displays "fileB.png"
        let foundA = false
        let foundB = false
        let foundC = false

        // Search the text in row delegates
        // ListView's contentItem contains the instantiated delegates
        let contentItem = listView.contentItem
        verify(contentItem !== null, "contentItem should exist")

        // Check each row delegate's child labels to verify distinct data
        for (let i = 0; i < contentItem.children.length; ++i) {
            let delegate = contentItem.children[i];
            // Check if it's a KaakaoTableRowDelegate or ItemDelegate
            if (delegate.toString().indexOf("TableRowDelegate") >= 0) {
                let label = findLabelByText(delegate, "fileA.pdf")
                if (label) foundA = true

                label = findLabelByText(delegate, "fileB.png")
                if (label) foundB = true

                label = findLabelByText(delegate, "fileC.pdf")
                if (label) foundC = true
            }
        }

        verify(foundA, "Row for fileA.pdf should be rendered with correct text")
        verify(foundB, "Row for fileB.png should be rendered with correct text")
        verify(foundC, "Row for fileC.pdf should be rendered with correct text")

        tableView.destroy()
    }

    function test_table_view_offline_color() {
        let tableView = tableViewComponent.createObject(this)
        verify(tableView !== null, "TableView should be created")

        tableView.model = mockModel

        // Wait for ListView to lay out and create delegates
        wait(200)

        // Find the inner ListView
        let listView = null
        for (let i = 0; i < tableView.children.length; ++i) {
            let child = tableView.children[i];
            if (child.children) {
                for (let j = 0; j < child.children.length; ++j) {
                    let inner = child.children[j];
                    if (inner.toString().indexOf("ListView") >= 0) {
                        listView = inner;
                        break;
                    }
                }
            }
        }
        verify(listView !== null, "Inner ListView should be found")

        let contentItem = listView.contentItem
        verify(contentItem !== null, "contentItem should exist")

        let offlineRowChecked = false
        let onlineRowChecked = false

        for (let i = 0; i < contentItem.children.length; ++i) {
            let delegate = contentItem.children[i];
            if (delegate.toString().indexOf("TableRowDelegate") >= 0) {
                let label = findLabelByText(delegate, "fileB.png") // This is offline: true
                if (label) {
                    compare(label.color, Qt.color("#8e8e93"), "Offline row text should be styled as #8e8e93")
                    offlineRowChecked = true
                }
                
                label = findLabelByText(delegate, "fileA.pdf") // This is offline: false
                if (label) {
                    verify(label.color !== Qt.color("#8e8e93"), "Online row text should not be styled as #8e8e93")
                    onlineRowChecked = true
                }
            }
        }

        verify(offlineRowChecked, "Should have verified the offline row color")
        verify(onlineRowChecked, "Should have verified the online row color")

        tableView.destroy()
    }

    function test_table_view_sorting() {
        let tableView = tableViewComponent.createObject(this)
        verify(tableView !== null, "TableView should be created")

        tableView.model = mockModel

        // Wait for ListView to lay out and create delegates
        wait(200)

        // Find the inner ListView
        let listView = null
        for (let i = 0; i < tableView.children.length; ++i) {
            let child = tableView.children[i];
            if (child.children) {
                for (let j = 0; j < child.children.length; ++j) {
                    let inner = child.children[j];
                    if (inner.toString().indexOf("ListView") >= 0) {
                        listView = inner;
                        break;
                    }
                }
            }
        }
        verify(listView !== null, "Inner ListView should be found")

        // Originally: fileA.pdf is row 0, fileB.png is row 1, fileC.pdf is row 2
        // Let's swap row 0 and row 2 in the mockModel (simulate sorting)
        dataModel.move(2, 0, 1)
        mockModel.move(2, 0, 1) // Move fileC (index 2) to index 0

        wait(100)

        // Verify that the delegate at index 0 now displays "fileC.pdf" and index 1 displays "fileA.pdf"
        let contentItem = listView.contentItem
        let row0Text = ""
        let row1Text = ""

        for (let i = 0; i < contentItem.children.length; ++i) {
            let delegate = contentItem.children[i];
            if (delegate.toString().indexOf("TableRowDelegate") >= 0) {
                let idx = delegate.rowIndex
                if (idx === 0) {
                    let label = findLabelByText(delegate, "fileC.pdf")
                    if (label) row0Text = "fileC.pdf"
                } else if (idx === 1) {
                    let label = findLabelByText(delegate, "fileA.pdf")
                    if (label) row1Text = "fileA.pdf"
                }
            }
        }

        compare(row0Text, "fileC.pdf", "Row 0 should update to fileC.pdf after move")
        compare(row1Text, "fileA.pdf", "Row 1 should update to fileA.pdf after move")

        // Restore mockModel order
        dataModel.move(0, 2, 1)
        mockModel.move(0, 2, 1)

        tableView.destroy()
    }

    function test_table_view_click_selection() {
        let tableView = tableViewComponent.createObject(this)
        verify(tableView !== null, "TableView should be created")
        tableView.model = mockModel
        wait(200)

        let listView = null
        for (let i = 0; i < tableView.children.length; ++i) {
            let child = tableView.children[i];
            if (child.children) {
                for (let j = 0; j < child.children.length; ++j) {
                    let inner = child.children[j];
                    if (inner.toString().indexOf("ListView") >= 0) {
                        listView = inner;
                        break;
                    }
                }
            }
        }
        verify(listView !== null, "Inner ListView should be found")

        let contentItem = listView.contentItem
        verify(contentItem !== null, "contentItem should exist")

        let delegate1 = null
        for (let i = 0; i < contentItem.children.length; ++i) {
            let delegate = contentItem.children[i];
            if (delegate.toString().indexOf("TableRowDelegate") >= 0 && delegate.rowIndex === 1) {
                delegate1 = delegate
                break
            }
        }
        verify(delegate1 !== null, "Delegate at index 1 should be found")

        // Click the delegate
        mouseClick(delegate1)
        wait(50)

        compare(listView.currentIndex, 1, "ListView currentIndex should change to 1 after click")
        tableView.destroy()
    }
}
