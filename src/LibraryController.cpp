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

#include "LibraryController.h"
#include "workers/ScannerTask.h"
#include "workers/OcrTask.h"
#include "workers/ThumbnailTask.h"
#include <QDir>
#include <QSqlQuery>
#include <QSqlError>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <thread>

#ifdef Q_OS_MAC
#include "utils/MacBookmarks.h"
#endif

LibraryController::LibraryController(DatabaseManager *dbMgr, QObject *parent)
    : QObject(parent)
    , m_dbMgr(dbMgr)
{
    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &LibraryController::onDirectoryChanged);

    // Hybrid background crawler timer (30-minute interval)
    m_crawlTimer = new QTimer(this);
    connect(m_crawlTimer, &QTimer::timeout, this, &LibraryController::triggerBackgroundCrawl);
    m_crawlTimer->start(30 * 60 * 1000); // 30 minutes in milliseconds

    // Initialize sidecar storage location
    m_sidecarDir = QDir::homePath() + "/.local/share/NinjaLibrary/sidecars/";
    QDir().mkpath(m_sidecarDir);

    // Cap thread pool concurrency to hardware_concurrency / 2
    int maxThreads = qMax(1, (int)(std::thread::hardware_concurrency() / 2));
    QThreadPool::globalInstance()->setMaxThreadCount(maxThreads);

    connect(this, &LibraryController::scanRequested, this, &LibraryController::onScanRequested);

    updateFoldersCache();
}

LibraryController::~LibraryController()
{
}

QStringList LibraryController::watchedFolders() const
{
    return m_watchedFoldersCache;
}

bool LibraryController::addWatchedFolder(const QString &folderPath)
{
    if (folderPath.isEmpty()) return false;
    QDir dir(folderPath);
    if (!dir.exists()) return false;

    QString absPath = dir.canonicalPath();
    if (absPath.isEmpty()) absPath = dir.absolutePath();
    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    if (!db.isOpen()) return false;

    QSqlQuery query(db);
    query.prepare("INSERT OR IGNORE INTO watched_folders (absolute_path, macos_bookmark) VALUES (:path, :bookmark);");
    query.bindValue(":path", absPath);

#ifdef Q_OS_MAC
    QByteArray bookmark = MacBookmarks::getBookmarkForUrl(absPath);
    query.bindValue(":bookmark", bookmark);
#else
    query.bindValue(":bookmark", QByteArray());
#endif

    if (!query.exec()) {
        qWarning() << "Failed to add watched folder to database:" << query.lastError().text();
        return false;
    }

    // Add path to directory watcher
    m_watcher->addPath(absPath);
    updateFoldersCache();

    // Trigger initial scan
    emit scanRequested(absPath);
    return true;
}

bool LibraryController::removeWatchedFolder(const QString &folderPath)
{
    if (folderPath.isEmpty()) return false;
    QString absPath = QDir(folderPath).canonicalPath();
    if (absPath.isEmpty()) absPath = QDir(folderPath).absolutePath();
    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    if (!db.isOpen()) return false;

    QSqlQuery query(db);
    query.prepare("DELETE FROM watched_folders WHERE absolute_path = :path;");
    query.bindValue(":path", absPath);

    if (!query.exec()) {
        qWarning() << "Failed to remove watched folder from database:" << query.lastError().text();
        return false;
    }

    m_watcher->removePath(absPath);
    updateFoldersCache();
    return true;
}

