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
#include <QCoreApplication>
#include <QFile>

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
    
    bool initialized = false;
#ifdef Q_OS_MAC
    // In a macOS bundle, the resources are located in Contents/Resources
    QString bundleTessData = QCoreApplication::applicationDirPath() + "/../Resources/tessdata";
    if (QFile::exists(bundleTessData + "/eng.traineddata")) {
        QByteArray tessDataPathBytes = bundleTessData.toUtf8();
        initialized = (api->Init(tessDataPathBytes.constData(), "eng") == 0);
    }
    
    if (!initialized) {
        // Fallback to standard Homebrew path
        initialized = (api->Init("/opt/homebrew/share/tessdata", "eng") == 0);
    }
#else
    // Check standard paths if the default Init works
    initialized = (api->Init(nullptr, "eng") == 0);
#endif

    if (!initialized) {
        qWarning() << "OcrTask: Could not initialize Tesseract OCR engine.";
        delete api;
        emit finished(m_docId);
        return;
    }

    api->SetImage(grayImg.bits(), grayImg.width(), grayImg.height(), 1, grayImg.bytesPerLine());
    
    char *outText = api->GetUTF8Text();
    QString ocrText = QString::fromUtf8(outText).trimmed();
    int confidence = api->MeanTextConf();
    
    delete [] outText;
    api->End();
    delete api;

    // Calculate alphanumeric ratio to detect noise/garbage characters
    int alphaNumericCount = 0;
    int totalNonSpaceCount = 0;
    for (const QChar &ch : ocrText) {
        if (ch.isSpace()) continue;
        totalNonSpaceCount++;
        if (ch.isLetterOrNumber()) {
            alphaNumericCount++;
        }
    }
    double alphanumericRatio = totalNonSpaceCount > 0 ? static_cast<double>(alphaNumericCount) / totalNonSpaceCount : 0.0;

    if (ocrText.isEmpty() || confidence < 50 || (totalNonSpaceCount > 0 && alphanumericRatio < 0.35)) {
        qDebug() << "OcrTask: Rejecting OCR text for" << m_filePath
                 << "(text length:" << ocrText.length()
                 << ", confidence:" << confidence
                 << ", alpha-ratio:" << alphanumericRatio << ")";
        emit finished(m_docId);
        return;
    }

    // Write text to SQLite virtual FTS5 search table
    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    if (db.isOpen()) {
        QSqlQuery beginQuery(db);
        if (beginQuery.exec("BEGIN IMMEDIATE TRANSACTION")) {
            bool success = true;
            QString existingText;
            QSqlQuery fetchQuery(db);
            fetchQuery.prepare("SELECT text_snippet FROM document_search WHERE document_id = :docId;");
            fetchQuery.bindValue(":docId", m_docId);
            if (fetchQuery.exec()) {
                if (fetchQuery.next()) {
                    existingText = fetchQuery.value(0).toString().trimmed();
                }
            } else {
                qWarning() << "OcrTask: Failed to fetch existing text snippet:" << fetchQuery.lastError().text();
                success = false;
            }

            if (success) {
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
                    success = false;
                }
            }

            if (success) {
                QSqlQuery commitQuery(db);
                if (!commitQuery.exec("COMMIT")) {
                    qWarning() << "OcrTask: Failed to commit transaction:" << commitQuery.lastError().text();
                    QSqlQuery rollbackQuery(db);
                    rollbackQuery.exec("ROLLBACK");
                }
            } else {
                QSqlQuery rollbackQuery(db);
                rollbackQuery.exec("ROLLBACK");
            }
        } else {
            qWarning() << "OcrTask: Failed to begin immediate transaction:" << beginQuery.lastError().text();
        }
    }

    emit finished(m_docId);
}
