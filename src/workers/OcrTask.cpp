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
#include "../utils/OcrUtils.h"

#include <QImage>
#include <QImageReader>
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

#include <QColorSpace>

#include "../utils/MacBookmarks.h"

OcrTask::~OcrTask()
{
}

void OcrTask::run()
{
    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
#ifdef Q_OS_MAC
    QByteArray bookmark;
    if (db.isOpen()) {
        QSqlQuery q(db);
        q.prepare("SELECT macos_bookmark FROM watched_folders WHERE :filePath LIKE absolute_path || '%' ORDER BY LENGTH(absolute_path) DESC LIMIT 1;");
        q.bindValue(":filePath", m_filePath);
        if (q.exec() && q.next()) {
            bookmark = q.value(0).toByteArray();
        }
    }
    MacBookmarks::SandboxAccess sandboxAccess(bookmark);
    if (!sandboxAccess.isValid() && !bookmark.isEmpty()) {
        qWarning() << "OcrTask: Failed to acquire security-scoped sandbox access for file:" << m_filePath;
    }
#endif

    QImage img;
    QFileInfo fileInfo(m_filePath);
    QString ext = fileInfo.suffix().toLower();

    if (ext == "pdf") {
        // Render page 1 at high DPI for OCR accuracy (1024 width)
        img = PdfUtils::renderPdfThumbnail(m_filePath, 1024);
    } else {
        QImageReader reader(m_filePath);
        reader.setAutoTransform(true);
        reader.read(&img);
    }

    if (img.isNull()) {
        qWarning() << "OcrTask: Failed to load/render image for OCR:" << m_filePath;
        emit finished(m_docId);
        return;
    }

    // Workaround: Clear color space metadata to prevent non-thread-safe color space conversions
    // in background threads that can crash inside QColorTrcLut/QColorTransform.
    img.setColorSpace(QColorSpace());

    // Convert to grayscale
    QImage grayImg = img.convertToFormat(QImage::Format_Grayscale8);

    // Perform OCR using platform-specific backend (Vision on macOS, Tesseract on Linux)
    OcrUtils::OcrResult ocrResult = OcrUtils::recognizeText(grayImg);
    QString ocrText = ocrResult.text;
    int confidence = ocrResult.confidence;

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
