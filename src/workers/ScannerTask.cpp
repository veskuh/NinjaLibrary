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
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER or CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ScannerTask.h"
#include <QList>
#include <QPair>
#include "../utils/HashUtils.h"
#include "../utils/PdfUtils.h"
#include "../utils/DocUtils.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QDateTime>
#include <QSqlQuery>
#include <QSqlError>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QSet>
#include <QDebug>
#include <QStandardPaths>

#include "../utils/MacBookmarks.h"
#include <QStorageInfo>

std::atomic<bool> ScannerTask::s_scanPaused{false};
QMutex ScannerTask::s_pauseMutex;
QWaitCondition ScannerTask::s_pauseCondition;
std::atomic<bool> ScannerTask::s_lowDiskSpace{false};

ScannerTask::ScannerTask(DatabaseManager *dbMgr, const QString &folderPath)
    : m_dbMgr(dbMgr)
    , m_folderPath(folderPath)
{
}

ScannerTask::~ScannerTask()
{
}

void ScannerTask::run()
{
    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    if (!db.isOpen()) {
        emit finished(m_folderPath);
        return;
    }

    // 1. Get folder ID and macOS bookmark from database
    int folderId = -1;
    QByteArray bookmark;
    {
        QSqlQuery query(db);
        query.prepare("SELECT id, macos_bookmark FROM watched_folders WHERE absolute_path = :path;");
        query.bindValue(":path", m_folderPath);
        if (query.exec() && query.next()) {
            folderId = query.value(0).toInt();
            bookmark = query.value(1).toByteArray();
        }
    }

    if (folderId == -1) {
        emit finished(m_folderPath);
        return;
    }

#ifdef Q_OS_MAC
    // Access security-scoped resources on the current background thread
    MacBookmarks::SandboxAccess sandboxAccess(bookmark);
    if (!sandboxAccess.isValid() && !bookmark.isEmpty()) {
        qWarning() << "ScannerTask: Failed to acquire security-scoped sandbox access for folder:" << m_folderPath;
    }
#endif

    QList<QPair<int, QString>> pendingOcr;
    QList<QPair<int, QString>> pendingThumbnails;

    QSet<QString> filesOnDisk;
    QList<QString> filesToProcess;
    QDirIterator it(m_folderPath, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString filePath = it.next();
        if (isSupportedDocument(filePath)) {
            filesToProcess.append(filePath);
        }
    }
    int totalFiles = filesToProcess.size();
    int processedCount = 0;
    emit progress(m_folderPath, 0, totalFiles);

    for (const QString &filePath : filesToProcess) {
        // Check disk space (safeguard threshold: 500MB)
        QStorageInfo storage(m_folderPath);
        if (storage.isValid() && storage.bytesAvailable() < 500LL * 1024LL * 1024LL) {
            s_lowDiskSpace = true;
            s_scanPaused = true;
            emit lowDiskSpaceDetected();
        } else {
            s_lowDiskSpace = false;
        }

        if (s_scanPaused) {
            QMutexLocker locker(&s_pauseMutex);
            while (s_scanPaused) {
                s_pauseCondition.wait(&s_pauseMutex);
            }
        }

        // Check if the folder is still in watched_folders list
        if (processedCount % 5 == 0) {
            QSqlQuery checkWatched(db);
            checkWatched.prepare("SELECT id FROM watched_folders WHERE id = :id;");
            checkWatched.bindValue(":id", folderId);
            if (!checkWatched.exec() || !checkWatched.next()) {
                qDebug() << "ScannerTask: Folder was unwatched during scan, aborting immediately:" << m_folderPath;
                emit finished(m_folderPath);
                return;
            }
        }

        filesOnDisk.insert(filePath);
        QFileInfo fileInfo(filePath);
        QString ext = fileInfo.suffix().toLower();
        qint64 currentSize = fileInfo.size();
        QDateTime currentModified = fileInfo.lastModified();

        // Check if document exists in DB
        QSqlQuery checkQuery(db);
        checkQuery.prepare("SELECT id, file_size, date_modified, is_offline FROM documents WHERE absolute_path = :path;");
        checkQuery.bindValue(":path", filePath);

        if (!checkQuery.exec()) {
            qWarning() << "ScannerTask database check failed:" << checkQuery.lastError().text();
            continue;
        }

        if (checkQuery.next()) {
            int docId = checkQuery.value(0).toInt();
            qint64 dbSize = checkQuery.value(1).toLongLong();
            QDateTime dbModified = checkQuery.value(2).toDateTime();
            bool isOffline = checkQuery.value(3).toBool();

            // Compare size and mod time (ignore sub-second difference due to sqlite precision)
            if (dbSize != currentSize || qAbs(dbModified.secsTo(currentModified)) > 1) {
                // File modified: update details and re-extract text
                QString fileHash = HashUtils::computeSha256(filePath);
                int pageCount = 0;
                QString extractedText;
                if (ext == "pdf") {
                    pageCount = PdfUtils::getPdfPageCount(filePath);
                    extractedText = PdfUtils::extractPdfText(filePath);
                } else if (DocUtils::isSupportedTextDocument(filePath)) {
                    pageCount = 1;
                    extractedText = DocUtils::extractText(filePath);
                } else {
                    pageCount = 1;
                }

                // Read sidecar metadata if available
                QStringList tags;
                int rating = 0;
                QString notes;
                QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
                if (dataDir.isEmpty()) {
                    dataDir = QDir::homePath() + "/.local/share/NinjaLibrary";
                }
                QString sidecarDir = dataDir + "/sidecars/";
                QCryptographicHash hasher(QCryptographicHash::Sha256);
                hasher.addData(filePath.toUtf8());
                QString hashStr = hasher.result().toHex();
                QString sidecarPath = sidecarDir + hashStr + ".ninja";

                if (QFile::exists(sidecarPath)) {
                    QFile file(sidecarPath);
                    if (file.open(QIODevice::ReadOnly)) {
                        QJsonDocument jsonDoc = QJsonDocument::fromJson(file.readAll());
                        if (jsonDoc.isObject()) {
                            QJsonObject obj = jsonDoc.object();
                            rating = obj["star_rating"].toInt(0);
                            notes = obj["notes"].toString();
                            QJsonArray tagsArray = obj["tags"].toArray();
                            for (int t = 0; t < tagsArray.size(); ++t) {
                                tags.append(tagsArray.at(t).toString());
                            }
                        }
                    }
                }

                QSqlQuery beginWrite(db);
                if (beginWrite.exec("BEGIN IMMEDIATE TRANSACTION")) {
                    bool ok = true;
                    QSqlQuery updateQuery(db);
                    updateQuery.prepare("UPDATE documents SET file_size = :size, date_modified = :modified, file_hash = :hash, page_count = :pageCount, is_offline = 0 WHERE id = :id;");
                    updateQuery.bindValue(":size", currentSize);
                    updateQuery.bindValue(":modified", currentModified);
                    updateQuery.bindValue(":hash", fileHash);
                    updateQuery.bindValue(":pageCount", pageCount);
                    updateQuery.bindValue(":id", docId);
                    ok &= updateQuery.exec();

                    // Reset search entry
                    QSqlQuery deleteSearch(db);
                    deleteSearch.prepare("DELETE FROM document_search WHERE document_id = :docId;");
                    deleteSearch.bindValue(":docId", docId);
                    ok &= deleteSearch.exec();

                    QSqlQuery insertSearch(db);
                    insertSearch.prepare("INSERT INTO document_search (document_id, file_name, text_snippet, notes) VALUES (:docId, :fileName, :text, :notes);");
                    insertSearch.bindValue(":docId", docId);
                    insertSearch.bindValue(":fileName", fileInfo.fileName());
                    insertSearch.bindValue(":text", extractedText);
                    insertSearch.bindValue(":notes", notes);
                    ok &= insertSearch.exec();

                    // Apply sidecar attributes to document record
                    if (rating > 0) {
                        QSqlQuery updateRating(db);
                        updateRating.prepare("UPDATE documents SET star_rating = :rating WHERE id = :docId;");
                        updateRating.bindValue(":rating", rating);
                        updateRating.bindValue(":docId", docId);
                        ok &= updateRating.exec();
                    }

                    for (const QString &tagName : tags) {
                        QSqlQuery insertTag(db);
                        insertTag.prepare("INSERT OR IGNORE INTO tags (name) VALUES (:name);");
                        insertTag.bindValue(":name", tagName.trimmed());
                        ok &= insertTag.exec();

                        QSqlQuery getTagId(db);
                        getTagId.prepare("SELECT id FROM tags WHERE name = :name;");
                        getTagId.bindValue(":name", tagName.trimmed());
                        if (getTagId.exec() && getTagId.next()) {
                            int tagId = getTagId.value(0).toInt();
                            QSqlQuery linkTag(db);
                            linkTag.prepare("INSERT OR IGNORE INTO document_tags (document_id, tag_id) VALUES (:docId, :tagId);");
                            linkTag.bindValue(":docId", docId);
                            linkTag.bindValue(":tagId", tagId);
                            ok &= linkTag.exec();
                        } else {
                            ok = false;
                        }
                    }

                    if (ok) {
                        QSqlQuery commitWrite(db);
                        if (!commitWrite.exec("COMMIT")) {
                            qWarning() << "ScannerTask: Commit failed on modified, rolling back:" << commitWrite.lastError().text();
                            QSqlQuery rollbackWrite(db);
                            rollbackWrite.exec("ROLLBACK");
                        }
                    } else {
                        qWarning() << "ScannerTask: Modified document updates failed, rolling back:" << updateQuery.lastError().text();
                        QSqlQuery rollbackWrite(db);
                        rollbackWrite.exec("ROLLBACK");
                    }
                } else {
                    qWarning() << "ScannerTask: Failed to begin immediate transaction for modified document:" << beginWrite.lastError().text();
                }

                bool isImage = (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "tiff" || ext == "bmp");
                if (isImage || (ext == "pdf" && countWords(extractedText) < 10)) {
                    pendingOcr.append({docId, filePath});
                }
                if (isImage || ext == "pdf") {
                    pendingThumbnails.append({docId, filePath});
                }
            } else if (isOffline) {
                // File came back online
                QSqlQuery beginWrite(db);
                if (beginWrite.exec("BEGIN IMMEDIATE TRANSACTION")) {
                    QSqlQuery updateOffline(db);
                    updateOffline.prepare("UPDATE documents SET is_offline = 0 WHERE id = :id;");
                    updateOffline.bindValue(":id", docId);
                    if (updateOffline.exec()) {
                        QSqlQuery commitWrite(db);
                        if (!commitWrite.exec("COMMIT")) {
                            qWarning() << "ScannerTask: Commit failed on online, rolling back:" << commitWrite.lastError().text();
                            QSqlQuery rollbackWrite(db);
                            rollbackWrite.exec("ROLLBACK");
                        }
                    } else {
                        QSqlQuery rollbackWrite(db);
                        rollbackWrite.exec("ROLLBACK");
                    }
                }
                bool isImage = (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "tiff" || ext == "bmp");
                if (isImage || ext == "pdf") {
                    pendingThumbnails.append({docId, filePath});
                }
            }
        } else {
            // New document: Ingest
            QString fileHash = HashUtils::computeSha256(filePath);
            int pageCount = 0;
            QString extractedText;
            if (ext == "pdf") {
                pageCount = PdfUtils::getPdfPageCount(filePath);
                extractedText = PdfUtils::extractPdfText(filePath);
            } else if (DocUtils::isSupportedTextDocument(filePath)) {
                pageCount = 1;
                extractedText = DocUtils::extractText(filePath);
            } else {
                pageCount = 1;
            }

            QDateTime created = fileInfo.birthTime();
            if (!created.isValid()) {
                created = fileInfo.metadataChangeTime();
            }
            if (!created.isValid()) {
                created = fileInfo.lastModified();
            }

            // Read sidecar metadata if available
            QStringList tags;
            int rating = 0;
            QString notes;
            QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
            if (dataDir.isEmpty()) {
                dataDir = QDir::homePath() + "/.local/share/NinjaLibrary";
            }
            QString sidecarDir = dataDir + "/sidecars/";
            QCryptographicHash hasher(QCryptographicHash::Sha256);
            hasher.addData(filePath.toUtf8());
            QString hashStr = hasher.result().toHex();
            QString sidecarPath = sidecarDir + hashStr + ".ninja";

            if (QFile::exists(sidecarPath)) {
                QFile file(sidecarPath);
                if (file.open(QIODevice::ReadOnly)) {
                    QJsonDocument jsonDoc = QJsonDocument::fromJson(file.readAll());
                    if (jsonDoc.isObject()) {
                        QJsonObject obj = jsonDoc.object();
                        rating = obj["star_rating"].toInt(0);
                        notes = obj["notes"].toString();
                        QJsonArray tagsArray = obj["tags"].toArray();
                        for (int t = 0; t < tagsArray.size(); ++t) {
                            tags.append(tagsArray.at(t).toString());
                        }
                    }
                }
            }

            QSqlQuery beginWrite(db);
            if (beginWrite.exec("BEGIN IMMEDIATE TRANSACTION")) {
                bool ok = true;

                QSqlQuery insertDoc(db);
                insertDoc.prepare("INSERT INTO documents (folder_id, file_name, absolute_path, file_size, file_hash, date_created, date_modified, page_count, is_offline) "
                                  "VALUES (:folderId, :fileName, :absPath, :size, :hash, :created, :modified, :pageCount, 0);");
                insertDoc.bindValue(":folderId", folderId);
                insertDoc.bindValue(":fileName", fileInfo.fileName());
                insertDoc.bindValue(":absPath", filePath);
                insertDoc.bindValue(":size", currentSize);
                insertDoc.bindValue(":hash", fileHash);
                insertDoc.bindValue(":created", created);
                insertDoc.bindValue(":modified", currentModified);
                insertDoc.bindValue(":pageCount", pageCount);

                ok &= insertDoc.exec();
                int docId = -1;
                if (ok) {
                    docId = insertDoc.lastInsertId().toInt();
                }

                if (ok && docId != -1) {
                    QSqlQuery insertSearch(db);
                    insertSearch.prepare("INSERT INTO document_search (document_id, file_name, text_snippet, notes) VALUES (:docId, :fileName, :text, :notes);");
                    insertSearch.bindValue(":docId", docId);
                    insertSearch.bindValue(":fileName", fileInfo.fileName());
                    insertSearch.bindValue(":text", extractedText);
                    insertSearch.bindValue(":notes", notes);
                    ok &= insertSearch.exec();

                    if (rating > 0) {
                        QSqlQuery updateRating(db);
                        updateRating.prepare("UPDATE documents SET star_rating = :rating WHERE id = :docId;");
                        updateRating.bindValue(":rating", rating);
                        updateRating.bindValue(":docId", docId);
                        ok &= updateRating.exec();
                    }

                    for (const QString &tagName : tags) {
                        QSqlQuery insertTag(db);
                        insertTag.prepare("INSERT OR IGNORE INTO tags (name) VALUES (:name);");
                        insertTag.bindValue(":name", tagName.trimmed());
                        ok &= insertTag.exec();

                        QSqlQuery getTagId(db);
                        getTagId.prepare("SELECT id FROM tags WHERE name = :name;");
                        getTagId.bindValue(":name", tagName.trimmed());
                        if (getTagId.exec() && getTagId.next()) {
                            int tagId = getTagId.value(0).toInt();
                            QSqlQuery linkTag(db);
                            linkTag.prepare("INSERT OR IGNORE INTO document_tags (document_id, tag_id) VALUES (:docId, :tagId);");
                            linkTag.bindValue(":docId", docId);
                            linkTag.bindValue(":tagId", tagId);
                            ok &= linkTag.exec();
                        } else {
                            ok = false;
                        }
                    }
                } else {
                    ok = false;
                }

                if (ok && docId != -1) {
                    QSqlQuery commitWrite(db);
                    if (!commitWrite.exec("COMMIT")) {
                        qWarning() << "ScannerTask: Commit failed on new, rolling back:" << commitWrite.lastError().text();
                        QSqlQuery rollbackWrite(db);
                        rollbackWrite.exec("ROLLBACK");
                    } else {
                        bool isImage = (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "tiff" || ext == "bmp");
                        if (isImage || (ext == "pdf" && countWords(extractedText) < 10)) {
                            pendingOcr.append({docId, filePath});
                        }
                        if (isImage || ext == "pdf") {
                            pendingThumbnails.append({docId, filePath});
                        }
                    }
                } else {
                    qWarning() << "Failed to insert document" << filePath << ":" << insertDoc.lastError().text();
                    QSqlQuery rollbackWrite(db);
                    rollbackWrite.exec("ROLLBACK");
                }
            } else {
                qWarning() << "ScannerTask: Failed to begin immediate transaction for new document:" << beginWrite.lastError().text();
            }
        }
        processedCount++;
        if (processedCount % 5 == 0 || processedCount == totalFiles) {
                            emit progress(m_folderPath, processedCount, totalFiles);
        }
    }

    // 2. Detect deleted files (present in DB but missing on disk)
    bool folderExists = QDir(m_folderPath).exists();

    QSqlQuery fetchDocs(db);
    fetchDocs.prepare("SELECT id, absolute_path, is_offline FROM documents WHERE folder_id = :folderId;");
    fetchDocs.bindValue(":folderId", folderId);
    if (fetchDocs.exec()) {
        QList<int> docsToMarkOffline;
        QList<QPair<int, QString>> docsToDelete;

        while (fetchDocs.next()) {
            int docId = fetchDocs.value(0).toInt();
            QString docPath = fetchDocs.value(1).toString();
            bool isOffline = fetchDocs.value(2).toBool();
            if (!filesOnDisk.contains(docPath)) {
                if (folderExists) {
                    docsToDelete.append(qMakePair(docId, docPath));
                } else if (!isOffline) {
                    docsToMarkOffline.append(docId);
                }
            }
        }

        // Process physical deletions (parent folder/volume is available)
        if (!docsToDelete.isEmpty()) {
            QSqlQuery beginWrite(db);
            if (beginWrite.exec("BEGIN IMMEDIATE TRANSACTION")) {
                bool ok = true;
                for (const auto &pair : docsToDelete) {
                    int docId = pair.first;
                    QString docPath = pair.second;

                    // A. Delete search index entries
                    QSqlQuery deleteSearch(db);
                    deleteSearch.prepare("DELETE FROM document_search WHERE document_id = :docId;");
                    deleteSearch.bindValue(":docId", docId);
                    ok &= deleteSearch.exec();

                    // B. Delete document tags
                    QSqlQuery deleteTags(db);
                    deleteTags.prepare("DELETE FROM document_tags WHERE document_id = :docId;");
                    deleteTags.bindValue(":docId", docId);
                    ok &= deleteTags.exec();

                    // C. Delete document
                    QSqlQuery deleteDoc(db);
                    deleteDoc.prepare("DELETE FROM documents WHERE id = :docId;");
                    deleteDoc.bindValue(":docId", docId);
                    ok &= deleteDoc.exec();

                    if (ok) {
                        // D. Clean up sidecar file if exists
                        QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
                        QString sidecarDir = dataDir + "/sidecars/";
                        QCryptographicHash hash(QCryptographicHash::Sha256);
                        hash.addData(docPath.toUtf8());
                        QString hashStr = hash.result().toHex();
                        QString sidecarPath = sidecarDir + hashStr + ".ninja";
                        if (QFile::exists(sidecarPath)) {
                            QFile::remove(sidecarPath);
                        }

                        // E. Clean up cached thumbnail
                        QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/thumbnails/";
                        QString thumbPath = cacheDir + hashStr + ".png";
                        if (QFile::exists(thumbPath)) {
                            QFile::remove(thumbPath);
                        }
                    }
                }
                if (ok) {
                    QSqlQuery commitWrite(db);
                    if (!commitWrite.exec("COMMIT")) {
                        qWarning() << "ScannerTask: Commit failed on deleting docs, rolling back:" << commitWrite.lastError().text();
                        QSqlQuery rollbackWrite(db);
                        rollbackWrite.exec("ROLLBACK");
                    }
                } else {
                    QSqlQuery rollbackWrite(db);
                    rollbackWrite.exec("ROLLBACK");
                }
            }
        }

        // Process offline markings (parent folder/volume is missing)
        if (!docsToMarkOffline.isEmpty()) {
            QSqlQuery beginWrite(db);
            if (beginWrite.exec("BEGIN IMMEDIATE TRANSACTION")) {
                bool ok = true;
                for (int docId : docsToMarkOffline) {
                    QSqlQuery markOffline(db);
                    markOffline.prepare("UPDATE documents SET is_offline = 1 WHERE id = :id;");
                    markOffline.bindValue(":id", docId);
                    ok &= markOffline.exec();
                }
                if (ok) {
                    QSqlQuery commitWrite(db);
                    if (!commitWrite.exec("COMMIT")) {
                        qWarning() << "ScannerTask: Commit failed on marking offline, rolling back:" << commitWrite.lastError().text();
                        QSqlQuery rollbackWrite(db);
                        rollbackWrite.exec("ROLLBACK");
                    }
                } else {
                    QSqlQuery rollbackWrite(db);
                    rollbackWrite.exec("ROLLBACK");
                }
            }
        }
    }

    for (const auto &pair : pendingOcr) {
        emit ocrRequested(pair.first, pair.second);
    }
    for (const auto &pair : pendingThumbnails) {
        emit thumbnailRequested(pair.first, pair.second);
    }

    emit finished(m_folderPath);
}

