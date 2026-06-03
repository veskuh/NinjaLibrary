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

#include "ThumbnailTask.h"
#include "../utils/PdfUtils.h"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QDebug>

ThumbnailTask::ThumbnailTask(int docId, const QString &filePath)
    : m_docId(docId)
    , m_filePath(filePath)
{
}

ThumbnailTask::~ThumbnailTask()
{
}

void ThumbnailTask::run()
{
    QString cacheDir = QDir::homePath() + "/.cache/NinjaLibrary/thumbnails/";
    QDir().mkpath(cacheDir);

    QCryptographicHash hasher(QCryptographicHash::Sha256);
    hasher.addData(m_filePath.toUtf8());
    QString hashStr = hasher.result().toHex();
    QString thumbnailPath = cacheDir + hashStr + ".png";

    // If thumbnail already exists, emit signal and return
    if (QFile::exists(thumbnailPath)) {
        emit finished(m_docId, thumbnailPath);
        return;
    }

    QImage img;
    QFileInfo fileInfo(m_filePath);
    QString ext = fileInfo.suffix().toLower();

    if (ext == "pdf") {
        img = PdfUtils::renderPdfThumbnail(m_filePath, 256);
    } else {
        QImage srcImg;
        if (srcImg.load(m_filePath)) {
            img = srcImg.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }

    if (!img.isNull()) {
        if (img.save(thumbnailPath, "PNG")) {
            emit finished(m_docId, thumbnailPath);
            return;
        }
    }

    qWarning() << "ThumbnailTask: Failed to generate thumbnail for" << m_filePath;
    emit finished(m_docId, "");
}
