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
        id: mockModel
        ListElement { fileName: "fileA.pdf"; fileSizeStr: "100 KB"; pageCount: 12; starRatingStr: "★★★☆☆"; offlineColor: "green" }
        ListElement { fileName: "fileB.png"; fileSizeStr: "200 KB"; pageCount: 1; starRatingStr: "★☆☆☆☆"; offlineColor: "red" }
        ListElement { fileName: "fileC.pdf"; fileSizeStr: "300 KB"; pageCount: 45; starRatingStr: "★★★★★"; offlineColor: "green" }

        function get(row, role) {
            if (row === 0) {
                if (role === "fileName") return "fileA.pdf";
                if (role === "fileSizeStr") return "100 KB";
                if (role === "pageCount") return 12;
                if (role === "starRatingStr") return "★★★☆☆";
                if (role === "offlineColor") return "green";
            } else if (row === 1) {
                if (role === "fileName") return "fileB.png";
                if (role === "fileSizeStr") return "200 KB";
                if (role === "pageCount") return 1;
                if (role === "starRatingStr") return "★☆☆☆☆";
                if (role === "offlineColor") return "red";
            } else if (row === 2) {
                if (role === "fileName") return "fileC.pdf";
                if (role === "fileSizeStr") return "300 KB";
                if (role === "pageCount") return 45;
                if (role === "starRatingStr") return "★★★★★";
                if (role === "offlineColor") return "green";
            }
            return "";
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
                    role: "offlineColor"
                    title: "Status"
                    width: 50
                    showAsIndicator: true
                },
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
}