bool LibraryController::batchUpdateTags(const QList<int> &documentIds, const QStringList &tags)
{
    if (documentIds.isEmpty()) return true;

    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    if (!db.isOpen()) return false;

    // Use a transaction for batch updates
    db.transaction();
    QSqlQuery query(db);

    for (int docId : documentIds) {
        // Clear old document tags
        query.prepare("DELETE FROM document_tags WHERE document_id = :docId;");
        query.bindValue(":docId", docId);
        if (!query.exec()) {
            db.rollback();
            return false;
        }

        // Insert new tags
        for (const QString &tagName : tags) {
            if (tagName.trimmed().isEmpty()) continue;

            // Ensure tag exists in the global tags table
            query.prepare("INSERT OR IGNORE INTO tags (name) VALUES (:name);");
            query.bindValue(":name", tagName.trimmed());
            if (!query.exec()) {
                db.rollback();
                return false;
            }

            // Get tag ID
            query.prepare("SELECT id FROM tags WHERE name = :name;");
            query.bindValue(":name", tagName.trimmed());
            if (!query.exec() || !query.next()) {
                db.rollback();
                return false;
            }
            int tagId = query.value(0).toInt();

            // Link tag to document
            query.prepare("INSERT OR IGNORE INTO document_tags (document_id, tag_id) VALUES (:docId, :tagId);");
            query.bindValue(":docId", docId);
            query.bindValue(":tagId", tagId);
            if (!query.exec()) {
                db.rollback();
                return false;
            }
        }
        
        // Load notes & rating to update the centralized sidecar if sidecar is active
        query.prepare("SELECT absolute_path, star_rating FROM documents WHERE id = :docId;");
        query.bindValue(":docId", docId);
        if (query.exec() && query.next()) {
            QString docPath = query.value(0).toString();
            int rating = query.value(1).toInt();
            
            QSqlQuery notesQuery(db);
            notesQuery.prepare("SELECT notes FROM document_search WHERE document_id = :docId;");
            notesQuery.bindValue(":docId", docId);
            QString notes;
            if (notesQuery.exec() && notesQuery.next()) {
                notes = notesQuery.value(0).toString();
            }
            
            // Sidecar updating helper
            writeSidecar(docPath, tags, rating, notes);
        }
    }

    db.commit();
    emit libraryChanged();
    return true;
}

bool LibraryController::batchUpdateRating(const QList<int> &documentIds, int rating)
{
    if (documentIds.isEmpty()) return true;
    if (rating < 0 || rating > 5) return false;

    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    if (!db.isOpen()) return false;

    db.transaction();
    QSqlQuery query(db);

    for (int docId : documentIds) {
        query.prepare("UPDATE documents SET star_rating = :rating WHERE id = :docId;");
        query.bindValue(":rating", rating);
        query.bindValue(":docId", docId);
        if (!query.exec()) {
            db.rollback();
            return false;
        }
        
        // Fetch current tags & notes to update the centralized sidecar
        query.prepare("SELECT absolute_path FROM documents WHERE id = :docId;");
        query.bindValue(":docId", docId);
        if (query.exec() && query.next()) {
            QString docPath = query.value(0).toString();
            
            // Fetch tags
            QSqlQuery tagQuery(db);
            tagQuery.prepare("SELECT name FROM tags t JOIN document_tags dt ON t.id = dt.tag_id WHERE dt.document_id = :docId;");
            tagQuery.bindValue(":docId", docId);
            QStringList tags;
            if (tagQuery.exec()) {
                while (tagQuery.next()) {
                    tags.append(tagQuery.value(0).toString());
                }
            }
            
            // Fetch notes
            QSqlQuery notesQuery(db);
            notesQuery.prepare("SELECT notes FROM document_search WHERE document_id = :docId;");
            notesQuery.bindValue(":docId", docId);
            QString notes;
            if (notesQuery.exec() && notesQuery.next()) {
                notes = notesQuery.value(0).toString();
            }
            
            writeSidecar(docPath, tags, rating, notes);
        }
    }

    db.commit();
    emit libraryChanged();
    return true;
}

bool LibraryController::updateNotes(int docId, const QString &notes)
{
    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    if (!db.isOpen()) return false;

    db.transaction();
    QSqlQuery query(db);
    query.prepare("UPDATE document_search SET notes = :notes WHERE document_id = :docId;");
    query.bindValue(":notes", notes);
    query.bindValue(":docId", docId);
    if (!query.exec()) {
        db.rollback();
        return false;
    }

    // Fetch details to update the sidecar on disk
    QSqlQuery docQuery(db);
    docQuery.prepare("SELECT absolute_path, star_rating FROM documents WHERE id = :docId;");
    docQuery.bindValue(":docId", docId);
    if (docQuery.exec() && docQuery.next()) {
        QString docPath = docQuery.value(0).toString();
        int rating = docQuery.value(1).toInt();

        // Fetch tags
        QSqlQuery tagQuery(db);
        tagQuery.prepare("SELECT name FROM tags t JOIN document_tags dt ON t.id = dt.tag_id WHERE dt.document_id = :docId;");
        tagQuery.bindValue(":docId", docId);
        QStringList tags;
        if (tagQuery.exec()) {
            while (tagQuery.next()) {
                tags.append(tagQuery.value(0).toString());
            }
        }

        writeSidecar(docPath, tags, rating, notes);
    }

    db.commit();
    emit libraryChanged();
    return true;
}

