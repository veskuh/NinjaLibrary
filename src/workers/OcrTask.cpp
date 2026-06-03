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

#include "OcrTask.h"
#include "../utils/PdfUtils.h"

#include <tesseract/baseapi.h>
#include <QImage>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QFileInfo>

OcrTask::OcrTask(DatabaseManager *dbMgr, int docId, const QString &filePath)
    : m_dbMgr(dbMgr)
    , m_docId(docId)
    , m_filePath(filePath)
{
}

OcrTask::~OcrTask()
{
}

void OcrTask::run()
{
    QImage img;
    QFileInfo fileInfo(m_filePath);
    QString ext = fileInfo.suffix().toLower();

    if (ext == "pdf") {
        // Render page 1 at high DPI for OCR accuracy (1024 width)
        img = PdfUtils::renderPdfThumbnail(m_filePath, 1024);
    } else {
        img.load(m_filePath);
    }

    if (img.isNull()) {
        qWarning() << "OcrTask: Failed to load/render image for OCR:" << m_filePath;
        emit finished(m_docId);
        return;
    }

    // Convert to grayscale
    QImage grayImg = img.convertToFormat(QImage::Format_Grayscale8);

    // Initialize Tesseract BaseAPI
    tesseract::TessBaseAPI *api = new tesseract::TessBaseAPI();
    
    // Check standard Homebrew paths if the default Init fails
    bool initialized = (api->Init(nullptr, "eng") == 0);
    
    if (!initialized) {
        // Try macOS Homebrew tessdata path specifically
#ifdef Q_OS_MAC
        initialized = (api->Init("/opt/homebrew/share/tessdata", "eng") == 0);
#endif
    }

    if (!initialized) {
        qWarning() << "OcrTask: Could not initialize Tesseract OCR engine.";
        delete api;
        emit finished(m_docId);
        return;
    }

    api->SetImage(grayImg.bits(), grayImg.width(), grayImg.height(), 1, grayImg.bytesPerLine());
    
    char *outText = api->GetUTF8Text();
    QString ocrText = QString::fromUtf8(outText).trimmed();
    
    delete [] outText;
    api->End();
    delete api;

    if (ocrText.isEmpty()) {
        emit finished(m_docId);
        return;
    }

    // Write text to SQLite virtual FTS5 search table
    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    if (db.isOpen()) {
        db.transaction();
        
        QString existingText;
        QSqlQuery fetchQuery(db);
        fetchQuery.prepare("SELECT text_snippet FROM document_search WHERE document_id = :docId;");
        fetchQuery.bindValue(":docId", m_docId);
        if (fetchQuery.exec() && fetchQuery.next()) {
            existingText = fetchQuery.value(0).toString().trimmed();
        }

        QString updatedText = existingText;
        if (!updatedText.isEmpty()) {
            updatedText += "\n";
        }
        updatedText += ocrText;

        QSqlQuery updateQuery(db);
        updateQuery.prepare("UPDATE document_search SET text_snippet = :text WHERE document_id = :docId;");
        updateQuery.bindValue(":text", updatedText);
        updateQuery.bindValue(":docId", m_docId);
        if (!updateQuery.exec()) {
            qWarning() << "OcrTask: Failed to save OCR text to database:" << updateQuery.lastError().text();
        }
        
        db.commit();
    }

    emit finished(m_docId);
}
