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

#include "DatabaseManager.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QSqlRecord>
#include <QStandardPaths>
#include <QThreadStorage>
#include <QUuid>

struct ConnectionHolder
{
    QString name;
    ~ConnectionHolder()
    {
        if (!name.isEmpty() && QCoreApplication::instance()) {
            {
                QSqlDatabase db = QSqlDatabase::database(name, false);
                if (db.isOpen()) {
                    db.close();
                }
            }
            QSqlDatabase::removeDatabase(name);
        }
    }
};

static QThreadStorage<ConnectionHolder *> s_connectionStorage;

DatabaseManager::DatabaseManager(const QString &dbPath, QObject *parent) : QObject(parent)
{
    if (dbPath.isEmpty()) {
        QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        if (dataDir.isEmpty()) {
            dataDir = QDir::homePath() + "/.config/NinjaLibrary";
        }
        m_dbPath = dataDir + "/library.db";
    } else if (dbPath == ":memory:") {
        m_dbPath = QString("file:ninjalib_shared_%1?mode=memory&cache=shared")
                       .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    } else {
        m_dbPath = dbPath;
    }

    // Ensure the folder exists if it's not an in-memory database
    if (m_dbPath != ":memory:" && !m_dbPath.startsWith("file:")) {
        QFileInfo fileInfo(m_dbPath);
        QDir().mkpath(fileInfo.absolutePath());
    }
}

DatabaseManager::~DatabaseManager() {}

QSqlDatabase DatabaseManager::getDatabaseConnection()
{
    QString threadId = QString::number(reinterpret_cast<quintptr>(QThread::currentThreadId()));
    QString connectionName = QString("NinjaLibrary_Connection_%1").arg(threadId);

    QString expectedDbName = m_dbPath;

    if (!s_connectionStorage.hasLocalData()) {
        if (QSqlDatabase::contains(connectionName)) {
            // This is a stale connection from a terminated thread with the same ID.
            // We must close and remove it to release locks and avoid warnings.
            {
                QSqlDatabase staleDb = QSqlDatabase::database(connectionName, false);
                if (staleDb.isOpen()) {
                    staleDb.close();
                }
            }
            QSqlDatabase::removeDatabase(connectionName);
        }

        ConnectionHolder *holder = new ConnectionHolder();
        holder->name = connectionName;
        s_connectionStorage.setLocalData(holder);
    } else {
        // If the connection name exists but points to a different database path,
        // we must close and recreate it.
        if (QSqlDatabase::contains(connectionName)) {
            bool mismatch = false;
            {
                QSqlDatabase db = QSqlDatabase::database(connectionName, false);
                if (db.databaseName() != expectedDbName) {
                    mismatch = true;
                    if (db.isOpen()) {
                        db.close();
                    }
                }
            }
            if (mismatch) {
                QSqlDatabase::removeDatabase(connectionName);
            }
        }
    }

    if (QSqlDatabase::contains(connectionName)) {
        QSqlDatabase db = QSqlDatabase::database(connectionName);
        if (db.isOpen()) {
            return db;
        }
    }

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    db.setDatabaseName(expectedDbName);

    if (!db.open()) {
        qWarning() << "Failed to open database connection" << connectionName << ":"
                   << db.lastError().text();
    } else {
        // Run thread-specific initialization PRAGMAs
        QSqlQuery query(db);
        query.exec("PRAGMA foreign_keys = ON;");
        query.exec("PRAGMA journal_mode = WAL;");
        query.exec("PRAGMA busy_timeout = 5000;");
        query.exec("PRAGMA synchronous = NORMAL;");
    }

    return db;
}

