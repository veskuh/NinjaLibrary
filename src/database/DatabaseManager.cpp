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
#include <QFileInfo>
#include <QSqlRecord>

DatabaseManager::DatabaseManager(const QString &dbPath, QObject *parent)
    : QObject(parent)
{
    if (dbPath.isEmpty()) {
        m_dbPath = QDir::homePath() + "/.config/NinjaLibrary/library.db";
    } else {
        m_dbPath = dbPath;
    }

    // Ensure the folder exists if it's not an in-memory database
    if (m_dbPath != ":memory:" && !m_dbPath.startsWith("file:")) {
        QFileInfo fileInfo(m_dbPath);
        QDir().mkpath(fileInfo.absolutePath());
    }
}

DatabaseManager::~DatabaseManager()
{
}

QSqlDatabase DatabaseManager::getDatabaseConnection()
{
    QString threadId = QString::number(reinterpret_cast<quintptr>(QThread::currentThreadId()));
    QString connectionName = QString("NinjaLibrary_Connection_%1").arg(threadId);

    if (QSqlDatabase::contains(connectionName)) {
        QSqlDatabase db = QSqlDatabase::database(connectionName);
        if (db.isOpen()) {
            return db;
        }
    }

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    
    // For in-memory, we can translate standard ":memory:" to a shared cache URI 
    // so multiple connections on different threads in the same process can access it
    if (m_dbPath == ":memory:") {
        db.setDatabaseName("file:ninjalib_shared_test?mode=memory&cache=shared");
    } else {
        db.setDatabaseName(m_dbPath);
    }

    if (!db.open()) {
        qWarning() << "Failed to open database connection" << connectionName << ":" << db.lastError().text();
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

    QSqlQuery query(db);
    int currentVersion = 0;
    if (query.exec("PRAGMA user_version;")) {
        if (query.next()) {
            currentVersion = query.value(0).toInt();
        }
    }

    if (currentVersion == 0) {
        db.transaction();
        bool ok = true;

        ok &= query.exec(
            "CREATE TABLE IF NOT EXISTS watched_folders ("
            "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "    absolute_path TEXT UNIQUE NOT NULL,"
            "    macos_bookmark BLOB,"
            "    is_available BOOLEAN DEFAULT 1"
            ");"
        );

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
            "    FOREIGN KEY(folder_id) REFERENCES watched_folders(id) ON DELETE CASCADE"
            ");"
        );

        // FTS5 Virtual Table for Instant Search
        ok &= query.exec(
            "CREATE VIRTUAL TABLE IF NOT EXISTS document_search USING fts5("
            "    document_id UNINDEXED,"
            "    file_name,"
            "    text_snippet,"
            "    notes"
            ");"
        );

        ok &= query.exec(
            "CREATE TABLE IF NOT EXISTS tags ("
            "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "    name TEXT UNIQUE NOT NULL,"
            "    color_hex TEXT DEFAULT '#3498db'"
            ");"
        );

        ok &= query.exec(
            "CREATE TABLE IF NOT EXISTS document_tags ("
            "    document_id INTEGER,"
            "    tag_id INTEGER,"
            "    PRIMARY KEY (document_id, tag_id),"
            "    FOREIGN KEY(document_id) REFERENCES documents(id) ON DELETE CASCADE,"
            "    FOREIGN KEY(tag_id) REFERENCES tags(id) ON DELETE CASCADE"
            ");"
        );

        ok &= query.exec(
            "CREATE TABLE IF NOT EXISTS smart_collections ("
            "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "    name TEXT UNIQUE NOT NULL,"
            "    query_json TEXT NOT NULL"
            ");"
        );

        if (ok) {
            ok &= query.exec("PRAGMA user_version = 1;");
        }

        if (ok) {
            db.commit();
        } else {
            db.rollback();
            qWarning() << "Schema creation failed: " << query.lastError().text();
            return false;
        }
    }

    return true;
}

QString DatabaseManager::databasePath() const
{
    return m_dbPath;
}
