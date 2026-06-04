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
#include <QThread>
#include <QTemporaryFile>
#include "../../src/database/DatabaseManager.h"

class TestDatabase : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();
    void testInitialization();
    void testThreadSafety();
    void testCaseInsensitiveTags();
    void testMigration();
};

void TestDatabase::initTestCase()
{
}

void TestDatabase::cleanupTestCase()
{
}

void TestDatabase::testInitialization()
{
    // Initialize in-memory database
    DatabaseManager dbManager(":memory:");
    QVERIFY(dbManager.initializeDatabase());
    
    QSqlDatabase db = dbManager.getDatabaseConnection();
    QVERIFY(db.isOpen());
    
    // Check tables exist
    QStringList tables = db.tables();
    QVERIFY(tables.contains("watched_folders"));
    QVERIFY(tables.contains("documents"));
    QVERIFY(tables.contains("tags"));
    QVERIFY(tables.contains("document_tags"));
    QVERIFY(tables.contains("smart_collections"));
    
    // Check FTS5 virtual table
    QSqlQuery query(db);
    QVERIFY(query.exec("SELECT * FROM document_search;"));
    
    // Check user_version PRAGMA is 2
    QVERIFY(query.exec("PRAGMA user_version;"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 2);
}

void TestDatabase::testThreadSafety()
{
    DatabaseManager dbManager(":memory:");
    QVERIFY(dbManager.initializeDatabase());

    // Run an insert query on another thread using dbManager
    class WorkerThread : public QThread {
    public:
        DatabaseManager &mgr;
        bool success = false;
        WorkerThread(DatabaseManager &m) : mgr(m) {}
        void run() override {
            QSqlDatabase db = mgr.getDatabaseConnection();
            if (db.isOpen() && db.tables().contains("watched_folders")) {
                QSqlQuery query(db);
                if (query.exec("INSERT INTO watched_folders (absolute_path) VALUES ('/test/path');")) {
                    success = true;
                }
            }
        }
    };

    WorkerThread thread(dbManager);
    thread.start();
    thread.wait();

    QVERIFY(thread.success);

    // Verify insertion on main thread
    QSqlDatabase db = dbManager.getDatabaseConnection();
    QSqlQuery query(db);
    QVERIFY(query.exec("SELECT absolute_path FROM watched_folders;"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QString("/test/path"));
}

void TestDatabase::testCaseInsensitiveTags()
{
    DatabaseManager dbManager(":memory:");
    QVERIFY(dbManager.initializeDatabase());
    
    QSqlDatabase db = dbManager.getDatabaseConnection();
    QVERIFY(db.isOpen());
    
    QSqlQuery query(db);
    
    // Insert first tag
    QVERIFY(query.exec("INSERT INTO tags (name) VALUES ('Notes');"));
    
    // Try to insert case-insensitive duplicate tag - should fail unique constraint
    QVERIFY(!query.exec("INSERT INTO tags (name) VALUES ('notes');"));
    
    // Try INSERT OR IGNORE
    QVERIFY(query.exec("INSERT OR IGNORE INTO tags (name) VALUES ('NOTES');"));
    
    // Verify only 1 tag exists in tags table
    QVERIFY(query.exec("SELECT COUNT(*) FROM tags;"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 1);
    
    // Verify SELECT matches case-insensitively
    query.prepare("SELECT name FROM tags WHERE name = :name;");
    query.bindValue(":name", "noTeS");
    QVERIFY(query.exec());
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QString("Notes"));
}

void TestDatabase::testMigration()
{
    QTemporaryFile tempDbFile;
    QVERIFY(tempDbFile.open());
    QString tempDbPath = tempDbFile.fileName();
    tempDbFile.close(); // Close so SQLite can open it

    // 1. Manually open a database connection to set up version 1
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "migration_test_setup");
        db.setDatabaseName(tempDbPath);
        QVERIFY(db.open());

        {
            QSqlQuery query(db);
            QVERIFY(query.exec("CREATE TABLE watched_folders (id INTEGER PRIMARY KEY AUTOINCREMENT, absolute_path TEXT UNIQUE NOT NULL, macos_bookmark BLOB, is_available BOOLEAN DEFAULT 1);"));
            QVERIFY(query.exec("CREATE TABLE documents (id INTEGER PRIMARY KEY AUTOINCREMENT, folder_id INTEGER, file_name TEXT NOT NULL, absolute_path TEXT UNIQUE NOT NULL, file_size INTEGER NOT NULL, file_hash TEXT, date_created DATETIME, date_modified DATETIME, date_added DATETIME DEFAULT CURRENT_TIMESTAMP, page_count INTEGER DEFAULT 0, star_rating INTEGER DEFAULT 0, is_offline BOOLEAN DEFAULT 0);"));
            QVERIFY(query.exec("CREATE VIRTUAL TABLE document_search USING fts5(document_id UNINDEXED, file_name, text_snippet, notes);"));
            
            // Version 1 tags table without COLLATE NOCASE
            QVERIFY(query.exec("CREATE TABLE tags (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT UNIQUE NOT NULL, color_hex TEXT DEFAULT '#3498db');"));
            QVERIFY(query.exec("CREATE TABLE document_tags (document_id INTEGER, tag_id INTEGER, PRIMARY KEY (document_id, tag_id), FOREIGN KEY(document_id) REFERENCES documents(id) ON DELETE CASCADE, FOREIGN KEY(tag_id) REFERENCES tags(id) ON DELETE CASCADE);"));
            QVERIFY(query.exec("CREATE TABLE smart_collections (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT UNIQUE NOT NULL, query_json TEXT NOT NULL);"));

            // Insert duplicate tags (case-sensitively)
            QVERIFY(query.exec("INSERT INTO tags (id, name) VALUES (1, 'Notes');"));
            QVERIFY(query.exec("INSERT INTO tags (id, name) VALUES (2, 'notes');"));
            QVERIFY(query.exec("INSERT INTO tags (id, name) VALUES (3, 'NOTES');"));
            QVERIFY(query.exec("INSERT INTO tags (id, name) VALUES (4, 'Other');"));

            // Insert documents
            QVERIFY(query.exec("INSERT INTO documents (id, file_name, absolute_path, file_size) VALUES (1, 'doc1.pdf', '/path/1', 100);"));
            QVERIFY(query.exec("INSERT INTO documents (id, file_name, absolute_path, file_size) VALUES (2, 'doc2.pdf', '/path/2', 200);"));

            // Link documents to tags
            // Doc 1 is linked to 'Notes' (1) and 'notes' (2) and 'Other' (4)
            QVERIFY(query.exec("INSERT INTO document_tags (document_id, tag_id) VALUES (1, 1);"));
            QVERIFY(query.exec("INSERT INTO document_tags (document_id, tag_id) VALUES (1, 2);"));
            QVERIFY(query.exec("INSERT INTO document_tags (document_id, tag_id) VALUES (1, 4);"));
            
            // Doc 2 is linked to 'NOTES' (3)
            QVERIFY(query.exec("INSERT INTO document_tags (document_id, tag_id) VALUES (2, 3);"));

            // Set version to 1
            QVERIFY(query.exec("PRAGMA user_version = 1;"));
        }
        db.close();
    }
    QSqlDatabase::removeDatabase("migration_test_setup");

    // 2. Initialize DatabaseManager targeting this database
    {
        DatabaseManager dbManager(tempDbPath);
        QVERIFY(dbManager.initializeDatabase());

        {
            QSqlDatabase db = dbManager.getDatabaseConnection();
            QVERIFY(db.isOpen());

            QSqlQuery query(db);

            // Check user_version PRAGMA is 2
            QVERIFY(query.exec("PRAGMA user_version;"));
            QVERIFY(query.next());
            QCOMPARE(query.value(0).toInt(), 2);

            // Check remaining tags (should only be 'Notes' (1) and 'Other' (4))
            QVERIFY(query.exec("SELECT id, name FROM tags ORDER BY id;"));
            QVERIFY(query.next());
            QCOMPARE(query.value(0).toInt(), 1);
            QCOMPARE(query.value(1).toString(), QString("Notes"));
            QVERIFY(query.next());
            QCOMPARE(query.value(0).toInt(), 4);
            QCOMPARE(query.value(1).toString(), QString("Other"));
            QVERIFY(!query.next());

            // Check document 1 links (should be linked to tag 1 and 4)
            QVERIFY(query.exec("SELECT tag_id FROM document_tags WHERE document_id = 1 ORDER BY tag_id;"));
            QVERIFY(query.next());
            QCOMPARE(query.value(0).toInt(), 1);
            QVERIFY(query.next());
            QCOMPARE(query.value(0).toInt(), 4);
            QVERIFY(!query.next());

            // Check document 2 links (should be linked to tag 1 now)
            QVERIFY(query.exec("SELECT tag_id FROM document_tags WHERE document_id = 2;"));
            QVERIFY(query.next());
            QCOMPARE(query.value(0).toInt(), 1);
            QVERIFY(!query.next());

            // Check that case-insensitive constraint works now (inserting 'notes' fails)
            QVERIFY(!query.exec("INSERT INTO tags (name) VALUES ('notes');"));
        }
    }
}

QTEST_MAIN(TestDatabase)
#include "test_database.moc"
