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

#include <QClipboard>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSaveFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUrl>
#include <thread>
#include "../database/TagRepository.h"

#include "../utils/DocUtils.h"
#include "../utils/PdfUtils.h"
#include "../workers/OcrTask.h"
#include "../workers/ScannerTask.h"
#include "../workers/ThumbnailTask.h"

#ifdef Q_OS_MAC
#include "../utils/MacBookmarks.h"
#endif

namespace {
struct SidecarInputs {
    QString docPath;
    QStringList tags;
    int rating = 0;
    QString notes;
    bool success = false;
};

SidecarInputs loadSidecarInputs(int docId, QSqlDatabase &db)
{
    SidecarInputs inputs;
    QSqlQuery docQuery(db);
    docQuery.prepare("SELECT absolute_path, star_rating FROM documents WHERE id = :docId;");
    docQuery.bindValue(":docId", docId);
    if (!docQuery.exec() || !docQuery.next()) {
        return inputs;
    }
    inputs.docPath = docQuery.value(0).toString();
    inputs.rating = docQuery.value(1).toInt();

    QSqlQuery tagQuery(db);
    tagQuery.prepare(
        "SELECT t.name FROM tags t JOIN document_tags dt ON t.id = dt.tag_id WHERE "
        "dt.document_id = :docId;");
    tagQuery.bindValue(":docId", docId);
    if (tagQuery.exec()) {
        while (tagQuery.next()) {
            inputs.tags << tagQuery.value(0).toString();
        }
    }

    QSqlQuery notesQuery(db);
    notesQuery.prepare("SELECT notes FROM document_search WHERE document_id = :docId;");
    notesQuery.bindValue(":docId", docId);
    if (notesQuery.exec() && notesQuery.next()) {
        inputs.notes = notesQuery.value(0).toString();
    }

    inputs.success = true;
    return inputs;
}
} // namespace

LibraryController::LibraryController(DatabaseManager *dbMgr, QObject *parent)
    : QObject(parent), m_dbMgr(dbMgr)
{
    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this,
            &LibraryController::onDirectoryChanged);

    // Hybrid background crawler timer (30-minute interval)
    m_crawlTimer = new QTimer(this);
    connect(m_crawlTimer, &QTimer::timeout, this, &LibraryController::triggerBackgroundCrawl);
    m_crawlTimer->start(30 * 60 * 1000);  // 30 minutes in milliseconds

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

    // Resume scans that were active when the app quit
    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    if (db.isOpen()) {
        QSqlQuery query("SELECT folder_path FROM active_scans;", db);
        while (query.next()) {
            QString folderPath = query.value(0).toString();
            emit scanRequested(folderPath);
        }
    }
}

LibraryController::~LibraryController() { resumeScan(); }

QStringList LibraryController::watchedFolders() const { return m_watchedFoldersCache; }

