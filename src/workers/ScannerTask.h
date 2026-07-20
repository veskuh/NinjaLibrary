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

#ifndef SCANNERTASK_H
#define SCANNERTASK_H

#include <QMutex>
#include <QObject>
#include <QRunnable>
#include <QString>
#include <QWaitCondition>
#include <atomic>

#include "../database/DatabaseManager.h"

class ScannerTask : public QObject, public QRunnable
{
    Q_OBJECT
public:
    ScannerTask(DatabaseManager *dbMgr, const QString &folderPath);
    ~ScannerTask();

    void run() override;
    void cancel() { m_cancelled = true; }

    static std::atomic<bool> s_scanPaused;
    static QMutex s_pauseMutex;
    static QWaitCondition s_pauseCondition;
    static std::atomic<bool> s_lowDiskSpace;

signals:
    void finished(const QString &folderPath);
    void progress(const QString &folderPath, int processed, int total);
    void ocrRequested(int docId, const QString &filePath);
    void thumbnailRequested(int docId, const QString &filePath);
    void lowDiskSpaceDetected();

private:
    DatabaseManager *m_dbMgr;
    QString m_folderPath;

    bool isSupportedDocument(const QString &filePath) const;
    int countWords(const QString &text) const;

    std::atomic<bool> m_cancelled{false};
};

#endif  // SCANNERTASK_H