bool ScannerTask::isSupportedDocument(const QString &filePath) const
{
    // Filter out files inside macOS package directory structures and other ignore-listed directories
    QFileInfo fileInfo(filePath);
    QString dirPath = fileInfo.absolutePath();
    QStringList segments = dirPath.split(QRegularExpression("[/\\\\]"), Qt::SkipEmptyParts);
    for (const QString &segment : segments) {
        if (segment.endsWith(".app", Qt::CaseInsensitive) ||
            segment.endsWith(".photoslibrary", Qt::CaseInsensitive) ||
            segment.endsWith(".photolibrary", Qt::CaseInsensitive) ||
            segment.endsWith(".migratedphotolibrary", Qt::CaseInsensitive) ||
            segment.endsWith(".framework", Qt::CaseInsensitive) ||
            segment.endsWith(".bundle", Qt::CaseInsensitive) ||
            segment.endsWith(".xcodeproj", Qt::CaseInsensitive) ||
            segment.endsWith(".pages", Qt::CaseInsensitive) ||
            segment.endsWith(".numbers", Qt::CaseInsensitive) ||
            segment.endsWith(".key", Qt::CaseInsensitive) ||
            segment.endsWith(".wdgt", Qt::CaseInsensitive) ||
            segment.endsWith(".plugin", Qt::CaseInsensitive) ||
            segment.endsWith(".appex", Qt::CaseInsensitive) ||
            segment.endsWith(".scnassets", Qt::CaseInsensitive) ||
            segment.endsWith(".xcassets", Qt::CaseInsensitive) ||
            segment == ".git" ||
            segment == ".svn" ||
            segment.toLower() == ".trash") {
            return false;
        }
    }

    QString ext = fileInfo.suffix().toLower();
    return ext == "pdf" || ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "tiff" || ext == "bmp" || DocUtils::isSupportedTextDocument(filePath);
}

int ScannerTask::countWords(const QString &text) const
{
    return text.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).size();
}