bool LibraryController::isScanning() const { return m_isScanning || m_activeOcrTasks > 0; }

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
        if (ScannerTask::s_lowDiskSpace) {
            return "Scanning Paused: Low Disk Space (< 500MB)";
        }
        if (isScanPaused()) {
            return QString("Scanning Paused: %1%").arg(qRound(m_scanProgress * 100));
        }
        return QString("Scanning: %1%").arg(qRound(m_scanProgress * 100));
    } else if (m_activeOcrTasks > 0) {
        return QString("Extracting Text: %1/%2")
            .arg(m_totalOcrTasks - m_activeOcrTasks)
            .arg(m_totalOcrTasks);
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

    // 1. Conflict detection: subfolder of an already watched directory
    for (const QString &parentPath : m_watchedFoldersCache) {
        if (absPath == parentPath) {
            return true;  // Already watched, return true to avoid duplicate errors
        }
        if (absPath.startsWith(parentPath + "/") || absPath.startsWith(parentPath + "\\")) {
            int lastSlash = parentPath.lastIndexOf('/');
            if (lastSlash == -1) {
                lastSlash = parentPath.lastIndexOf('\\');
            }
            QString folderName = (lastSlash != -1) ? parentPath.mid(lastSlash + 1) : parentPath;
            if (folderName.isEmpty()) {
                folderName = parentPath;
            }
            emit folderConflictDetected(
                QString("Folder is already monitored under: %1").arg(folderName));
            return false;
        }
    }

    // 2. Conflict detection / merging: parent of already watched directories
    QStringList childrenToRemove;
    for (const QString &childPath : m_watchedFoldersCache) {
        if (childPath.startsWith(absPath + "/") || childPath.startsWith(absPath + "\\")) {
            childrenToRemove.append(childPath);
        }
    }

    for (const QString &childPath : childrenToRemove) {
        removeWatchedFolder(childPath);
    }

    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    if (!db.isOpen()) return false;

    QSqlQuery query(db);
    query.prepare(
        "INSERT OR IGNORE INTO watched_folders (absolute_path, macos_bookmark) VALUES (:path, "
        ":bookmark);");
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
    emit folderAdded(absPath);
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

    // Build possible paths to handle macOS NFC/NFD Unicode normalization discrepancies
    QStringList possiblePaths;
    possiblePaths << folderPath << absPath
                  << folderPath.normalized(QString::NormalizationForm_C)
                  << folderPath.normalized(QString::NormalizationForm_D)
                  << absPath.normalized(QString::NormalizationForm_C)
                  << absPath.normalized(QString::NormalizationForm_D);
    possiblePaths.removeDuplicates();

    int folderId = -1;
    QString matchedPath = absPath; // fallback

    // 1. Get folder ID and exact stored path from database
    for (const QString &pathCandidate : std::as_const(possiblePaths)) {
        QSqlQuery idQuery(db);
        idQuery.prepare("SELECT id, absolute_path FROM watched_folders WHERE absolute_path = :path;");
        idQuery.bindValue(":path", pathCandidate);
        if (idQuery.exec() && idQuery.next()) {
            folderId = idQuery.value(0).toInt();
            matchedPath = idQuery.value(1).toString();
            break;
        }
    }

    if (folderId == -1) {
        qWarning() << "removeWatchedFolder: no matching watched folder for" << folderPath;
        db.rollback();
        return false;
    }

    // Delete active scan for this folder if exists
    {
        QSqlQuery removeScan(db);
        removeScan.prepare("DELETE FROM active_scans WHERE folder_path = :path;");
        removeScan.bindValue(":path", matchedPath);
        removeScan.exec();
    }

    if (folderId != -1) {
        QSqlQuery selectDocs(db);
        selectDocs.prepare("SELECT id FROM documents WHERE folder_id = :folderId;");
        selectDocs.bindValue(":folderId", folderId);
        if (selectDocs.exec()) {
            while (selectDocs.next()) {
                int docId = selectDocs.value(0).toInt();
                if (!DatabaseManager::deleteDocumentCascade(db, docId)) {
                    db.rollback();
                    return false;
                }
            }
        } else {
            qWarning() << "Failed to query documents on stopwatching:" << selectDocs.lastError().text();
            db.rollback();
            return false;
        }
    }

    // 5. Delete the watched folder record
    QSqlQuery query(db);
    query.prepare("DELETE FROM watched_folders WHERE absolute_path = :path;");
    query.bindValue(":path", matchedPath);

    if (!query.exec()) {
        qWarning() << "Failed to remove watched folder from database:" << query.lastError().text();
        db.rollback();
        return false;
    }

    db.commit();

    m_watcher->removePath(matchedPath);
    updateFoldersCache();
    cleanupSidecars();

    emit libraryChanged();
    return true;
}

