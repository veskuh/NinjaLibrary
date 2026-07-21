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

#include <QSignalSpy>
#include <QTest>

#include "../../src/database/DatabaseManager.h"
#include "../../src/models/DocumentModel.h"
#include "../../src/models/ProxyFilter.h"

class TestModels : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();
    void testModelFiltering();
    void testProxyFilterGet();
    void testProxyFilterGetBounds();
    void testMultiTermSearch();
    void testScopeFiltering();
    void testSorting();
    void testRecentCategory();
    void testTagSelectionAndClearing();
    void testModelChangeScopeDebounce();
    void testSearchMatchDebounce();

private:
    DatabaseManager *m_dbMgr;
};

void TestModels::initTestCase()
{
    m_dbMgr = new DatabaseManager(":memory:", this);
    QVERIFY(m_dbMgr->initializeDatabase());

    // Populates test items in DB
    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    QSqlQuery query(db);

    QVERIFY(query.exec("INSERT INTO watched_folders (absolute_path) VALUES ('/test/docs');"));
    int folderId = query.lastInsertId().toInt();

    // Doc 1: File A, size 100, hash123, rating 5, online, tags: work, important
    QVERIFY(query.exec(QString("INSERT INTO documents (folder_id, file_name, absolute_path, "
                               "file_size, file_hash, star_rating, is_offline, date_modified) "
                               "VALUES (%1, 'fileA.pdf', '/test/docs/fileA.pdf', 100, 'hash123', "
                               "5, 0, datetime('now', 'localtime'));")
                           .arg(folderId)));
    int docAId = query.lastInsertId().toInt();

    // Doc 2: File B, size 200, hash456, rating 3, online, tags: work, personal
    QVERIFY(query.exec(QString("INSERT INTO documents (folder_id, file_name, absolute_path, "
                               "file_size, file_hash, star_rating, is_offline, date_modified) "
                               "VALUES (%1, 'fileB.png', '/test/docs/fileB.png', 200, 'hash456', "
                               "3, 0, datetime('now', 'localtime'));")
                           .arg(folderId)));
    int docBId = query.lastInsertId().toInt();

    // Doc 3: File C, size 100, hash123, rating 2, offline, tags: personal (shares hash123 with File
    // A -> Duplicate!)
    QVERIFY(query.exec(QString("INSERT INTO documents (folder_id, file_name, absolute_path, "
                               "file_size, file_hash, star_rating, is_offline, date_modified) "
                               "VALUES (%1, 'fileC.pdf', '/test/docs/fileC.pdf', 100, 'hash123', "
                               "2, 1, datetime('now', 'localtime'));")
                           .arg(folderId)));
    int docCId = query.lastInsertId().toInt();

    // Insert FTS text
    bool okA = query.exec(
        QString("INSERT INTO document_search (rowid, file_name, text_snippet, notes) VALUES "
                "(%1, 'fileA.pdf', 'Hello world PDF text.', 'Boss notes');")
            .arg(docAId));
    if (!okA) qWarning() << "Error A:" << query.lastError().text();
    QVERIFY(okA);
    QVERIFY(query.exec(QString("INSERT INTO document_search (rowid, file_name, text_snippet, "
                               "notes) VALUES (%1, 'fileB.png', 'Scanned image content.', '');")
                           .arg(docBId)));
    QVERIFY(query.exec(QString("INSERT INTO document_search (rowid, file_name, text_snippet, "
                               "notes) VALUES (%1, 'fileC.pdf', 'Hello duplicate test.', 'Clone');")
                           .arg(docCId)));

    // Insert Tags
    QVERIFY(query.exec("INSERT INTO tags (id, name) VALUES (1, 'work');"));
    QVERIFY(query.exec("INSERT INTO tags (id, name) VALUES (2, 'important');"));
    QVERIFY(query.exec("INSERT INTO tags (id, name) VALUES (3, 'personal');"));

    // Link Tags
    QVERIFY(query.exec(
        QString("INSERT INTO document_tags (document_id, tag_id) VALUES (%1, 1);").arg(docAId)));
    QVERIFY(query.exec(
        QString("INSERT INTO document_tags (document_id, tag_id) VALUES (%1, 2);").arg(docAId)));

    QVERIFY(query.exec(
        QString("INSERT INTO document_tags (document_id, tag_id) VALUES (%1, 1);").arg(docBId)));
    QVERIFY(query.exec(
        QString("INSERT INTO document_tags (document_id, tag_id) VALUES (%1, 3);").arg(docBId)));

    QVERIFY(query.exec(
        QString("INSERT INTO document_tags (document_id, tag_id) VALUES (%1, 3);").arg(docCId)));
}

