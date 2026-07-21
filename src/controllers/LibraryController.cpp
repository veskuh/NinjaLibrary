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
#include <QEventLoop>
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
#include <QStandardPaths>
#include <QUrl>
#include <QtConcurrent>
#include <thread>
#include "../database/TagRepository.h"
#include "../database/WatchedFolderRepository.h"
#include "../database/DocumentRepository.h"

#include "../utils/DocUtils.h"
#include "../utils/PdfUtils.h"
#include "../workers/OcrTask.h"
#include "../workers/ScannerTask.h"
#include "../workers/ThumbnailTask.h"

#ifdef Q_OS_MAC
#include "../utils/MacBookmarks.h"
#endif

LibraryController::LibraryController(DatabaseManager *dbMgr, QObject *parent)
    : QObject(parent), m_dbMgr(dbMgr)
{
    m_taskManager = new ScanTaskManager(m_dbMgr, this);
    connect(m_taskManager, &ScanTaskManager::isScanningChanged, this, &LibraryController::isScanningChanged);
    connect(m_taskManager, &ScanTaskManager::scanProgressChanged, this, &LibraryController::scanProgressChanged);
    connect(m_taskManager, &ScanTaskManager::scanStatusTextChanged, this, &LibraryController::scanStatusTextChanged);
    connect(m_taskManager, &ScanTaskManager::isScanPausedChanged, this, &LibraryController::isScanPausedChanged);
    connect(m_taskManager, &ScanTaskManager::thumbnailGenerated, this, &LibraryController::thumbnailGenerated);
    connect(m_taskManager, &ScanTaskManager::scannerTaskFinished, this, &LibraryController::onScannerTaskFinished);
    connect(m_taskManager, &ScanTaskManager::postScanFinished, this, &LibraryController::postScanFinished);
    connect(m_taskManager, &ScanTaskManager::libraryChanged, this, &LibraryController::libraryChanged);

    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this,
            &LibraryController::onDirectoryChanged);

    // Hybrid background crawler timer (30-minute interval)
    m_crawlTimer = new QTimer(this);
    connect(m_crawlTimer, &QTimer::timeout, this, &LibraryController::triggerBackgroundCrawl);
    m_crawlTimer->start(30 * 60 * 1000);  // 30 minutes in milliseconds

    connect(this, &LibraryController::scanRequested, m_taskManager, &ScanTaskManager::requestScan);

    m_startupResumeTimer = new QTimer(this);
    connect(m_startupResumeTimer, &QTimer::timeout, this, &LibraryController::processNextStartupResume);

    m_scanDebounceTimer = new QTimer(this);
    m_scanDebounceTimer->setSingleShot(true);
    connect(m_scanDebounceTimer, &QTimer::timeout, this, &LibraryController::processDirtyRoots);

    updateFoldersCache();

    // Resume scans that were active when the app quit (staggered)
    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    if (db.isOpen()) {
        m_pendingStartupResumes = WatchedFolderRepository::getActiveScans(db);
    }
    if (!m_pendingStartupResumes.isEmpty()) {
        m_startupResumeTimer->start(500); // Stagger scans by 500ms
    }
}

LibraryController::~LibraryController()
{
}

QStringList LibraryController::watchedFolders() const { return m_watchedFoldersCache; }

QStringList LibraryController::watchedDirectories() const
{
    return m_watcher ? m_watcher->directories() : QStringList();
}

bool LibraryController::isScanning() const { return m_taskManager->isScanning(); }

double LibraryController::scanProgress() const { return m_taskManager->scanProgress(); }