bool LibraryController::moveToTrash(int documentId, const QString &filePath)
{
    QUrl url(filePath);
    QString localPath = url.isLocalFile() ? url.toLocalFile() : filePath;

    if (localPath.isEmpty() || !QFile::exists(localPath)) {
        qWarning() << "moveToTrash: File does not exist or path is empty:" << localPath;
        return false;
    }

    if (!QFile::moveToTrash(localPath)) {
        qWarning() << "moveToTrash: Failed to move file to trash:" << localPath;
        return false;
    }

    // Clean up sidecar file if exists
    QString sidecarPath = getSidecarPath(localPath);
    if (!sidecarPath.isEmpty() && QFile::exists(sidecarPath)) {
        QFile::remove(sidecarPath);
    }

    // Clean up cached thumbnail file if exists
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(localPath.toUtf8());
    QString hashStr = hash.result().toHex();
    QString cacheDir =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/thumbnails/";
    QString thumbPath = cacheDir + hashStr + ".png";
    if (QFile::exists(thumbPath)) {
        QFile::remove(thumbPath);
    }

    // Now remove from database
    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    if (!db.isOpen()) return false;

    db.transaction();

    if (!DatabaseManager::deleteDocumentCascade(db, documentId)) {
        db.rollback();
        return false;
    }

    db.commit();
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
            if (!TagRepository::ensureTagLinked(db, docId, tagName)) {
                db.rollback();
                return false;
            }
        }

        // Load notes & rating to update the centralized sidecar if sidecar is active
        SidecarInputs inputs = loadSidecarInputs(docId, db);
        if (inputs.success) {
            writeSidecar(inputs.docPath, tags, inputs.rating, inputs.notes);
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
            if (!TagRepository::ensureTagLinked(db, docId, tagName)) {
                db.rollback();
                return false;
            }
        }

        SidecarInputs inputs = loadSidecarInputs(docId, db);
        if (inputs.success) {
            writeSidecar(inputs.docPath, inputs.tags, inputs.rating, inputs.notes);
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
                continue;  // Tag doesn't exist globally, nothing to remove
            }
            int tagId = query.value(0).toInt();

            // Delete link
            query.prepare(
                "DELETE FROM document_tags WHERE document_id = :docId AND tag_id = :tagId;");
            query.bindValue(":docId", docId);
            query.bindValue(":tagId", tagId);
            if (!query.exec()) {
                db.rollback();
                return false;
            }
        }

        SidecarInputs inputs = loadSidecarInputs(docId, db);
        if (inputs.success) {
            writeSidecar(inputs.docPath, inputs.tags, inputs.rating, inputs.notes);
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

    QSqlQuery query(
        "SELECT DISTINCT t.name FROM tags t JOIN document_tags dt ON t.id = dt.tag_id ORDER BY "
        "t.name COLLATE NOCASE ASC;",
        db);
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

        SidecarInputs inputs = loadSidecarInputs(docId, db);
        if (inputs.success) {
            writeSidecar(inputs.docPath, inputs.tags, inputs.rating, inputs.notes);
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

    SidecarInputs inputs = loadSidecarInputs(docId, db);
    if (inputs.success) {
        writeSidecar(inputs.docPath, inputs.tags, inputs.rating, inputs.notes);
    }

    db.commit();
    emit libraryChanged();
    return true;
}

bool LibraryController::writeSidecar(const QString &documentPath, const QStringList &tags,
                                     int rating, const QString &notes)
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
    QSaveFile file(sidecarPath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(doc.toJson());
    return file.commit();
}

bool LibraryController::readSidecar(const QString &documentPath, QStringList &tags, int &rating,
                                    QString &notes)
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
    // Find the top-level watched folder containing this path
    QString topLevelPath;
    for (const QString &watchedFolder : m_watchedFoldersCache) {
        if (path == watchedFolder || path.startsWith(watchedFolder + "/")) {
            if (topLevelPath.isEmpty() || watchedFolder.length() > topLevelPath.length()) {
                topLevelPath = watchedFolder;
            }
        }
    }

    if (!topLevelPath.isEmpty()) {
        emit scanRequested(topLevelPath);
    } else {
        emit scanRequested(path);
    }
}

void LibraryController::triggerBackgroundCrawl()
{
    // 30-minute crawler sweep of all watched folders to handle deep changes not captured by watch
    // limit.
    for (const QString &folder : m_watchedFoldersCache) {
        emit scanRequested(folder);
    }
}

