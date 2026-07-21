/*
 * Copyright (c) 2026, NinjaLibrary
 * All rights reserved.
 */

#ifndef SCANTASKMANAGER_H
#define SCANTASKMANAGER_H

#include <QElapsedTimer>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QStringList>
#include <QThreadPool>
#include <QPair>

class DatabaseManager;
class ScannerTask;

class ScanTaskManager : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ScanTaskManager)

public:
    explicit ScanTaskManager(DatabaseManager *dbMgr, QObject *parent = nullptr);
    ~ScanTaskManager() override;

    bool isScanning() const { return m_isScanning || m_activeOcrTasks > 0; }
    bool isScannerActive() const { return m_isScanning; }
    int activeOcrTasks() const { return m_activeOcrTasks; }
    int totalOcrTasks() const { return m_totalOcrTasks; }
    double scanProgress() const;
    bool isScanPaused() const;

    QThreadPool *postScanThreadPool() { return &m_postScanThreadPool; }

    void requestScan(const QString &absPath);
    void requestThumbnail(int docId, const QString &filePath, bool highPriority = false);
    void cancelScanForFolder(const QString &folderPath);

    void pauseScan();
    void resumeScan();
    void toggleScanPause();

    void waitForWorkersForTesting();

signals:
    void isScanningChanged(bool isScanning);
    void scanProgressChanged(double progress);
    void scanStatusTextChanged(const QString &statusText);
    void isScanPausedChanged(bool isPaused);
    void thumbnailGenerated(int docId, const QString &thumbnailPath);
    void scannerTaskFinished(const QString &folderPath, bool runCleanup);
    void postScanFinished();
    void libraryChanged();

public slots:
    void onOcrBatchRequested(const QList<QPair<int, QString>> &batch);
    void onThumbnailBatchRequested(const QList<QPair<int, QString>> &batch);
    void onScannerTaskFinished(const QString &folderPath);
    void onScanProgress(const QString &folderPath, int processed, int total);
    void onLowDiskSpaceDetected();
    void onOcrTaskFinished(int docId);
    void onThumbnailTaskFinished(int docId, const QString &thumbnailPath);

private:
    void updateScanProgress();

    DatabaseManager *m_dbMgr;
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

    QSet<int> m_inFlightThumbnails;
    QMutex m_inFlightThumbnailsMutex;
    QElapsedTimer m_lastCoarseRefreshTimer;
};

#endif // SCANTASKMANAGER_H
