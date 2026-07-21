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

#ifndef LIBRARYCONTROLLER_H
#define LIBRARYCONTROLLER_H

#include <QFileSystemWatcher>
#include <QMap>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QStringList>
#include <QThreadPool>
#include <QTimer>
#include <QVariantMap>
#include <QElapsedTimer>
#include <QMutex>
#include <QPair>

#include "../database/DatabaseManager.h"

class ScannerTask;

class LibraryController : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(LibraryController)
    Q_PROPERTY(QStringList watchedFolders READ watchedFolders NOTIFY watchedFoldersChanged)
    Q_PROPERTY(bool isScanning READ isScanning NOTIFY isScanningChanged)
    Q_PROPERTY(double scanProgress READ scanProgress NOTIFY scanProgressChanged)
    Q_PROPERTY(QString scanStatusText READ scanStatusText NOTIFY scanStatusTextChanged)
    Q_PROPERTY(bool isScanPaused READ isScanPaused NOTIFY isScanPausedChanged)

public:
    explicit LibraryController(DatabaseManager *dbMgr, QObject *parent = nullptr);
    ~LibraryController();

    QStringList watchedFolders() const;
    bool isScanning() const;
    double scanProgress() const;
    QString scanStatusText() const;
    bool isScanPaused() const;

public slots:
    bool addWatchedFolder(const QString &folderPath);
    bool removeWatchedFolder(const QString &folderPath);
    bool moveToTrash(int documentId, const QString &filePath);

    // Batch operations executed inside SQL transactions
    bool batchUpdateTags(const QList<int> &documentIds, const QStringList &tags);
    bool batchUpdateRating(const QList<int> &documentIds, int rating);
    bool batchAddTags(const QList<int> &documentIds, const QStringList &tags);
    bool batchRemoveTags(const QList<int> &documentIds, const QStringList &tags);
    bool updateNotes(int documentId, const QString &notes);
    QStringList getUniqueTags() const;
    Q_INVOKABLE QVariantMap handleDroppedUrl(const QString &urlStr);
    Q_INVOKABLE void showInFinder(const QString &filePath);
    Q_INVOKABLE void copyToClipboard(const QString &text);
    Q_INVOKABLE QVariantList searchDocumentContent(int docId, const QString &absolutePath, const QString &query);
    Q_INVOKABLE QVariantList searchDocuments(const QString &queryStr);
    Q_INVOKABLE QString getDocumentText(int docId) const;
    Q_INVOKABLE QString readTextFile(const QString &filePath);
    Q_INVOKABLE QString fileTypeDescription(const QString &fileName) const;
    bool markDocumentOpened(int docId);

    // Centralized sidecar read/write APIs
    bool writeSidecar(const QString &documentPath, const QStringList &tags, int rating,
                      const QString &notes);
    bool readSidecar(const QString &documentPath, QStringList &tags, int &rating, QString &notes);
    void cleanupSidecars();

    // QML-facing thumbnail request
    void requestThumbnail(int docId, const QString &filePath, bool highPriority = false);

    // Pause/Resume API
    void pauseScan();
    void resumeScan();
    Q_INVOKABLE void toggleScanPause();

    Q_INVOKABLE QStringList watchedDirectories() const;

signals:
    void watchedFoldersChanged();
    void folderAdded(const QString &folderPath);
    void isScanningChanged();
    void scanProgressChanged();
    void scanStatusTextChanged();
    void scanRequested(const QString &folderPath);
    void libraryChanged();
    void libraryUpdated();
    void thumbnailGenerated(int docId, const QString &thumbnailPath);
    void isScanPausedChanged();
    void folderConflictDetected(const QString &message);
    void postScanFinished();

private slots:
    void onDirectoryChanged(const QString &path);
    void triggerBackgroundCrawl();

    // Worker slots
    void onScanRequested(const QString &folderPath);
    void onOcrBatchRequested(const QList<QPair<int, QString>> &batch);
    void onThumbnailBatchRequested(const QList<QPair<int, QString>> &batch);
    void onScannerTaskFinished(const QString &folderPath);
    void onScanProgress(const QString &folderPath, int processed, int total);
    void onLowDiskSpaceDetected();
    void onOcrTaskFinished(int docId);
    void onThumbnailTaskFinished(int docId, const QString &thumbnailPath);
    void processNextStartupResume();
    void addWatcherPathsBatch(const QStringList &paths);

private:
    DatabaseManager *m_dbMgr;
    QFileSystemWatcher *m_watcher;
    QTimer *m_crawlTimer;
    QStringList m_watchedFoldersCache;
    QString m_sidecarDir;

    static QStringList collectSubdirectories(const QString &folderPath, const QPointer<LibraryController> &self);
    void updateFoldersCache();
    void watchFolderRecursively(const QString &folderPath);
    QString getSidecarPath(const QString &documentPath) const;

private:
    bool m_isScanning = false;
    double m_scanProgress = 0.0;
    int m_activeOcrTasks = 0;
    int m_totalOcrTasks = 0;
    struct ActiveScan {
        QPointer<ScannerTask> task;
        int processed = 0;
        int total = 0;
    };
    QMap<QString, ActiveScan> m_activeScans;
    QMap<QString, bool> m_pendingScanRequests;
    QThreadPool m_thumbnailThreadPool;
    QThreadPool m_postScanThreadPool;
    QThreadPool m_scannerThreadPool;
    QThreadPool m_ocrThreadPool;
    
public:
    void waitForWorkersForTesting() {
        m_scannerThreadPool.waitForDone();
        m_ocrThreadPool.waitForDone();
        m_thumbnailThreadPool.waitForDone();
        m_postScanThreadPool.waitForDone();
    }
private:
    QSet<int> m_inFlightThumbnails;
    QMutex m_inFlightThumbnailsMutex;
    QStringList m_pendingStartupResumes;
    QTimer *m_startupResumeTimer;
    QElapsedTimer m_lastCoarseRefreshTimer;
    
    QTimer *m_scanDebounceTimer;
    QSet<QString> m_dirtyRoots;

private slots:
    void processDirtyRoots();

private:
    void updateScanProgress();
};

#endif  // LIBRARYCONTROLLER_H
