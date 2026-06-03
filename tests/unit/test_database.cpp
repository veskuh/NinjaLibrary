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
#include "../../src/database/DatabaseManager.h"

class TestDatabase : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();
    void testInitialization();
    void testThreadSafety();
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
    
    // Check user_version PRAGMA is 1
    QVERIFY(query.exec("PRAGMA user_version;"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 1);
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

QTEST_MAIN(TestDatabase)
#include "test_database.moc"
