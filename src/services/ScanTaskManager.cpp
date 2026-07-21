/*
 * Copyright (c) 2026, NinjaLibrary
 * All rights reserved.
 */

#include "ScanTaskManager.h"

#include <QDebug>
#include <QDir>
#include <QEventLoop>

#include "../database/DatabaseManager.h"
#include "../database/WatchedFolderRepository.h"
#include "../utils/DocUtils.h"
#include "../workers/OcrTask.h"
#include "../workers/ScannerTask.h"
#include "../workers/ThumbnailTask.h"

ScanTaskManager::ScanTaskManager(DatabaseManager *dbMgr, QObject *parent)
    : QObject(parent), m_dbMgr(dbMgr)
{
    int maxThreads = qMax(1, (int)(std::thread::hardware_concurrency() / 2));
    QThreadPool::globalInstance()->setMaxThreadCount(maxThreads);

    m_thumbnailThreadPool.setMaxThreadCount(2);
    m_postScanThreadPool.setMaxThreadCount(1);
    m_scannerThreadPool.setMaxThreadCount(2);
    m_ocrThreadPool.setMaxThreadCount(qMax(1, (int)(std::thread::hardware_concurrency() / 4)));
}

ScanTaskManager::~ScanTaskManager()
{
    waitForWorkersForTesting();
}

double ScanTaskManager::scanProgress() const
{
    if (m_isScanning) {
        return m_scanProgress;
    } else if (m_totalOcrTasks > 0) {
        return static_cast<double>(m_totalOcrTasks - m_activeOcrTasks) / m_totalOcrTasks;
    }
    return 0.0;
}

bool ScanTaskManager::isScanPaused() const
{
    return ScannerTask::s_scanPaused;
}

void ScanTaskManager::requestScan(const QString &absPath)
{
    if (absPath.isEmpty()) return;

    if (m_activeScans.contains(absPath)) {
        m_pendingScanRequests[absPath] = true;
        return;
    }

    if (!m_isScanning) {
        m_isScanning = true;
        emit isScanningChanged(m_isScanning);
        emit scanStatusTextChanged("Scanning...");
    }

    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    if (db.isOpen()) {
        WatchedFolderRepository::recordActiveScan(db, absPath);
    }

    ScannerTask *task = new ScannerTask(m_dbMgr, absPath);
    m_activeScans[absPath].task = task;

    connect(task, &ScannerTask::ocrBatchRequested, this, &ScanTaskManager::onOcrBatchRequested);
    connect(task, &ScannerTask::thumbnailBatchRequested, this, &ScanTaskManager::onThumbnailBatchRequested);
    connect(task, &ScannerTask::progress, this, &ScanTaskManager::onScanProgress);
    connect(task, &ScannerTask::finished, this, &ScanTaskManager::onScannerTaskFinished);
    connect(task, &ScannerTask::lowDiskSpaceDetected, this, &ScanTaskManager::onLowDiskSpaceDetected);

    task->setAutoDelete(true);
    m_scannerThreadPool.start(task);
}

void ScanTaskManager::cancelScanForFolder(const QString &folderPath)
{
    if (m_activeScans.contains(folderPath)) {
        QPointer<ScannerTask> task = m_activeScans.value(folderPath).task;
        if (task) {
            task->cancel();
            if (m_scannerThreadPool.tryTake(task.data())) {
                delete task.data();
            } else {
                QEventLoop loop;
                connect(task.data(), &ScannerTask::finished, &loop, &QEventLoop::quit);
                if (task) {
                    loop.exec();
                }
            }
        }
        m_activeScans.remove(folderPath);
    }
}

void ScanTaskManager::requestThumbnail(int docId, const QString &filePath, bool highPriority)
{
    if (DocUtils::isSupportedTextDocument(filePath)) {
        return;
    }
    {
        QMutexLocker locker(&m_inFlightThumbnailsMutex);
        if (m_inFlightThumbnails.contains(docId)) {
            return;
        }
        m_inFlightThumbnails.insert(docId);
    }

    ThumbnailTask *task = new ThumbnailTask(m_dbMgr, docId, filePath);
    connect(task, &ThumbnailTask::finished, this, &ScanTaskManager::onThumbnailTaskFinished);
    task->setAutoDelete(true);
    m_thumbnailThreadPool.start(task, highPriority ? 10 : 0);
}

void ScanTaskManager::pauseScan()
{
    if (!ScannerTask::s_scanPaused) {
        ScannerTask::s_scanPaused = true;
        emit isScanPausedChanged(true);
        emit scanStatusTextChanged("Paused");
    }
}

void ScanTaskManager::resumeScan()
{
    if (ScannerTask::s_scanPaused) {
        ScannerTask::s_scanPaused = false;
        {
            QMutexLocker locker(&ScannerTask::s_pauseMutex);
            ScannerTask::s_pauseCondition.wakeAll();
        }

        for (auto it = m_activeScans.begin(); it != m_activeScans.end(); ++it) {
            if (!it.value().task) {
                ScannerTask *task = new ScannerTask(m_dbMgr, it.key());
                it.value().task = task;
                connect(task, &ScannerTask::finished, this, &ScanTaskManager::onScannerTaskFinished);
                connect(task, &ScannerTask::progress, this, &ScanTaskManager::onScanProgress);
                connect(task, &ScannerTask::ocrBatchRequested, this, &ScanTaskManager::onOcrBatchRequested);
                connect(task, &ScannerTask::thumbnailBatchRequested, this, &ScanTaskManager::onThumbnailBatchRequested);
                connect(task, &ScannerTask::lowDiskSpaceDetected, this, &ScanTaskManager::onLowDiskSpaceDetected);
                task->setAutoDelete(true);
                m_scannerThreadPool.start(task);
            }
        }

        emit isScanPausedChanged(false);
        emit scanStatusTextChanged(m_isScanning ? "Scanning..." : "Idle");
    }
}