void TestModels::cleanupTestCase() { delete m_dbMgr; }

void TestModels::testModelFiltering()
{
    // Test base DocumentModel loads all 3 documents
    DocumentModel sourceModel(m_dbMgr);
    QTRY_COMPARE(sourceModel.rowCount(), 3);
    QTRY_COMPARE(sourceModel.rowCount(), 3);

    // Test ProxyFilter
    ProxyFilter proxyFilter(m_dbMgr);
    proxyFilter.setSourceModel(&sourceModel);
    proxyFilter.setShowUnavailable(true);

    // Initial count with showOffline = true
    QTRY_COMPARE(proxyFilter.rowCount(), 3);

    // Test offline hiding
    proxyFilter.setShowUnavailable(false);
    QTRY_COMPARE(proxyFilter.rowCount(), 2);  // File A & B are online
    proxyFilter.setShowUnavailable(true);

    // Test tag intersection (AND filtering)
    // Filter tag "work" (matches File A, File B)
    proxyFilter.setSelectedTags(QStringList{"work"});
    QTRY_COMPARE(proxyFilter.rowCount(), 2);

    // Filter tags "work" AND "important" (matches File A only)
    proxyFilter.setSelectedTags(QStringList{"work", "important"});
    QTRY_COMPARE(proxyFilter.rowCount(), 1);
    QCOMPARE(proxyFilter.data(proxyFilter.index(0, 0), DocumentModel::FileNameRole).toString(),
             QString("fileA.pdf"));

    // Reset tags filter
    proxyFilter.setSelectedTags(QStringList{});
    QTRY_COMPARE(proxyFilter.rowCount(), 3);

    // Test Rating filter >= 4 (File A = 5, matches)
    proxyFilter.setMinRating(4);
    QTRY_COMPARE(proxyFilter.rowCount(), 1);
    QCOMPARE(proxyFilter.data(proxyFilter.index(0, 0), DocumentModel::FileNameRole).toString(),
             QString("fileA.pdf"));
    proxyFilter.setMinRating(0);

    // Test Duplicate filter (File A and C share 'hash123')
    proxyFilter.setDuplicatesOnly(true);
    QTRY_COMPARE(proxyFilter.rowCount(), 2);
    proxyFilter.setDuplicatesOnly(false);

    // Test FTS5 search (debounced search update)
    // Search "world" (matches File A)
    proxyFilter.setFilterString("world");
    QTRY_VERIFY(proxyFilter.rowCount() == 1 && proxyFilter.data(proxyFilter.index(0, 0), DocumentModel::FileNameRole).toString() == "fileA.pdf");

    // Search "duplicate" (matches File C)
    proxyFilter.setFilterString("duplicate");
    QTRY_VERIFY(proxyFilter.rowCount() == 1 && proxyFilter.data(proxyFilter.index(0, 0), DocumentModel::FileNameRole).toString() == "fileC.pdf");

    proxyFilter.setFilterString("");
    QTRY_COMPARE(proxyFilter.rowCount(), 3);
}

