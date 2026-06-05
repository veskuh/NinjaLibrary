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
#include "../workers/ScannerTask.h"
#include <QStandardPaths>
#include "../workers/OcrTask.h"
#include "../workers/ThumbnailTask.h"
#include "../utils/DocUtils.h"
#include <QDir>
#include <QSqlQuery>
#include <QSqlError>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QUrl>
#include <QFileInfo>
#include <QProcess>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QClipboard>
#include <thread>

#ifdef Q_OS_MAC
#include "../utils/MacBookmarks.h"
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
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (dataDir.isEmpty()) {
        dataDir = QDir::homePath() + "/.local/share/NinjaLibrary";
    }
    m_sidecarDir = dataDir + "/sidecars/";
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

bool LibraryController::isScanning() const
{
    return m_isScanning || m_activeOcrTasks > 0;
}

double LibraryController::scanProgress() const
{
    if (m_isScanning) {
        return m_scanProgress;
    } else if (m_totalOcrTasks > 0) {
        return static_cast<double>(m_totalOcrTasks - m_activeOcrTasks) / m_totalOcrTasks;
    }
    return 0.0;
}

QString LibraryController::scanStatusText() const
{
    if (m_isScanning) {
        return QString("Scanning: %1%").arg(qRound(m_scanProgress * 100));
    } else if (m_activeOcrTasks > 0) {
        return QString("Extracting Text: %1/%2").arg(m_totalOcrTasks - m_activeOcrTasks).arg(m_totalOcrTasks);
    }
    return "";
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

    db.transaction();

    // 1. Get folder ID from database
    int folderId = -1;
    QSqlQuery idQuery(db);
    idQuery.prepare("SELECT id FROM watched_folders WHERE absolute_path = :path;");
    idQuery.bindValue(":path", absPath);
    if (idQuery.exec() && idQuery.next()) {
        folderId = idQuery.value(0).toInt();
    }

    if (folderId != -1) {
        // 2. Delete search index entries first (since there is no foreign key constraint on virtual FTS5 table)
        QSqlQuery deleteSearch(db);
        deleteSearch.prepare("DELETE FROM document_search WHERE document_id IN (SELECT id FROM documents WHERE folder_id = :folderId);");
        deleteSearch.bindValue(":folderId", folderId);
        if (!deleteSearch.exec()) {
            qWarning() << "Failed to clean up document search index on stopwatching:" << deleteSearch.lastError().text();
        }

        // 3. Delete document tags first
        QSqlQuery deleteTags(db);
        deleteTags.prepare("DELETE FROM document_tags WHERE document_id IN (SELECT id FROM documents WHERE folder_id = :folderId);");
        deleteTags.bindValue(":folderId", folderId);
        if (!deleteTags.exec()) {
            qWarning() << "Failed to clean up document tags on stopwatching:" << deleteTags.lastError().text();
        }

        // 4. Delete documents
        QSqlQuery deleteDocs(db);
        deleteDocs.prepare("DELETE FROM documents WHERE folder_id = :folderId;");
        deleteDocs.bindValue(":folderId", folderId);
        if (!deleteDocs.exec()) {
            qWarning() << "Failed to delete documents on stopwatching:" << deleteDocs.lastError().text();
            db.rollback();
            return false;
        }
    }

    // 5. Delete the watched folder record
    QSqlQuery query(db);
    query.prepare("DELETE FROM watched_folders WHERE absolute_path = :path;");
    query.bindValue(":path", absPath);

    if (!query.exec()) {
        qWarning() << "Failed to remove watched folder from database:" << query.lastError().text();
        db.rollback();
        return false;
    }

    db.commit();

    m_watcher->removePath(absPath);
    updateFoldersCache();

    emit libraryChanged();
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

bool LibraryController::batchAddTags(const QList<int> &documentIds, const QStringList &tags)
{
    if (documentIds.isEmpty() || tags.isEmpty()) return true;

    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    if (!db.isOpen()) return false;

    db.transaction();
    QSqlQuery query(db);

    for (int docId : documentIds) {
        for (const QString &tagName : tags) {
            if (tagName.trimmed().isEmpty()) continue;

            // Ensure tag exists globally
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
        
        // Fetch all current tags for this document to write the sidecar
        QSqlQuery fetchTags(db);
        fetchTags.prepare("SELECT t.name FROM tags t JOIN document_tags dt ON t.id = dt.tag_id WHERE dt.document_id = :docId;");
        fetchTags.bindValue(":docId", docId);
        QStringList allDocTags;
        if (fetchTags.exec()) {
            while (fetchTags.next()) {
                allDocTags << fetchTags.value(0).toString();
            }
        }
        
        // Fetch path, rating, notes for sidecar
        QSqlQuery docQuery(db);
        docQuery.prepare("SELECT absolute_path, star_rating FROM documents WHERE id = :docId;");
        docQuery.bindValue(":docId", docId);
        if (docQuery.exec() && docQuery.next()) {
            QString docPath = docQuery.value(0).toString();
            int rating = docQuery.value(1).toInt();
            
            QSqlQuery notesQuery(db);
            notesQuery.prepare("SELECT notes FROM document_search WHERE document_id = :docId;");
            notesQuery.bindValue(":docId", docId);
            QString notes;
            if (notesQuery.exec() && notesQuery.next()) {
                notes = notesQuery.value(0).toString();
            }
            writeSidecar(docPath, allDocTags, rating, notes);
        }
    }

    db.commit();
    emit libraryChanged();
    return true;
}

bool LibraryController::batchRemoveTags(const QList<int> &documentIds, const QStringList &tags)
{
    if (documentIds.isEmpty() || tags.isEmpty()) return true;

    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    if (!db.isOpen()) return false;

    db.transaction();
    QSqlQuery query(db);

    for (int docId : documentIds) {
        for (const QString &tagName : tags) {
            if (tagName.trimmed().isEmpty()) continue;

            // Get tag ID
            query.prepare("SELECT id FROM tags WHERE name = :name;");
            query.bindValue(":name", tagName.trimmed());
            if (!query.exec() || !query.next()) {
                continue; // Tag doesn't exist globally, nothing to remove
            }
            int tagId = query.value(0).toInt();

            // Delete link
            query.prepare("DELETE FROM document_tags WHERE document_id = :docId AND tag_id = :tagId;");
            query.bindValue(":docId", docId);
            query.bindValue(":tagId", tagId);
            if (!query.exec()) {
                db.rollback();
                return false;
            }
        }

        // Fetch all current tags for this document to write the sidecar
        QSqlQuery fetchTags(db);
        fetchTags.prepare("SELECT t.name FROM tags t JOIN document_tags dt ON t.id = dt.tag_id WHERE dt.document_id = :docId;");
        fetchTags.bindValue(":docId", docId);
        QStringList allDocTags;
        if (fetchTags.exec()) {
            while (fetchTags.next()) {
                allDocTags << fetchTags.value(0).toString();
            }
        }
        
        // Fetch path, rating, notes for sidecar
        QSqlQuery docQuery(db);
        docQuery.prepare("SELECT absolute_path, star_rating FROM documents WHERE id = :docId;");
        docQuery.bindValue(":docId", docId);
        if (docQuery.exec() && docQuery.next()) {
            QString docPath = docQuery.value(0).toString();
            int rating = docQuery.value(1).toInt();
            
            QSqlQuery notesQuery(db);
            notesQuery.prepare("SELECT notes FROM document_search WHERE document_id = :docId;");
            notesQuery.bindValue(":docId", docId);
            QString notes;
            if (notesQuery.exec() && notesQuery.next()) {
                notes = notesQuery.value(0).toString();
            }
            writeSidecar(docPath, allDocTags, rating, notes);
        }
    }

    db.commit();
    emit libraryChanged();
    return true;
}

QStringList LibraryController::getUniqueTags() const
{
    QStringList tags;
    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    if (!db.isOpen()) return tags;

    QSqlQuery query("SELECT DISTINCT t.name FROM tags t JOIN document_tags dt ON t.id = dt.tag_id ORDER BY t.name COLLATE NOCASE ASC;", db);
    while (query.next()) {
        tags << query.value(0).toString();
    }
    return tags;
}

QVariantMap LibraryController::handleDroppedUrl(const QString &urlStr)
{
    QVariantMap result;
    result["status"] = "error";
    result["isFolder"] = false;
    result["watchedFolder"] = "";
    result["docPath"] = "";
    result["docId"] = -1;

    QUrl url(urlStr);
    QString path = url.toLocalFile();
    if (path.isEmpty()) {
        path = urlStr;
    }

    QFileInfo fileInfo(path);
    if (!fileInfo.exists()) {
        qWarning() << "Dropped path does not exist:" << path;
        return result;
    }

    path = fileInfo.canonicalFilePath();
    if (path.isEmpty()) {
        path = fileInfo.absoluteFilePath();
    }
    fileInfo = QFileInfo(path);

    bool isFolder = fileInfo.isDir();
    QString watchedFolder;
    QString docPath;
    int docId = -1;

    if (isFolder) {
        watchedFolder = path;
        if (!addWatchedFolder(watchedFolder)) {
            qWarning() << "Failed to add watched folder:" << watchedFolder;
            return result;
        }
    } else {
        docPath = path;
        watchedFolder = fileInfo.absolutePath();
        if (!addWatchedFolder(watchedFolder)) {
            qWarning() << "Failed to add watched folder parent:" << watchedFolder;
            return result;
        }

        QSqlDatabase db = m_dbMgr->getDatabaseConnection();
        if (db.isOpen()) {
            QSqlQuery query(db);
            query.prepare("SELECT id FROM documents WHERE absolute_path = :path;");
            query.bindValue(":path", docPath);
            if (query.exec() && query.next()) {
                docId = query.value(0).toInt();
            }
        }
    }

    result["status"] = "success";
    result["isFolder"] = isFolder;
    result["watchedFolder"] = watchedFolder;
    result["docPath"] = docPath;
    result["docId"] = docId;
    return result;
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

    if (m_scanProgressMap.contains(absPath)) {
        m_pendingScanRequests[absPath] = true;
        return;
    }

    m_scanProgressMap.insert(absPath, qMakePair(0, 0));
    if (!m_isScanning) {
        m_isScanning = true;
        emit isScanningChanged();
        emit scanStatusTextChanged();
    }

    ScannerTask *task = new ScannerTask(m_dbMgr, absPath);
    connect(task, &ScannerTask::ocrRequested, this, &LibraryController::onOcrRequested);
    connect(task, &ScannerTask::thumbnailRequested, this, &LibraryController::onThumbnailRequested);
    connect(task, &ScannerTask::progress, this, &LibraryController::onScanProgress);
    connect(task, &ScannerTask::finished, this, &LibraryController::onScannerTaskFinished);
    task->setAutoDelete(true);
    QThreadPool::globalInstance()->start(task);
}

void LibraryController::onOcrRequested(int docId, const QString &filePath)
{
    if (m_activeOcrTasks == 0) {
        m_totalOcrTasks = 0;
        bool wasScanning = isScanning();
        m_activeOcrTasks++;
        m_totalOcrTasks++;
        if (!wasScanning) {
            emit isScanningChanged();
        }
    } else {
        m_activeOcrTasks++;
        m_totalOcrTasks++;
    }

    OcrTask *task = new OcrTask(m_dbMgr, docId, filePath);
    connect(task, &OcrTask::finished, this, &LibraryController::onOcrTaskFinished);
    task->setAutoDelete(true);
    QThreadPool::globalInstance()->start(task);

    emit scanProgressChanged();
    emit scanStatusTextChanged();
}

void LibraryController::onThumbnailRequested(int docId, const QString &filePath)
{
    requestThumbnail(docId, filePath, false);
}

void LibraryController::requestThumbnail(int docId, const QString &filePath, bool highPriority)
{
    if (DocUtils::isSupportedTextDocument(filePath)) {
        return;
    }
    ThumbnailTask *task = new ThumbnailTask(docId, filePath);
    connect(task, &ThumbnailTask::finished, this, &LibraryController::onThumbnailTaskFinished);
    task->setAutoDelete(true);
    QThreadPool::globalInstance()->start(task, highPriority ? 10 : 0);
}

void LibraryController::onScanProgress(const QString &folderPath, int processed, int total)
{
    m_scanProgressMap[folderPath] = qMakePair(processed, total);
    updateScanProgress();
}

void LibraryController::onScannerTaskFinished(const QString &folderPath)
{
    m_scanProgressMap.remove(folderPath);

    if (m_scanProgressMap.isEmpty()) {
        if (m_isScanning) {
            m_isScanning = false;
            if (m_activeOcrTasks == 0) {
                emit isScanningChanged();
            }
        }
        if (m_scanProgress != 0.0) {
            m_scanProgress = 0.0;
            emit scanProgressChanged();
        }
    } else {
        updateScanProgress();
    }

    emit scanStatusTextChanged();
    emit libraryChanged();

    // If there is a pending request for this folder, restart the scan
    if (m_pendingScanRequests.value(folderPath, false)) {
        m_pendingScanRequests.remove(folderPath);
        onScanRequested(folderPath);
    }
}

void LibraryController::updateScanProgress()
{
    int totalFiles = 0;
    int totalProcessed = 0;

    for (auto it = m_scanProgressMap.constBegin(); it != m_scanProgressMap.constEnd(); ++it) {
        totalProcessed += it.value().first;
        totalFiles += it.value().second;
    }

    double progress = 0.0;
    if (totalFiles > 0) {
        progress = static_cast<double>(totalProcessed) / totalFiles;
    }

    if (qAbs(m_scanProgress - progress) > 0.00001) {
        m_scanProgress = progress;
        emit scanProgressChanged();
        emit scanStatusTextChanged();
    }
}

void LibraryController::onOcrTaskFinished(int docId)
{
    Q_UNUSED(docId);
    
    if (m_activeOcrTasks > 0) {
        m_activeOcrTasks--;
        if (m_activeOcrTasks == 0) {
            m_totalOcrTasks = 0;
            emit isScanningChanged();
        }
        emit scanProgressChanged();
        emit scanStatusTextChanged();
    }

    emit libraryChanged();
}

void LibraryController::onThumbnailTaskFinished(int docId, const QString &thumbnailPath)
{
    emit thumbnailGenerated(docId, thumbnailPath);
}

void LibraryController::showInFinder(const QString &filePath)
{
#if defined(Q_OS_MAC)
    QProcess::startDetached("open", QStringList() << "-R" << filePath);
#elif defined(Q_OS_WIN)
    QProcess::startDetached("explorer.exe", QStringList() << "/select," << QDir::toNativeSeparators(filePath));
#else
    QFileInfo fi(filePath);
    QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absolutePath()));
#endif
}

void LibraryController::copyToClipboard(const QString &text)
{
    DocUtils::copyToClipboard(text);
}

bool LibraryController::markDocumentOpened(int docId)
{
    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    if (!db.isOpen()) return false;

    QSqlQuery query(db);
    query.prepare("UPDATE documents SET last_opened = :lastOpened WHERE id = :docId;");
    query.bindValue(":lastOpened", QDateTime::currentSecsSinceEpoch());
    query.bindValue(":docId", docId);
    if (!query.exec()) {
        qWarning() << "Failed to mark document opened:" << query.lastError().text();
        return false;
    }

    emit libraryChanged();
    return true;
}


