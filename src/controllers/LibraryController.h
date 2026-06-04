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

#include <QObject>
#include <QStringList>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QThreadPool>
#include <QVariantMap>
#include "../database/DatabaseManager.h"

class LibraryController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList watchedFolders READ watchedFolders NOTIFY watchedFoldersChanged)

public:
    explicit LibraryController(DatabaseManager *dbMgr, QObject *parent = nullptr);
    ~LibraryController();

    QStringList watchedFolders() const;

public slots:
    bool addWatchedFolder(const QString &folderPath);
    bool removeWatchedFolder(const QString &folderPath);
    
    // Batch operations executed inside SQL transactions
    bool batchUpdateTags(const QList<int> &documentIds, const QStringList &tags);
    bool batchUpdateRating(const QList<int> &documentIds, int rating);
    bool batchAddTags(const QList<int> &documentIds, const QStringList &tags);
    bool batchRemoveTags(const QList<int> &documentIds, const QStringList &tags);
    bool updateNotes(int documentId, const QString &notes);
    QStringList getUniqueTags() const;
    Q_INVOKABLE QVariantMap handleDroppedUrl(const QString &urlStr);
    Q_INVOKABLE void showInFinder(const QString &filePath);
    
    // Centralized sidecar read/write APIs
    bool writeSidecar(const QString &documentPath, const QStringList &tags, int rating, const QString &notes);
    bool readSidecar(const QString &documentPath, QStringList &tags, int &rating, QString &notes);

    // QML-facing thumbnail request
    void requestThumbnail(int docId, const QString &filePath, bool highPriority = false);

signals:
    void watchedFoldersChanged();
    void scanRequested(const QString &folderPath);
    void libraryChanged();
    void thumbnailGenerated(int docId, const QString &thumbnailPath);

private slots:
    void onDirectoryChanged(const QString &path);
    void triggerBackgroundCrawl();
    
    // Worker slots
    void onScanRequested(const QString &folderPath);
    void onOcrRequested(int docId, const QString &filePath);
    void onThumbnailRequested(int docId, const QString &filePath);
    
    void onScannerTaskFinished();
    void onOcrTaskFinished(int docId);
    void onThumbnailTaskFinished(int docId, const QString &thumbnailPath);

private:
    DatabaseManager *m_dbMgr;
    QFileSystemWatcher *m_watcher;
    QTimer *m_crawlTimer;
    QStringList m_watchedFoldersCache;
    QString m_sidecarDir;

    void updateFoldersCache();
    QString getSidecarPath(const QString &documentPath) const;
};

#endif // LIBRARYCONTROLLER_H