void LibraryController::updateFoldersCache()
{
#ifdef Q_OS_MAC
    MacBookmarks::releaseAllBookmarkAccesses();
#endif

    // Clear all watched directories first to avoid orphaned watches
    QStringList currentDirs = m_watcher->directories();
    if (!currentDirs.isEmpty()) {
        m_watcher->removePaths(currentDirs);
    }

    m_watchedFoldersCache.clear();
    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    if (!db.isOpen()) return;

    QList<QPair<QString, QString>> pathsToUpdate;

    QSqlQuery query("SELECT absolute_path, macos_bookmark FROM watched_folders;", db);
    while (query.next()) {
        QString path = query.value(0).toString();
        QByteArray bookmark = query.value(1).toByteArray();

        QString resolvedPath = path;
#ifdef Q_OS_MAC
        if (!bookmark.isEmpty()) {
            QString tmpResolved;
            if (MacBookmarks::resolveAndAccessBookmark(bookmark, tmpResolved)) {
                resolvedPath = tmpResolved;
                if (resolvedPath != path) {
                    pathsToUpdate.append({path, resolvedPath});
                }
            }
        }
#endif
        m_watchedFoldersCache.append(resolvedPath);
        // Ensure path and its subdirectories are watched
        watchFolderRecursively(resolvedPath);
    }

    if (!pathsToUpdate.isEmpty()) {
        db.transaction();
        QSqlQuery updateQuery(db);
        updateQuery.prepare("UPDATE watched_folders SET absolute_path = :newPath WHERE absolute_path = :oldPath;");
        for (const auto &pair : std::as_const(pathsToUpdate)) {
            updateQuery.bindValue(":newPath", pair.second);
            updateQuery.bindValue(":oldPath", pair.first);
            if (!updateQuery.exec()) {
                qWarning() << "Failed to update watched folder path after bookmark resolution:" << updateQuery.lastError().text();
            } else {
                qDebug() << "Updated watched folder path in DB from" << pair.first << "to" << pair.second;
            }
        }
        db.commit();
    }

    emit watchedFoldersChanged();
}

void LibraryController::watchFolderRecursively(const QString &folderPath)
{
    if (folderPath.isEmpty() || !QDir(folderPath).exists()) {
        return;
    }

    m_watcher->addPath(folderPath);

    QDirIterator it(folderPath, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString subDirPath = it.next();
        QFileInfo dirInfo(subDirPath);
        QString absSubDirPath = dirInfo.canonicalFilePath();
        if (absSubDirPath.isEmpty()) {
            absSubDirPath = dirInfo.absoluteFilePath();
        }

        // Filter out unwanted directories (same logic as in ScannerTask)
        if (!DocUtils::isInsideIgnoredDir(absSubDirPath)) {
            m_watcher->addPath(absSubDirPath);
        }
    }
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

    // Record active scan in database
    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    if (db.isOpen()) {
        QSqlQuery recordScan(db);
        recordScan.prepare("INSERT OR IGNORE INTO active_scans (folder_path) VALUES (:path);");
        recordScan.bindValue(":path", absPath);
        if (!recordScan.exec()) {
            qWarning() << "Failed to record active scan in database:"
                       << recordScan.lastError().text();
        }
    }

    ScannerTask *task = new ScannerTask(m_dbMgr, absPath);
    connect(task, &ScannerTask::ocrRequested, this, &LibraryController::onOcrRequested);
    connect(task, &ScannerTask::thumbnailRequested, this, &LibraryController::onThumbnailRequested);
    connect(task, &ScannerTask::progress, this, &LibraryController::onScanProgress);
    connect(task, &ScannerTask::finished, this, &LibraryController::onScannerTaskFinished);
    connect(task, &ScannerTask::lowDiskSpaceDetected, this,
            &LibraryController::onLowDiskSpaceDetected);
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
    ThumbnailTask *task = new ThumbnailTask(m_dbMgr, docId, filePath);
    connect(task, &ThumbnailTask::finished, this, &LibraryController::onThumbnailTaskFinished);
    task->setAutoDelete(true);
    QThreadPool::globalInstance()->start(task, highPriority ? 10 : 0);
}