void TestModels::testProxyFilterGet()
{
    // Verify that ProxyFilter::get(row, roleName) returns correct per-row data.
    // This is the mechanism KaakaoTableView delegates use to retrieve cell values.
    DocumentModel sourceModel(m_dbMgr);
    QTRY_COMPARE(sourceModel.rowCount(), 3);
    ProxyFilter proxyFilter(m_dbMgr);
    proxyFilter.setSourceModel(&sourceModel);
    proxyFilter.setShowUnavailable(true);

    // Call header getters to ensure code coverage
    QCOMPARE(proxyFilter.filterString(), QString(""));
    QCOMPARE(proxyFilter.selectedTags(), QStringList());
    QCOMPARE(proxyFilter.minRating(), 0);
    QCOMPARE(proxyFilter.showUnavailable(), true);
    QCOMPARE(proxyFilter.duplicatesOnly(), false);
    QCOMPARE(proxyFilter.categoryFilter(), QString("All"));
    QCOMPARE(proxyFilter.folderFilter(), QString(""));

    QTRY_COMPARE(proxyFilter.rowCount(), 3);

    // Collect all file names via get() — each row must return a distinct name
    QStringList names;
    for (int i = 0; i < proxyFilter.rowCount(); ++i) {
        QVariant val = proxyFilter.get(i, "fileName");
        QVERIFY(!val.isNull());
        QVERIFY(val.isValid());
        names << val.toString();
    }
    QVERIFY(names.contains("fileA.pdf"));
    QVERIFY(names.contains("fileB.png"));
    QVERIFY(names.contains("fileC.pdf"));

    // Verify each row returns its own unique data (the Table View bug was all rows showing same
    // data)
    QSet<QString> uniqueNames(names.begin(), names.end());
    QCOMPARE(uniqueNames.size(), 3);

    // Verify other roles return correct per-row values
    // Find fileA's row index
    int fileARow = -1;
    for (int i = 0; i < proxyFilter.rowCount(); ++i) {
        if (proxyFilter.get(i, "fileName").toString() == "fileA.pdf") {
            fileARow = i;
            break;
        }
    }
    QVERIFY(fileARow >= 0);
    QCOMPARE(proxyFilter.get(fileARow, "starRating").toInt(), 5);
    QCOMPARE(proxyFilter.get(fileARow, "fileSize").toLongLong(), 100LL);
    QCOMPARE(proxyFilter.get(fileARow, "isOffline").toBool(), false);
    QVERIFY(!proxyFilter.get(fileARow, "absolutePath").toString().isEmpty());
    QVERIFY(!proxyFilter.get(fileARow, "offlineColor").toString().isEmpty());
    QVERIFY(!proxyFilter.get(fileARow, "fileSizeStr").toString().isEmpty());
    QVERIFY(!proxyFilter.get(fileARow, "starRatingStr").toString().isEmpty());

    // Find fileC and verify it's offline
    int fileCRow = -1;
    for (int i = 0; i < proxyFilter.rowCount(); ++i) {
        if (proxyFilter.get(i, "fileName").toString() == "fileC.pdf") {
            fileCRow = i;
            break;
        }
    }
    QVERIFY(fileCRow >= 0);
    QCOMPARE(proxyFilter.get(fileCRow, "starRating").toInt(), 2);
    QCOMPARE(proxyFilter.get(fileCRow, "isOffline").toBool(), true);
}

void TestModels::testProxyFilterGetBounds()
{
    // Verify that out-of-bounds and invalid role names return null QVariant
    DocumentModel sourceModel(m_dbMgr);
    QTRY_COMPARE(sourceModel.rowCount(), 3);
    ProxyFilter proxyFilter(m_dbMgr);
    proxyFilter.setSourceModel(&sourceModel);
    proxyFilter.setShowUnavailable(true);

    QTRY_COMPARE(proxyFilter.rowCount(), 3);

    // Negative row
    QVERIFY(!proxyFilter.get(-1, "fileName").isValid());

    // Row beyond count
    QVERIFY(!proxyFilter.get(999, "fileName").isValid());

    // Non-existent role name
    QVERIFY(!proxyFilter.get(0, "nonExistentRole").isValid());

    // Empty role name
    QVERIFY(!proxyFilter.get(0, "").isValid());
}