bool LibraryController::writeSidecar(const QString &documentPath, const QStringList &tags, int rating, const QString &notes)
{
    QString sidecarPath = getSidecarPath(documentPath);
    if (sidecarPath.isEmpty()) return false;

    QJsonObject obj;
    obj["document_path"] = documentPath;
    obj["star_rating"] = rating;
    obj["notes"] = notes;
    
    QJsonArray tagsArray;
    for (const QString &tag : tags) {
        tagsArray.append(tag);
    }
    obj["tags"] = tagsArray;

    QJsonDocument doc(obj);
    QFile file(sidecarPath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(doc.toJson());
    file.close();
    return true;
}

bool LibraryController::readSidecar(const QString &documentPath, QStringList &tags, int &rating, QString &notes)
{
    QString sidecarPath = getSidecarPath(documentPath);
    if (sidecarPath.isEmpty()) return false;

    QFile file(sidecarPath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) return false;
    QJsonObject obj = doc.object();

    rating = obj["star_rating"].toInt(0);
    notes = obj["notes"].toString();
    
    tags.clear();
    QJsonArray tagsArray = obj["tags"].toArray();
    for (int i = 0; i < tagsArray.size(); ++i) {
        tags.append(tagsArray.at(i).toString());
    }

    return true;
}

void LibraryController::onDirectoryChanged(const QString &path)
{
    // A nested change occurred or directory was modified. Trigger a scanner task update.
    emit scanRequested(path);
}

void LibraryController::triggerBackgroundCrawl()
{
    // 30-minute crawler sweep of all watched folders to handle deep changes not captured by watch limit.
    for (const QString &folder : m_watchedFoldersCache) {
        emit scanRequested(folder);
    }
}

void LibraryController::updateFoldersCache()
{
    m_watchedFoldersCache.clear();
    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    if (!db.isOpen()) return;

    QSqlQuery query("SELECT absolute_path FROM watched_folders;", db);
    while (query.next()) {
        QString path = query.value(0).toString();
        m_watchedFoldersCache.append(path);
        // Ensure path is watched (safe to call addPath multiple times, it won't duplicate)
        m_watcher->addPath(path);
    }

    emit watchedFoldersChanged();
}

QString LibraryController::getSidecarPath(const QString &documentPath) const
{
    if (documentPath.isEmpty()) return QString();
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(documentPath.toUtf8());
    QString hashStr = hash.result().toHex();
    return m_sidecarDir + hashStr + ".ninja";
}

void LibraryController::onScanRequested(const QString &folderPath)
{
    QString absPath = QDir(folderPath).canonicalPath();
    if (absPath.isEmpty()) absPath = QDir(folderPath).absolutePath();
    ScannerTask *task = new ScannerTask(m_dbMgr, absPath);
    connect(task, &ScannerTask::ocrRequested, this, &LibraryController::onOcrRequested);
    connect(task, &ScannerTask::thumbnailRequested, this, &LibraryController::onThumbnailRequested);
    connect(task, &ScannerTask::finished, this, &LibraryController::onScannerTaskFinished);
    task->setAutoDelete(true);
    QThreadPool::globalInstance()->start(task);
}

void LibraryController::onOcrRequested(int docId, const QString &filePath)
{
    OcrTask *task = new OcrTask(m_dbMgr, docId, filePath);
    connect(task, &OcrTask::finished, this, &LibraryController::onOcrTaskFinished);
    task->setAutoDelete(true);
    QThreadPool::globalInstance()->start(task);
}

void LibraryController::onThumbnailRequested(int docId, const QString &filePath)
{
    requestThumbnail(docId, filePath, false);
}

void LibraryController::requestThumbnail(int docId, const QString &filePath, bool highPriority)
{
    ThumbnailTask *task = new ThumbnailTask(docId, filePath);
    connect(task, &ThumbnailTask::finished, this, &LibraryController::onThumbnailTaskFinished);
    task->setAutoDelete(true);
    QThreadPool::globalInstance()->start(task, highPriority ? 10 : 0);
}

void LibraryController::onScannerTaskFinished()
{
    emit libraryChanged();
}

void LibraryController::onOcrTaskFinished(int docId)
{
    Q_UNUSED(docId);
    emit libraryChanged();
}

void LibraryController::onThumbnailTaskFinished(int docId, const QString &thumbnailPath)
{
    emit thumbnailGenerated(docId, thumbnailPath);
}

