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

#include <QTest>
#include <QSignalSpy>
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
    QVERIFY(query.exec(QString(
        "INSERT INTO documents (folder_id, file_name, absolute_path, file_size, file_hash, star_rating, is_offline) "
        "VALUES (%1, 'fileA.pdf', '/test/docs/fileA.pdf', 100, 'hash123', 5, 0);").arg(folderId)));
    int docAId = query.lastInsertId().toInt();

    // Doc 2: File B, size 200, hash456, rating 3, online, tags: work, personal
    QVERIFY(query.exec(QString(
        "INSERT INTO documents (folder_id, file_name, absolute_path, file_size, file_hash, star_rating, is_offline) "
        "VALUES (%1, 'fileB.png', '/test/docs/fileB.png', 200, 'hash456', 3, 0);").arg(folderId)));
    int docBId = query.lastInsertId().toInt();

    // Doc 3: File C, size 100, hash123, rating 2, offline, tags: personal (shares hash123 with File A -> Duplicate!)
    QVERIFY(query.exec(QString(
        "INSERT INTO documents (folder_id, file_name, absolute_path, file_size, file_hash, star_rating, is_offline) "
        "VALUES (%1, 'fileC.pdf', '/test/docs/fileC.pdf', 100, 'hash123', 2, 1);").arg(folderId)));
    int docCId = query.lastInsertId().toInt();

    // Insert FTS text
    QVERIFY(query.exec(QString("INSERT INTO document_search (document_id, file_name, text_snippet, notes) VALUES (%1, 'fileA.pdf', 'Hello world PDF text.', 'Boss notes');").arg(docAId)));
    QVERIFY(query.exec(QString("INSERT INTO document_search (document_id, file_name, text_snippet, notes) VALUES (%1, 'fileB.png', 'Scanned image content.', '');").arg(docBId)));
    QVERIFY(query.exec(QString("INSERT INTO document_search (document_id, file_name, text_snippet, notes) VALUES (%1, 'fileC.pdf', 'Hello duplicate test.', 'Clone');").arg(docCId)));

    // Insert Tags
    QVERIFY(query.exec("INSERT INTO tags (id, name) VALUES (1, 'work');"));
    QVERIFY(query.exec("INSERT INTO tags (id, name) VALUES (2, 'important');"));
    QVERIFY(query.exec("INSERT INTO tags (id, name) VALUES (3, 'personal');"));

    // Link Tags
    QVERIFY(query.exec(QString("INSERT INTO document_tags (document_id, tag_id) VALUES (%1, 1);").arg(docAId)));
    QVERIFY(query.exec(QString("INSERT INTO document_tags (document_id, tag_id) VALUES (%1, 2);").arg(docAId)));

    QVERIFY(query.exec(QString("INSERT INTO document_tags (document_id, tag_id) VALUES (%1, 1);").arg(docBId)));
    QVERIFY(query.exec(QString("INSERT INTO document_tags (document_id, tag_id) VALUES (%1, 3);").arg(docBId)));

    QVERIFY(query.exec(QString("INSERT INTO document_tags (document_id, tag_id) VALUES (%1, 3);").arg(docCId)));
}

void TestModels::cleanupTestCase()
{
    delete m_dbMgr;
}