void TestModels::testMultiTermSearch()
{
    DocumentModel sourceModel(m_dbMgr);
    QTRY_COMPARE(sourceModel.rowCount(), 3);
    ProxyFilter proxyFilter(m_dbMgr);
    proxyFilter.setSourceModel(&sourceModel);
    proxyFilter.setShowUnavailable(true);

    // Initial count
    QTRY_COMPARE(proxyFilter.rowCount(), 3);

    // Multi-term search: notes and tag
    // fileA.pdf has tag "work" (1), notes "Boss notes"
    // Let's search "boss work" -> should match fileA.pdf
    proxyFilter.setFilterString("boss work");
    QTRY_COMPARE(proxyFilter.rowCount(), 1);
    QCOMPARE(proxyFilter.data(proxyFilter.index(0, 0), DocumentModel::FileNameRole).toString(),
             QString("fileA.pdf"));

    // Case-insensitivity check: "BoSs WoRk"
    proxyFilter.setFilterString("BoSs WoRk");
    QTRY_COMPARE(proxyFilter.rowCount(), 1);
    QCOMPARE(proxyFilter.data(proxyFilter.index(0, 0), DocumentModel::FileNameRole).toString(),
             QString("fileA.pdf"));

    // Non-matching term: "boss play" -> no match
    proxyFilter.setFilterString("boss play");
    QTRY_COMPARE(proxyFilter.rowCount(), 0);

    // Matches notes, content, filename, tag
    // fileA has filename "fileA.pdf", text "Hello world PDF text.", notes "Boss notes", tags
    // "work", "important" Search "fileA important pdf" -> should match fileA.pdf
    proxyFilter.setFilterString("fileA important pdf");
    QTRY_COMPARE(proxyFilter.rowCount(), 1);
    QCOMPARE(proxyFilter.data(proxyFilter.index(0, 0), DocumentModel::FileNameRole).toString(),
             QString("fileA.pdf"));

    // Multi-document matches
    // "work" tag is on File A and File B
    // Search "work" -> matches File A and File B
    proxyFilter.setFilterString("work");
    QTRY_COMPARE(proxyFilter.rowCount(), 2);

    // "work hello" -> "work" on File A/B, but "hello" is only on File A ("Hello world...") and File
    // C ("Hello duplicate...") So "work hello" matches only File A
    proxyFilter.setFilterString("work hello");
    QTRY_COMPARE(proxyFilter.rowCount(), 1);
    QCOMPARE(proxyFilter.data(proxyFilter.index(0, 0), DocumentModel::FileNameRole).toString(),
             QString("fileA.pdf"));

    // Reset filter
    proxyFilter.setFilterString("");
    QTRY_COMPARE(proxyFilter.rowCount(), 3);
}