void LibraryController::onScanProgress(const QString &folderPath, int processed, int total)
{
    m_scanProgressMap[folderPath] = qMakePair(processed, total);
    updateScanProgress();
    emit libraryUpdated();
}

void LibraryController::onScannerTaskFinished(const QString &folderPath)
{
    m_scanProgressMap.remove(folderPath);

    // Remove active scan record from database
    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    if (db.isOpen()) {
        QSqlQuery removeScan(db);
        removeScan.prepare("DELETE FROM active_scans WHERE folder_path = :path;");
        removeScan.bindValue(":path", folderPath);
        if (!removeScan.exec()) {
            qWarning() << "Failed to remove active scan record:" << removeScan.lastError().text();
        }
    }

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
        cleanupSidecars();
    } else {
        updateScanProgress();
    }

    // Clean up watched subdirectories that no longer exist on disk
    QStringList watchedPaths = m_watcher->directories();
    for (const QString &path : watchedPaths) {
        if (path.startsWith(folderPath + "/") && !QDir(path).exists()) {
            m_watcher->removePath(path);
        }
    }
    // Watch any new subdirectories recursively
    watchFolderRecursively(folderPath);

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

void LibraryController::onLowDiskSpaceDetected()
{
    emit isScanPausedChanged();
    emit scanStatusTextChanged();
}

void LibraryController::showInFinder(const QString &filePath)
{
#if defined(Q_OS_MAC)
    QProcess::startDetached("open", QStringList() << "-R" << filePath);
#elif defined(Q_OS_WIN)
    QProcess::startDetached("explorer.exe",
                            QStringList() << "/select," << QDir::toNativeSeparators(filePath));
#else
    QFileInfo fi(filePath);
    QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absolutePath()));
#endif
}

void LibraryController::copyToClipboard(const QString &text) { DocUtils::copyToClipboard(text); }

QVariantList LibraryController::searchDocumentContent(int docId, const QString &absolutePath, const QString &query)
{
    QVariantList results;
    if (query.trimmed().isEmpty()) {
        return results;
    }

    if (absolutePath.toLower().endsWith(".pdf")) {
        return PdfUtils::searchPdfPages(absolutePath, query);
    }

    // Non-PDF fallback (plain text, markdown, docx etc. treat as single page 0)
    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    if (db.isOpen()) {
        QSqlQuery q(db);
        q.prepare("SELECT text_snippet FROM document_search WHERE document_id = :docId;");
        q.bindValue(":docId", docId);
        if (q.exec() && q.next()) {
            QString pageText = q.value(0).toString();
            int index = 0;
            while ((index = pageText.indexOf(query, index, Qt::CaseInsensitive)) != -1) {
                QVariantMap map;
                map["pageIndex"] = 0;
                
                int start = qMax(0, index - 100);
                int end = qMin(pageText.length(), index + query.length() + 100);
                QString prefix = (start > 0) ? "..." : "";
                QString suffix = (end < pageText.length()) ? "..." : "";
                map["context"] = prefix + pageText.mid(start, end - start).replace('\n', ' ') + suffix;
                
                results.append(map);
                index += query.length();
                if (results.size() >= 100) break;
            }
        }
    }
    return results;
}