QString LibraryController::scanStatusText() const
{
    if (m_taskManager->isScannerActive()) {
        if (ScannerTask::s_lowDiskSpace) {
            return "Scanning Paused: Low Disk Space (< 500MB)";
        }
        if (isScanPaused()) {
            return QString("Scanning Paused: %1%").arg(qRound(scanProgress() * 100));
        }
        return QString("Scanning: %1%").arg(qRound(scanProgress() * 100));
    } else if (m_taskManager->activeOcrTasks() > 0) {
        return QString("Extracting Text: %1/%2")
            .arg(m_taskManager->totalOcrTasks() - m_taskManager->activeOcrTasks())
            .arg(m_taskManager->totalOcrTasks());
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

#ifdef Q_OS_MAC
    QByteArray bookmark = MacBookmarks::getBookmarkForUrl(absPath);
#else
    QByteArray bookmark;
#endif

    if (!WatchedFolderRepository::addWatchedFolder(db, absPath, bookmark)) {
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

    // Build possible paths to handle macOS NFC/NFD Unicode normalization discrepancies
    QStringList possiblePaths;
    possiblePaths << folderPath << absPath
                  << folderPath.normalized(QString::NormalizationForm_C)
                  << folderPath.normalized(QString::NormalizationForm_D)
                  << absPath.normalized(QString::NormalizationForm_C)
                  << absPath.normalized(QString::NormalizationForm_D);
    possiblePaths.removeDuplicates();

    QString matchedPath = absPath; // fallback
    int folderId = WatchedFolderRepository::findFolderIdAndMatchedPath(db, possiblePaths, matchedPath);

    if (folderId == -1) {
        qWarning() << "removeWatchedFolder: no matching watched folder for" << folderPath;
        return false;
    }

    // Cancel active/queued scanner task if exists and wait for database lock release
    m_taskManager->cancelScanForFolder(matchedPath);

    // 2. Collect document IDs to delete BEFORE starting any write queries or transactions
    QList<int> docsToDelete = DocumentRepository::getDocIdsByFolderId(db, folderId);

    // 3. Execute all deletions inside a clean database transaction
    if (!db.transaction()) {
        qWarning() << "removeWatchedFolder: failed to begin transaction";
        return false;
    }

    WatchedFolderRepository::removeActiveScan(db, matchedPath);

    if (!WatchedFolderRepository::removeWatchedFolder(db, matchedPath)) {
        db.rollback();
        return false;
    }

    for (int docId : docsToDelete) {
        if (!DatabaseManager::deleteDocumentCascade(db, docId)) {
            db.rollback();
            return false;
        }
    }

    if (!db.commit()) {
        qWarning() << "Failed to commit removeWatchedFolder transaction:" << db.lastError().text();
        db.rollback();
        return false;
    }

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
    QString sidecarPath = m_sidecarManager.getSidecarPath(localPath);
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

    db.transaction();
    if (!DocumentRepository::batchUpdateTags(db, documentIds, tags)) {
        db.rollback();
        return false;
    }

    for (int docId : documentIds) {
        SidecarInputs inputs = SidecarManager::loadSidecarInputs(docId, db);
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
    if (!DocumentRepository::batchAddTags(db, documentIds, tags)) {
        db.rollback();
        return false;
    }

    for (int docId : documentIds) {
        SidecarInputs inputs = SidecarManager::loadSidecarInputs(docId, db);
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
    if (!DocumentRepository::batchRemoveTags(db, documentIds, tags)) {
        db.rollback();
        return false;
    }

    for (int docId : documentIds) {
        SidecarInputs inputs = SidecarManager::loadSidecarInputs(docId, db);
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
    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    return DocumentRepository::getUniqueTags(db);
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
            docId = DocumentRepository::getDocIdByPath(db, docPath);
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
    if (!DocumentRepository::batchUpdateRating(db, documentIds, rating)) {
        db.rollback();
        return false;
    }

    for (int docId : documentIds) {
        SidecarInputs inputs = SidecarManager::loadSidecarInputs(docId, db);
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
    if (!DocumentRepository::updateNotes(db, docId, notes)) {
        db.rollback();
        return false;
    }

    SidecarInputs inputs = SidecarManager::loadSidecarInputs(docId, db);
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
    return m_sidecarManager.writeSidecar(documentPath, tags, rating, notes);
}

bool LibraryController::readSidecar(const QString &documentPath, QStringList &tags, int &rating,
                                    QString &notes)
{
    return m_sidecarManager.readSidecar(documentPath, tags, rating, notes);
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
        m_dirtyRoots.insert(topLevelPath);
    } else {
        m_dirtyRoots.insert(path);
    }
    
    // Coalesce filesystem events with a 3 second quiet period
    m_scanDebounceTimer->start(3000);
}

void LibraryController::processDirtyRoots()
{
    for (const QString &root : std::as_const(m_dirtyRoots)) {
        emit scanRequested(root);
    }
    m_dirtyRoots.clear();
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

    QStringList folders = WatchedFolderRepository::getWatchedFolders(db);
    for (const QString &path : folders) {
        QString resolvedPath = path;
#ifdef Q_OS_MAC
        // Attempt bookmark resolution if available
        QByteArray bookmark = WatchedFolderRepository::getBookmark(db, path);
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
        for (const auto &pair : std::as_const(pathsToUpdate)) {
            if (WatchedFolderRepository::updateFolderPath(db, pair.first, pair.second)) {
                qDebug() << "Updated watched folder path in DB from" << pair.first << "to" << pair.second;
            }
        }
        db.commit();
    }

    emit watchedFoldersChanged();
}

QStringList LibraryController::collectSubdirectories(const QString &folderPath, const QPointer<LibraryController> &self)
{
    QStringList paths;
    if (folderPath.isEmpty() || !QDir(folderPath).exists()) {
        return paths;
    }

    paths.append(folderPath);

    QDirIterator it(folderPath, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        if (!self) { // Controller destroyed
            return paths;
        }
        
        QString subDirPath = it.next();
        QFileInfo dirInfo(subDirPath);
        QString absSubDirPath = dirInfo.canonicalFilePath();
        if (absSubDirPath.isEmpty()) {
            absSubDirPath = dirInfo.absoluteFilePath();
        }

        // Filter out unwanted directories (same logic as in ScannerTask)
        if (!DocUtils::isInsideIgnoredDir(absSubDirPath)) {
            paths.append(absSubDirPath);
        }
    }
    return paths;
}

void LibraryController::onScannerTaskFinished(const QString &folderPath, bool runCleanup)
{
    // Clean up watched subdirectories that no longer exist on disk
    QStringList watchedPaths = m_watcher->directories();
    QStringList pathsToRemove;
    for (const QString &path : watchedPaths) {
        if (path.startsWith(folderPath + "/") && !QDir(path).exists()) {
            pathsToRemove.append(path);
        }
    }
    if (!pathsToRemove.isEmpty()) {
        m_watcher->removePaths(pathsToRemove);
    }

    // Run cleanupSidecars and recursive directory scan on background thread (post-scan pool)
    QPointer<LibraryController> self(this);
    QRunnable *task = QRunnable::create([self, folderPath, runCleanup]() {
        if (!self) return;

        if (runCleanup) {
            self->cleanupSidecars();
        }

        if (!self) return;

        QStringList pathsToWatch = collectSubdirectories(folderPath, self);

        if (!self) return;

        QMetaObject::invokeMethod(self.data(), [self, pathsToWatch]() {
            if (!self) return;
            if (!pathsToWatch.isEmpty()) {
                self->m_watcher->addPaths(pathsToWatch);
            }
            emit self->postScanFinished();
        }, Qt::QueuedConnection);
    });
    m_taskManager->postScanThreadPool()->start(task, -10);
}

void LibraryController::watchFolderRecursively(const QString &folderPath)
{
    QPointer<LibraryController> self(this);
    QRunnable *task = QRunnable::create([self, folderPath]() {
        if (!self) return;
        
        QStringList paths = collectSubdirectories(folderPath, self);
        
        if (!self) return;
        
        const int batchSize = 500;
        for (int i = 0; i < paths.size(); i += batchSize) {
            if (!self) return;
            QStringList batch = paths.mid(i, batchSize);
            QMetaObject::invokeMethod(self.data(), "addWatcherPathsBatch", Qt::QueuedConnection,
                                      Q_ARG(QStringList, batch));
        }
    });
    m_taskManager->postScanThreadPool()->start(task);
}

void LibraryController::addWatcherPathsBatch(const QStringList &batch)
{
    if (!batch.isEmpty()) {
        QStringList failed = m_watcher->addPaths(batch);
        if (!failed.isEmpty()) {
            qWarning() << "QFileSystemWatcher failed to watch" << failed.size() << "directories out of a batch of" << batch.size();
        }
    }
}

void LibraryController::requestThumbnail(int docId, const QString &filePath, bool highPriority)
{
    m_taskManager->requestThumbnail(docId, filePath, highPriority);
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
    return DocumentRepository::searchDocumentContent(db, docId, query);
}

QVariantList LibraryController::searchDocuments(const QString &queryStr)
{
    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    return DocumentRepository::searchDocuments(db, queryStr);
}

QString LibraryController::getDocumentText(int docId) const
{
    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    return DocumentRepository::getDocumentText(db, docId);
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

    if (!DocumentRepository::markDocumentOpened(db, docId)) {
        return false;
    }

    emit libraryChanged();
    return true;
}

void LibraryController::cleanupSidecars()
{
    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    m_sidecarManager.cleanupOrphanSidecars(db);
}

bool LibraryController::isScanPaused() const { return m_taskManager->isScanPaused(); }

void LibraryController::pauseScan()
{
    m_taskManager->pauseScan();
}

void LibraryController::resumeScan()
{
    m_taskManager->resumeScan();
}

void LibraryController::toggleScanPause()
{
    m_taskManager->toggleScanPause();
}

void LibraryController::processNextStartupResume()
{
    if (m_pendingStartupResumes.isEmpty()) {
        m_startupResumeTimer->stop();
        return;
    }
    QString folderPath = m_pendingStartupResumes.takeFirst();
    emit scanRequested(folderPath);
    if (m_pendingStartupResumes.isEmpty()) {
        m_startupResumeTimer->stop();
    }
}