void TestModels::testScopeFiltering()
{
    DocumentModel sourceModel(m_dbMgr);
    QTRY_COMPARE(sourceModel.rowCount(), 3);
    ProxyFilter proxyFilter(m_dbMgr);
    proxyFilter.setSourceModel(&sourceModel);
    proxyFilter.setShowUnavailable(true);

    // Initial check: activeScopes should contain All, Today, This Week, This Month, current year,
    // Local, Unavailable, PDF, PNG
    QStringList active = proxyFilter.activeScopes();
    QVERIFY(active.contains("All"));
    QVERIFY(active.contains("Today"));
    QVERIFY(active.contains("This Week"));
    QVERIFY(active.contains("This Month"));
    QVERIFY(active.contains(QString::number(QDate::currentDate().year())));
    QVERIFY(active.contains("Local"));
    QVERIFY(active.contains("Unavailable"));
    QVERIFY(active.contains("PDF"));
    QVERIFY(active.contains("PNG"));

    // Test PDF filtering
    proxyFilter.setScopeFilter("PDF");
    QTRY_COMPARE(proxyFilter.rowCount(), 2);  // fileA.pdf and fileC.pdf
    for (int i = 0; i < proxyFilter.rowCount(); ++i) {
        QString name = proxyFilter.get(i, "fileName").toString();
        QVERIFY(name.endsWith(".pdf"));
    }

    // Test PNG filtering
    proxyFilter.setScopeFilter("PNG");
    QTRY_COMPARE(proxyFilter.rowCount(), 1);  // fileB.png
    QCOMPARE(proxyFilter.get(0, "fileName").toString(), "fileB.png");

    // Test Online filtering
    proxyFilter.setScopeFilter("Local");
    QTRY_COMPARE(proxyFilter.rowCount(), 2);  // fileA.pdf and fileB.png
    for (int i = 0; i < proxyFilter.rowCount(); ++i) {
        QVERIFY(!proxyFilter.get(i, "isOffline").toBool());
    }

    // Test Offline filtering
    proxyFilter.setScopeFilter("Unavailable");
    QTRY_COMPARE(proxyFilter.rowCount(), 1);  // fileC.pdf
    QCOMPARE(proxyFilter.get(0, "fileName").toString(), "fileC.pdf");
    QVERIFY(proxyFilter.get(0, "isOffline").toBool());

    // Reset scope
    proxyFilter.setScopeFilter("All");
    QTRY_COMPARE(proxyFilter.rowCount(), 3);

    // Now test that scopes are updated when other filters change.
    // Set min rating to 4. Only fileA.pdf (PDF, Local, Rating 5) matches.
    // fileB.png (rating 3) and fileC.pdf (rating 2) are filtered out by rating.
    proxyFilter.setMinRating(4);

    // Active scopes should now NOT contain Unavailable or PNG
    QStringList active2 = proxyFilter.activeScopes();
    QVERIFY(active2.contains("All"));
    QVERIFY(active2.contains("PDF"));
    QVERIFY(!active2.contains("PNG"));
    QVERIFY(active2.contains("Local"));
    QVERIFY(!active2.contains("Unavailable"));

    // Reset rating filter
    proxyFilter.setMinRating(0);
    QStringList active3 = proxyFilter.activeScopes();
    QVERIFY(active3.contains("PNG"));
    QVERIFY(active3.contains("Unavailable"));
}

void TestModels::testSorting()
{
    DocumentModel sourceModel(m_dbMgr);
    QTRY_COMPARE(sourceModel.rowCount(), 3);
    ProxyFilter proxyFilter(m_dbMgr);
    proxyFilter.setSourceModel(&sourceModel);
    proxyFilter.setShowUnavailable(true);

    // 1. Sort by FileNameRole (259) Ascending
    proxyFilter.setSortRole(259);
    proxyFilter.sort(0, Qt::AscendingOrder);
    QTRY_COMPARE(proxyFilter.rowCount(), 3);
    QCOMPARE(proxyFilter.get(0, "fileName").toString(), QString("fileA.pdf"));
    QCOMPARE(proxyFilter.get(1, "fileName").toString(), QString("fileB.png"));
    QCOMPARE(proxyFilter.get(2, "fileName").toString(), QString("fileC.pdf"));

    // 2. Sort by FileNameRole (259) Descending
    proxyFilter.sort(0, Qt::DescendingOrder);
    QCOMPARE(proxyFilter.get(0, "fileName").toString(), QString("fileC.pdf"));
    QCOMPARE(proxyFilter.get(1, "fileName").toString(), QString("fileB.png"));
    QCOMPARE(proxyFilter.get(2, "fileName").toString(), QString("fileA.pdf"));

    // 3. Sort by StarRatingRole (267) Ascending
    proxyFilter.setSortRole(267);
    proxyFilter.sort(0, Qt::AscendingOrder);
    QCOMPARE(proxyFilter.get(0, "fileName").toString(), QString("fileC.pdf"));  // Rating 2
    QCOMPARE(proxyFilter.get(1, "fileName").toString(), QString("fileB.png"));  // Rating 3
    QCOMPARE(proxyFilter.get(2, "fileName").toString(), QString("fileA.pdf"));  // Rating 5

    // 4. Sort by StarRatingRole (267) Descending
    proxyFilter.sort(0, Qt::DescendingOrder);
    QCOMPARE(proxyFilter.get(0, "fileName").toString(), QString("fileA.pdf"));  // Rating 5
    QCOMPARE(proxyFilter.get(1, "fileName").toString(), QString("fileB.png"));  // Rating 3
    QCOMPARE(proxyFilter.get(2, "fileName").toString(), QString("fileC.pdf"));  // Rating 2

    // 5. Sort by FileSizeRole (261) Ascending
    proxyFilter.setSortRole(261);
    proxyFilter.sort(0, Qt::AscendingOrder);
    // fileB.png (size 200) must be the last one since it's the largest
    QCOMPARE(proxyFilter.get(2, "fileName").toString(), QString("fileB.png"));
}