bool DatabaseManager::initializeDatabase()
{
    QSqlDatabase db = getDatabaseConnection();
    if (!db.isOpen()) {
        return false;
    }

    int currentVersion = 0;
    {
        QSqlQuery query(db);
        if (query.exec("PRAGMA user_version;")) {
            if (query.next()) {
                currentVersion = query.value(0).toInt();
            }
        }
    }

    if (currentVersion == 0) {
        db.transaction();
        bool ok = true;
        {
            QSqlQuery query(db);

            ok &= query.exec(
                "CREATE TABLE IF NOT EXISTS watched_folders ("
                "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "    absolute_path TEXT UNIQUE NOT NULL,"
                "    macos_bookmark BLOB,"
                "    is_available BOOLEAN DEFAULT 1"
                ");");

            ok &= query.exec(
                "CREATE TABLE IF NOT EXISTS documents ("
                "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "    folder_id INTEGER,"
                "    file_name TEXT NOT NULL,"
                "    absolute_path TEXT UNIQUE NOT NULL,"
                "    file_size INTEGER NOT NULL,"
                "    file_hash TEXT,"
                "    date_created DATETIME,"
                "    date_modified DATETIME,"
                "    date_added DATETIME DEFAULT CURRENT_TIMESTAMP,"
                "    page_count INTEGER DEFAULT 0,"
                "    star_rating INTEGER DEFAULT 0 CHECK(star_rating BETWEEN 0 AND 5),"
                "    is_offline BOOLEAN DEFAULT 0,"
                "    last_opened INTEGER DEFAULT 0,"
                "    FOREIGN KEY(folder_id) REFERENCES watched_folders(id) ON DELETE CASCADE"
                ");");

            // FTS5 Virtual Table for Instant Search
            ok &= query.exec(
                "CREATE VIRTUAL TABLE IF NOT EXISTS document_search USING fts5("
                "    document_id UNINDEXED,"
                "    file_name,"
                "    text_snippet,"
                "    notes"
                ");");

            ok &= query.exec(
                "CREATE TABLE IF NOT EXISTS tags ("
                "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "    name TEXT UNIQUE COLLATE NOCASE NOT NULL,"
                "    color_hex TEXT DEFAULT '#3498db'"
                ");");

            ok &= query.exec(
                "CREATE TABLE IF NOT EXISTS document_tags ("
                "    document_id INTEGER,"
                "    tag_id INTEGER,"
                "    PRIMARY KEY (document_id, tag_id),"
                "    FOREIGN KEY(document_id) REFERENCES documents(id) ON DELETE CASCADE,"
                "    FOREIGN KEY(tag_id) REFERENCES tags(id) ON DELETE CASCADE"
                ");");

            ok &= query.exec(
                "CREATE TABLE IF NOT EXISTS smart_collections ("
                "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "    name TEXT UNIQUE NOT NULL,"
                "    query_json TEXT NOT NULL"
                ");");

            if (ok) {
                ok &= query.exec("PRAGMA user_version = 3;");
            }
        }

        if (ok) {
            db.commit();
        } else {
            db.rollback();
            qWarning() << "Schema creation failed.";
            return false;
        }
    }

    if (currentVersion > 0 && currentVersion < 2) {
        // Disable foreign keys outside the transaction
        {
            QSqlQuery disableFk(db);
            disableFk.exec("PRAGMA foreign_keys = OFF;");
        }

        db.transaction();
        QSqlQuery q(db);
        bool ok = true;

        // 1. Deduplicate tags
        {
            QSqlQuery dupQuery(
                "SELECT LOWER(name) as lname, MIN(id) as keep_id FROM tags GROUP BY lname HAVING "
                "COUNT(*) > 1;",
                db);
            struct MergeJob
            {
                int keepId;
                QString lname;
            };
            QList<MergeJob> jobs;
            while (dupQuery.next()) {
                jobs.append({dupQuery.value(1).toInt(), dupQuery.value(0).toString()});
            }

            for (const auto &job : jobs) {
                QSqlQuery findDups(db);
                findDups.prepare(
                    "SELECT id FROM tags WHERE name = :lname COLLATE NOCASE AND id != :keepId;");
                findDups.bindValue(":lname", job.lname);
                findDups.bindValue(":keepId", job.keepId);
                if (findDups.exec()) {
                    while (findDups.next()) {
                        int dupId = findDups.value(0).toInt();

                        QSqlQuery mergeDocs(db);
                        mergeDocs.prepare(
                            "INSERT OR IGNORE INTO document_tags (document_id, tag_id) "
                            "SELECT document_id, :keepId FROM document_tags WHERE tag_id = "
                            ":dupId;");
                        mergeDocs.bindValue(":keepId", job.keepId);
                        mergeDocs.bindValue(":dupId", dupId);
                        if (!mergeDocs.exec()) {
                            qWarning()
                                << "Migration: mergeDocs failed:" << mergeDocs.lastError().text();
                            ok = false;
                        }

                        QSqlQuery deleteDupLinks(db);
                        deleteDupLinks.prepare("DELETE FROM document_tags WHERE tag_id = :dupId;");
                        deleteDupLinks.bindValue(":dupId", dupId);
                        if (!deleteDupLinks.exec()) {
                            qWarning() << "Migration: deleteDupLinks failed:"
                                       << deleteDupLinks.lastError().text();
                            ok = false;
                        }

                        QSqlQuery deleteDupTag(db);
                        deleteDupTag.prepare("DELETE FROM tags WHERE id = :dupId;");
                        deleteDupTag.bindValue(":dupId", dupId);
                        if (!deleteDupTag.exec()) {
                            qWarning() << "Migration: deleteDupTag failed:"
                                       << deleteDupTag.lastError().text();
                            ok = false;
                        }
                    }
                } else {
                    qWarning() << "Migration: findDups failed:" << findDups.lastError().text();
                    ok = false;
                }
            }
        }

        // 2. Recreate tags table with COLLATE NOCASE
        {
            QSqlQuery debugQ(db);
            if (debugQ.exec("SELECT id, name FROM tags;")) {
                while (debugQ.next()) {
                    qDebug() << "MIGRATION BEFORE RECREATE:" << debugQ.value(0).toInt()
                             << debugQ.value(1).toString();
                }
            }
        }
        if (ok && !q.exec("CREATE TABLE tags_new ("
                          "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
                          "    name TEXT UNIQUE COLLATE NOCASE NOT NULL,"
                          "    color_hex TEXT DEFAULT '#3498db'"
                          ");")) {
            qWarning() << "Migration: CREATE TABLE tags_new failed:" << q.lastError().text();
            ok = false;
        }
        if (ok && !q.exec("INSERT INTO tags_new (id, name, color_hex) SELECT id, name, color_hex "
                          "FROM tags;")) {
            qWarning() << "Migration: INSERT INTO tags_new failed:" << q.lastError().text();
            ok = false;
        }
        if (ok && !q.exec("DROP TABLE tags;")) {
            qWarning() << "Migration: DROP TABLE tags failed:" << q.lastError().text();
            ok = false;
        }
        if (ok && !q.exec("ALTER TABLE tags_new RENAME TO tags;")) {
            qWarning() << "Migration: ALTER TABLE tags_new RENAME failed:" << q.lastError().text();
            ok = false;
        }

        if (ok) {
            if (!q.exec("PRAGMA user_version = 2;")) {
                qWarning() << "Migration: Set user_version = 2 failed:" << q.lastError().text();
                ok = false;
            }
        }

        if (ok) {
            db.commit();
            currentVersion = 2;
        } else {
            db.rollback();
            qWarning() << "Database migration to version 2 failed.";
            {
                QSqlQuery enableFk(db);
                enableFk.exec("PRAGMA foreign_keys = ON;");
            }
            return false;
        }

        // Re-enable foreign keys outside/after transaction commit
        {
            QSqlQuery enableFk(db);
            enableFk.exec("PRAGMA foreign_keys = ON;");
        }
    }

    if (currentVersion > 0 && currentVersion < 3) {
        db.transaction();
        QSqlQuery q(db);
        bool ok = q.exec("ALTER TABLE documents ADD COLUMN last_opened INTEGER DEFAULT 0;");
        if (ok) {
            ok &= q.exec("PRAGMA user_version = 3;");
        }

        if (ok) {
            db.commit();
            currentVersion = 3;
        } else {
            db.rollback();
            qWarning() << "Database migration to version 3 failed:" << q.lastError().text();
            return false;
        }
    }

    // Ensure active_scans table exists for interrupted scan tracking
    {
        QSqlQuery query(db);
        if (!query.exec(
                "CREATE TABLE IF NOT EXISTS active_scans (folder_path TEXT PRIMARY KEY);")) {
            qWarning() << "Failed to create active_scans table:" << query.lastError().text();
        }
    }

    return true;
}

QString DatabaseManager::databasePath() const { return m_dbPath; }