void ScanTaskManager::toggleScanPause()
{
    if (isScanPaused()) {
        resumeScan();
    } else {
        pauseScan();
    }
}

void ScanTaskManager::waitForWorkersForTesting()
{
    m_scannerThreadPool.waitForDone();
    m_ocrThreadPool.waitForDone();
    m_thumbnailThreadPool.waitForDone();
    m_postScanThreadPool.waitForDone();
}

void ScanTaskManager::onOcrBatchRequested(const QList<QPair<int, QString>> &batch)
{
    if (batch.isEmpty()) return;

    bool wasScanning = isScanning();
    m_activeOcrTasks += batch.size();
    m_totalOcrTasks += batch.size();
    if (!wasScanning) {
        emit isScanningChanged(isScanning());
    }

    emit scanProgressChanged(scanProgress());
    emit scanStatusTextChanged("OCR processing...");

    for (const auto &item : batch) {
        OcrTask *task = new OcrTask(m_dbMgr, item.first, item.second);
        connect(task, &OcrTask::finished, this, &ScanTaskManager::onOcrTaskFinished);
        task->setAutoDelete(true);
        m_ocrThreadPool.start(task);
    }
}

void ScanTaskManager::onThumbnailBatchRequested(const QList<QPair<int, QString>> &batch)
{
    for (const auto &item : batch) {
        requestThumbnail(item.first, item.second, false);
    }
}

void ScanTaskManager::onScanProgress(const QString &folderPath, int processed, int total)
{
    if (m_activeScans.contains(folderPath)) {
        m_activeScans[folderPath].processed = processed;
        m_activeScans[folderPath].total = total;
    }
    updateScanProgress();

    if (!m_lastCoarseRefreshTimer.isValid() || m_lastCoarseRefreshTimer.elapsed() > 2000) {
        emit libraryChanged();
        m_lastCoarseRefreshTimer.restart();
    }
}

void ScanTaskManager::onScannerTaskFinished(const QString &folderPath)
{
    if (ScannerTask::s_scanPaused) {
        return;
    }

    m_activeScans.remove(folderPath);

    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    if (db.isOpen()) {
        WatchedFolderRepository::removeActiveScan(db, folderPath);
    }

    bool runCleanup = false;
    if (m_activeScans.isEmpty()) {
        m_lastCoarseRefreshTimer.invalidate();
        if (m_isScanning) {
            m_isScanning = false;
            if (m_activeOcrTasks == 0) {
                emit isScanningChanged(false);
            }
        }
        if (m_scanProgress != 0.0) {
            m_scanProgress = 0.0;
            emit scanProgressChanged(0.0);
        }
        runCleanup = true;
    } else {
        updateScanProgress();
    }

    emit scanStatusTextChanged(m_isScanning ? "Scanning..." : (m_activeOcrTasks > 0 ? "OCR processing..." : "Idle"));
    emit libraryChanged();
    emit scannerTaskFinished(folderPath, runCleanup);

    if (m_pendingScanRequests.value(folderPath, false)) {
        m_pendingScanRequests.remove(folderPath);
        requestScan(folderPath);
    }
}

void ScanTaskManager::onOcrTaskFinished(int docId)
{
    Q_UNUSED(docId);

    if (m_activeOcrTasks > 0) {
        m_activeOcrTasks--;
        if (m_activeOcrTasks == 0) {
            m_totalOcrTasks = 0;
            emit isScanningChanged(isScanning());
        }
        emit scanProgressChanged(scanProgress());
        emit scanStatusTextChanged("OCR processing...");
    }

    emit libraryChanged();
}

void ScanTaskManager::onThumbnailTaskFinished(int docId, const QString &thumbnailPath)
{
    {
        QMutexLocker locker(&m_inFlightThumbnailsMutex);
        m_inFlightThumbnails.remove(docId);
    }
    emit thumbnailGenerated(docId, thumbnailPath);
}

void ScanTaskManager::onLowDiskSpaceDetected()
{
    emit isScanPausedChanged(isScanPaused());
    emit scanStatusTextChanged("Low disk space");
}

void ScanTaskManager::updateScanProgress()
{
    int totalFiles = 0;
    int totalProcessed = 0;

    for (auto it = m_activeScans.constBegin(); it != m_activeScans.constEnd(); ++it) {
        totalProcessed += it.value().processed;
        totalFiles += it.value().total;
    }

    double progress = 0.0;
    if (totalFiles > 0) {
        progress = static_cast<double>(totalProcessed) / totalFiles;
    }

    if (qAbs(m_scanProgress - progress) > 0.00001) {
        m_scanProgress = progress;
        emit scanProgressChanged(m_scanProgress);
        emit scanStatusTextChanged(m_isScanning ? "Scanning..." : "Idle");
    }
}