void TestModels::testModelFiltering()
{
    // Test base DocumentModel loads all 3 documents
    DocumentModel sourceModel(m_dbMgr);
    QCOMPARE(sourceModel.rowCount(), 3);

    // Test ProxyFilter
    ProxyFilter proxyFilter(m_dbMgr);
    proxyFilter.setSourceModel(&sourceModel);
    proxyFilter.setShowOffline(true);

    // Initial count with showOffline = true
    QCOMPARE(proxyFilter.rowCount(), 3);

    // Test offline hiding
    proxyFilter.setShowOffline(false);
    QCOMPARE(proxyFilter.rowCount(), 2); // File A & B are online
    proxyFilter.setShowOffline(true);

    // Test tag intersection (AND filtering)
    // Filter tag "work" (matches File A, File B)
    proxyFilter.setSelectedTags(QStringList{"work"});
    QCOMPARE(proxyFilter.rowCount(), 2);

    // Filter tags "work" AND "important" (matches File A only)
    proxyFilter.setSelectedTags(QStringList{"work", "important"});
    QCOMPARE(proxyFilter.rowCount(), 1);
    QCOMPARE(proxyFilter.data(proxyFilter.index(0, 0), DocumentModel::FileNameRole).toString(), QString("fileA.pdf"));

    // Reset tags filter
    proxyFilter.setSelectedTags(QStringList{});
    QCOMPARE(proxyFilter.rowCount(), 3);

    // Test Rating filter >= 4 (File A = 5, matches)
    proxyFilter.setMinRating(4);
    QCOMPARE(proxyFilter.rowCount(), 1);
    QCOMPARE(proxyFilter.data(proxyFilter.index(0, 0), DocumentModel::FileNameRole).toString(), QString("fileA.pdf"));
    proxyFilter.setMinRating(0);

    // Test Duplicate filter (File A and C share 'hash123')
    proxyFilter.setDuplicatesOnly(true);
    QCOMPARE(proxyFilter.rowCount(), 2);
    proxyFilter.setDuplicatesOnly(false);

    // Test FTS5 search (debounced search update)
    // Search "world" (matches File A)
    proxyFilter.setFilterString("world");
    QCOMPARE(proxyFilter.rowCount(), 1);
    QCOMPARE(proxyFilter.data(proxyFilter.index(0, 0), DocumentModel::FileNameRole).toString(), QString("fileA.pdf"));

    // Search "duplicate" (matches File C)
    proxyFilter.setFilterString("duplicate");
    QCOMPARE(proxyFilter.rowCount(), 1);
    QCOMPARE(proxyFilter.data(proxyFilter.index(0, 0), DocumentModel::FileNameRole).toString(), QString("fileC.pdf"));

    proxyFilter.setFilterString("");
    QCOMPARE(proxyFilter.rowCount(), 3);
}

void TestModels::testProxyFilterGet()
{
    // Verify that ProxyFilter::get(row, roleName) returns correct per-row data.
    // This is the mechanism KaakaoTableView delegates use to retrieve cell values.
    DocumentModel sourceModel(m_dbMgr);
    ProxyFilter proxyFilter(m_dbMgr);
    proxyFilter.setSourceModel(&sourceModel);
    proxyFilter.setShowOffline(true);

    // Call header getters to ensure code coverage
    QCOMPARE(proxyFilter.filterString(), QString(""));
    QCOMPARE(proxyFilter.selectedTags(), QStringList());
    QCOMPARE(proxyFilter.minRating(), 0);
    QCOMPARE(proxyFilter.showOffline(), true);
    QCOMPARE(proxyFilter.duplicatesOnly(), false);
    QCOMPARE(proxyFilter.categoryFilter(), QString("All"));
    QCOMPARE(proxyFilter.folderFilter(), QString(""));

    QCOMPARE(proxyFilter.rowCount(), 3);

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

    // Verify each row returns its own unique data (the Table View bug was all rows showing same data)
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
    ProxyFilter proxyFilter(m_dbMgr);
    proxyFilter.setSourceModel(&sourceModel);
    proxyFilter.setShowOffline(true);

    QCOMPARE(proxyFilter.rowCount(), 3);

    // Negative row
    QVERIFY(!proxyFilter.get(-1, "fileName").isValid());

    // Row beyond count
    QVERIFY(!proxyFilter.get(999, "fileName").isValid());

    // Non-existent role name
    QVERIFY(!proxyFilter.get(0, "nonExistentRole").isValid());

    // Empty role name
    QVERIFY(!proxyFilter.get(0, "").isValid());
}

QTEST_MAIN(TestModels)
#include "test_models.moc"