void TestModels::testRecentCategory()
{
    DocumentModel sourceModel(m_dbMgr);
    QTRY_COMPARE(sourceModel.rowCount(), 3);
    ProxyFilter proxyFilter(m_dbMgr);
    proxyFilter.setSourceModel(&sourceModel);

    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    QSqlQuery query(db);

    QVERIFY(query.exec("UPDATE documents SET last_opened = 0;"));
    sourceModel.forceRefresh();
    QTRY_VERIFY(!sourceModel.isRefreshing());

    // By default, category filter "Recent" should show nothing because last_opened is 0 for all
    proxyFilter.setCategoryFilter("Recent");
    QTRY_COMPARE(proxyFilter.rowCount(), 0);

    if (!query.exec("UPDATE documents SET last_opened = 100 WHERE file_name = 'fileA.pdf';")) {
        qWarning() << "SQL ERROR:" << query.lastError().text();
        QFAIL("Query failed");
    }
    QVERIFY(query.exec("UPDATE documents SET last_opened = 200 WHERE file_name = 'fileB.png';"));
    sourceModel.forceRefresh();
    QTRY_VERIFY(!sourceModel.isRefreshing());

    // Should show 2 documents (File A and File B, but not File C)
    QTRY_COMPARE(proxyFilter.rowCount(), 2);

    // They must be sorted by last_opened DESC: fileB.png (200) should be first, then fileA.pdf
    // (100)
    QCOMPARE(proxyFilter.get(0, "fileName").toString(), QString("fileB.png"));
    QCOMPARE(proxyFilter.get(1, "fileName").toString(), QString("fileA.pdf"));

    // Clean up category filter
    proxyFilter.setCategoryFilter("All");
}

void TestModels::testTagSelectionAndClearing()
{
    DocumentModel sourceModel(m_dbMgr);
    QTRY_COMPARE(sourceModel.rowCount(), 3);
    ProxyFilter proxyFilter(m_dbMgr);
    proxyFilter.setSourceModel(&sourceModel);
    proxyFilter.setShowUnavailable(true);

    // Initial count
    QTRY_COMPARE(proxyFilter.rowCount(), 3);

    // Filter by tag "work"
    proxyFilter.setSelectedTags(QStringList{"work"});
    QTRY_COMPARE(proxyFilter.rowCount(), 2);

    // Clear tag selection
    proxyFilter.setSelectedTags(QStringList{});
    QTRY_COMPARE(proxyFilter.rowCount(), 3);
}