QVariantList LibraryController::searchDocuments(const QString &queryStr)
{
    QVariantList results;
    QString trimmed = queryStr.trimmed();
    if (trimmed.isEmpty()) {
        return results;
    }

    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    if (!db.isOpen()) return results;

    QSqlQuery query(db);
    query.prepare(
        "SELECT d.id, d.file_name, d.absolute_path, d.file_size, d.is_offline, d.star_rating, ds.text_snippet, "
        "       (SELECT group_concat(t.name) FROM document_tags dt JOIN tags t ON dt.tag_id = t.id WHERE dt.document_id = d.id) as tags "
        "FROM documents d "
        "JOIN document_search ds ON d.id = ds.document_id "
        "WHERE d.id IN (SELECT document_id FROM document_search WHERE document_search MATCH :ftsQuery) "
        "ORDER BY d.file_name ASC;"
    );

    QStringList terms = trimmed.split(" ", Qt::SkipEmptyParts);
    QString ftsQuery;
    for (int i = 0; i < terms.size(); ++i) {
        if (i > 0) ftsQuery += " AND ";
        QString term = terms[i];
        // Remove characters that might break FTS5 parser
        term.replace("\"", "").replace("'", "").replace("*", "").replace(":", "");
        if (!term.isEmpty()) {
            ftsQuery += term + "*";
        }
    }

    if (ftsQuery.isEmpty()) {
        return results;
    }

    query.bindValue(":ftsQuery", ftsQuery);

    if (query.exec()) {
        while (query.next()) {
            QVariantMap doc;
            doc["docId"] = query.value(0).toInt();
            doc["id"] = query.value(0).toInt();
            doc["fileName"] = query.value(1).toString();
            doc["absolutePath"] = query.value(2).toString();
            
            qint64 size = query.value(3).toLongLong();
            doc["fileSize"] = size;
            QString sizeStr;
            if (size < 1024) sizeStr = QString("%1 B").arg(size);
            else if (size < 1024 * 1024) sizeStr = QString("%1 KB").arg(size / 1024);
            else sizeStr = QString("%1 MB").arg(double(size) / (1024 * 1024), 0, 'f', 1);
            doc["fileSizeStr"] = sizeStr;

            doc["isOffline"] = query.value(4).toBool();
            doc["starRating"] = query.value(5).toInt();
            
            QString textSnippet = query.value(6).toString();
            QString tagsConcat = query.value(7).toString();
            QStringList tags = tagsConcat.isEmpty() ? QStringList() : tagsConcat.split(",");
            doc["tags"] = tags;
            
            int matchCount = 0;
            if (!terms.isEmpty()) {
                QString firstTerm = terms[0];
                int idx = 0;
                while ((idx = textSnippet.indexOf(firstTerm, idx, Qt::CaseInsensitive)) != -1) {
                    matchCount++;
                    idx += firstTerm.length();
                }
            }
            doc["matchCount"] = matchCount;
            
            results.append(doc);
        }
    } else {
        qWarning() << "searchDocuments FTS5 query failed:" << query.lastError().text();
    }

    return results;
}

QString LibraryController::readTextFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

QString LibraryController::fileTypeDescription(const QString &fileName) const
{
    return DocUtils::fileTypeDescription(fileName);
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

void LibraryController::cleanupSidecars()
{
    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    if (!db.isOpen()) return;

    QSet<QString> activeHashes;
    QSqlQuery query("SELECT absolute_path FROM documents;", db);
    while (query.next()) {
        QString docPath = query.value(0).toString();
        QCryptographicHash hasher(QCryptographicHash::Sha256);
        hasher.addData(docPath.toUtf8());
        activeHashes.insert(hasher.result().toHex());
    }

    QDir dir(m_sidecarDir);
    QStringList filters;
    filters << "*.ninja";
    QStringList files = dir.entryList(filters, QDir::Files);
    for (const QString &filename : files) {
        QString baseName = QFileInfo(filename).baseName();
        if (!activeHashes.contains(baseName)) {
            dir.remove(filename);
        }
    }
}

bool LibraryController::isScanPaused() const { return ScannerTask::s_scanPaused; }

void LibraryController::pauseScan()
{
    if (!ScannerTask::s_scanPaused) {
        ScannerTask::s_scanPaused = true;
        emit isScanPausedChanged();
        emit scanStatusTextChanged();
    }
}

void LibraryController::resumeScan()
{
    if (ScannerTask::s_scanPaused) {
        ScannerTask::s_scanPaused = false;
        {
            QMutexLocker locker(&ScannerTask::s_pauseMutex);
            ScannerTask::s_pauseCondition.wakeAll();
        }
        emit isScanPausedChanged();
        emit scanStatusTextChanged();
    }
}

void LibraryController::toggleScanPause()
{
    if (isScanPaused()) {
        resumeScan();
    } else {
        pauseScan();
    }
}