void TestModels::testModelChangeScopeDebounce()
{
    // Verify that when the source model changes while no search is active, row
    // membership stays up to date synchronously (via dynamicSortFilter), while the
    // scope recount is debounced instead of running per model signal.
    DocumentModel sourceModel(m_dbMgr);
    QTRY_COMPARE(sourceModel.rowCount(), 3);
    ProxyFilter proxyFilter(m_dbMgr);
    proxyFilter.setSourceModel(&sourceModel);
    proxyFilter.setShowUnavailable(true);

    QTRY_COMPARE(proxyFilter.rowCount(), 3);
    QVERIFY(!proxyFilter.activeScopes().contains("TXT"));

    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    QSqlQuery query(db);

    int folderId = -1;
    QVERIFY(query.exec("SELECT id FROM watched_folders WHERE absolute_path = '/test/docs';"));
    QVERIFY(query.next());
    folderId = query.value(0).toInt();

    QVERIFY(query.exec(QString("INSERT INTO documents (folder_id, file_name, absolute_path, "
                               "file_size, file_hash, star_rating, is_offline, date_modified) "
                               "VALUES (%1, 'fileD.txt', '/test/docs/fileD.txt', 50, 'hash789', "
                               "0, 0, datetime('now', 'localtime'));")
                           .arg(folderId)));
    int docDId = query.lastInsertId().toInt();
    QVERIFY(query.exec(QString("INSERT INTO document_search (rowid, file_name, "
                               "text_snippet, notes) VALUES (%1, 'fileD.txt', 'Plain text "
                               "content.', '');")
                           .arg(docDId)));

    sourceModel.forceRefresh();
    QTRY_VERIFY(!sourceModel.isRefreshing());

    // Row membership updates synchronously via base-class dynamic filtering
    QTRY_COMPARE(proxyFilter.rowCount(), 4);

    // Scope recount is debounced; applied once the 400ms timer fires
    QTest::qWait(500);
    QVERIFY(proxyFilter.activeScopes().contains("TXT"));

    // Clean up
    QVERIFY(query.exec(QString("DELETE FROM document_search WHERE rowid = %1;").arg(docDId)));
    QVERIFY(query.exec(QString("DELETE FROM documents WHERE id = %1;").arg(docDId)));
}

void TestModels::testSearchMatchDebounce()
{
    // Verify that when model data changes while a search is active, the match set
    // recomputation is debounced and applied once the timer fires.
    DocumentModel sourceModel(m_dbMgr);
    QTRY_COMPARE(sourceModel.rowCount(), 3);
    ProxyFilter proxyFilter(m_dbMgr);
    proxyFilter.setSourceModel(&sourceModel);
    proxyFilter.setShowUnavailable(true);

    // "world" initially matches only fileA.pdf
    proxyFilter.setFilterString("world");
    QTRY_COMPARE(proxyFilter.rowCount(), 1);

    // Change fileB's indexed text so it also matches the active search
    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    QSqlQuery query(db);
    bool okUpdate = query.exec("UPDATE document_search SET text_snippet = 'world peace' WHERE file_name = 'fileB.png';");
    if (!okUpdate) qWarning() << "Update failed:" << query.lastError().text();
    else if (query.numRowsAffected() == 0) qWarning() << "Update affected 0 rows!";
    QVERIFY(okUpdate && query.numRowsAffected() > 0);

    sourceModel.forceRefresh();
    QTRY_VERIFY(!sourceModel.isRefreshing());

    // Recompute is debounced; applied once the 400ms timer fires
    QTRY_VERIFY(proxyFilter.rowCount() == 2);

    // Clearing the search stays synchronous
    proxyFilter.setFilterString("");
    QTRY_COMPARE(proxyFilter.rowCount(), 3);

    // Restore original text
    QVERIFY(
        query.exec("UPDATE document_search SET text_snippet = 'Scanned image content.' "
                   "WHERE file_name = 'fileB.png';"));
}

QTEST_MAIN(TestModels)
#include "test_models.moc"
