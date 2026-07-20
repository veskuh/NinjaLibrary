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

#include "DocumentModel.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStandardPaths>
#include <QtConcurrent>

static QString getParentDirectory(const QString &absolutePath)
{
    if (absolutePath.isEmpty()) {
        return QString();
    }
    int lastSlash = absolutePath.lastIndexOf('/');
    int lastBack = absolutePath.lastIndexOf('\\');
    int last = (lastSlash > lastBack) ? lastSlash : lastBack;
    if (last > 0) {
        return absolutePath.left(last);
    } else if (last == 0) {
        return absolutePath.left(1);
    }
    return "";
}

static QString getFileName(const QString &path)
{
    int lastSlash = path.lastIndexOf('/');
    int lastBack = path.lastIndexOf('\\');
    int last = (lastSlash > lastBack) ? lastSlash : lastBack;
    if (last >= 0) {
        return path.mid(last + 1);
    }
    return path;
}

DocumentModel::DocumentModel(DatabaseManager *dbMgr, QObject *parent)
    : QAbstractListModel(parent), m_dbMgr(dbMgr)
{
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setSingleShot(true);
    connect(m_refreshTimer, &QTimer::timeout, this, &DocumentModel::forceRefresh);

    m_refreshWatcher = new QFutureWatcher<QList<DocumentInfo>>(this);
    connect(m_refreshWatcher, &QFutureWatcher<QList<DocumentInfo>>::finished, this,
            &DocumentModel::onRefreshFinished);

    forceRefresh();
}

DocumentModel::~DocumentModel() {}

int DocumentModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_documents.size();
}

QVariant DocumentModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_documents.size()) return QVariant();

    const DocumentInfo &doc = m_documents.at(index.row());

    switch (role) {
        case IdRole:
            return doc.id;
        case FolderIdRole:
            return doc.folderId;
        case FileNameRole:
            return doc.fileName;
        case AbsolutePathRole:
            return doc.absolutePath;
        case FileSizeRole:
            return doc.fileSize;
        case FileHashRole:
            return doc.fileHash;
        case DateCreatedRole:
            return doc.dateCreated;
        case DateModifiedRole:
            return doc.dateModified;
        case DateAddedRole:
            return doc.dateAdded;
        case PageCountRole:
            return doc.pageCount;
        case StarRatingRole:
            return doc.starRating;
        case IsOfflineRole:
            return doc.isOffline;
        case TagsRole:
            return doc.tags;
        case TextSnippetRole:
            return doc.textSnippet;
        case NotesRole:
            return doc.notes;
        case ThumbnailPathRole:
            return doc.thumbnailPath;
        case FileSizeStrRole: {
            qint64 kb = doc.fileSize / 1024;
            if (kb > 1024) {
                return QString("%1 MB").arg(double(kb) / 1024.0, 0, 'f', 1);
            }
            return QString("%1 KB").arg(kb);
        }
        case StarRatingStrRole: {
            QString stars;
            for (int i = 0; i < 5; ++i) {
                stars += (i < doc.starRating) ? "★" : "☆";
            }
            return stars;
        }
        case OfflineColorRole: {
            return doc.isOffline ? "red" : "green";
        }
        case DateModifiedStrRole: {
            return doc.dateModified.toString("yyyy-MM-dd hh:mm");
        }
        case TagsStrRole: {
            return doc.tags.join(", ");
        }
        case LastOpenedRole:
            return doc.lastOpened;
        case IsFolderRole:
            return doc.isFolder;
        case ItemCountRole:
            return doc.itemCount;
        case ItemCountStrRole: {
            if (!doc.isFolder) return QString();
            return QString("%1 item%2").arg(doc.itemCount).arg(doc.itemCount == 1 ? "" : "s");
        }
        default:
            return QVariant();
    }
}

QHash<int, QByteArray> DocumentModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "docId";
    roles[FolderIdRole] = "folderId";
    roles[FileNameRole] = "fileName";
    roles[AbsolutePathRole] = "absolutePath";
    roles[FileSizeRole] = "fileSize";
    roles[FileHashRole] = "fileHash";
    roles[DateCreatedRole] = "dateCreated";
    roles[DateModifiedRole] = "dateModified";
    roles[DateAddedRole] = "dateAdded";
    roles[PageCountRole] = "pageCount";
    roles[StarRatingRole] = "starRating";
    roles[IsOfflineRole] = "isOffline";
    roles[TagsRole] = "tags";
    roles[TextSnippetRole] = "textSnippet";
    roles[NotesRole] = "notes";
    roles[ThumbnailPathRole] = "thumbnailPath";
    roles[FileSizeStrRole] = "fileSizeStr";
    roles[StarRatingStrRole] = "starRatingStr";
    roles[OfflineColorRole] = "offlineColor";
    roles[DateModifiedStrRole] = "dateModifiedStr";
    roles[TagsStrRole] = "tagsStr";
    roles[LastOpenedRole] = "lastOpened";
    roles[IsFolderRole] = "isFolder";
    roles[ItemCountRole] = "itemCount";
    roles[ItemCountStrRole] = "itemCountStr";
    return roles;
}

void DocumentModel::refresh()
{
    m_refreshTimer->start(200);  // Debounce by 200ms
}

static QList<DocumentInfo> computeRefreshSnapshotOffThread(DatabaseManager *dbMgr)
{
    QList<DocumentInfo> newDocs;
    QSqlDatabase db = dbMgr->getDatabaseConnection();
    if (!db.isOpen()) return newDocs;

    QList<QSqlRecord> records;
    {
        QSqlQuery query(
            "SELECT d.id, d.folder_id, d.file_name, d.absolute_path, d.file_size, d.file_hash, "
            "       d.date_created, d.date_modified, d.date_added, d.page_count, d.star_rating, "
            "d.is_offline, "
            "       d.last_opened, "
            "       (SELECT group_concat(t.name, ',') FROM tags t JOIN document_tags dt ON t.id = "
            "dt.tag_id WHERE dt.document_id = d.id) as tags_list, "
            "       (SELECT text_snippet FROM document_search WHERE document_id = d.id) as text, "
            "       (SELECT notes FROM document_search WHERE document_id = d.id) as notes "
            "FROM documents d;",
            db);
        while (query.next()) {
            records.append(query.record());
        }
    }

    QMap<int, QString> watchedRoots;
    {
        QSqlQuery q("SELECT id, absolute_path FROM watched_folders;", db);
        while (q.next()) {
            watchedRoots[q.value(0).toInt()] = q.value(1).toString();
        }
    }

    QMap<QString, int> folderFileCounts;
    QSet<QString> uniqueFolders;
    QMap<QString, int> folderIdMap;

    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cacheDir.isEmpty()) {
        cacheDir = QDir::homePath() + "/.cache/NinjaLibrary";
    }
    cacheDir += "/thumbnails/";

    for (const QSqlRecord &record : records) {
        DocumentInfo doc;
        doc.id = record.value(0).toInt();
        doc.folderId = record.value(1).toInt();
        doc.fileName = record.value(2).toString();
        doc.absolutePath = record.value(3).toString();
        doc.fileSize = record.value(4).toLongLong();
        doc.fileHash = record.value(5).toString();
        doc.dateCreated = record.value(6).toDateTime();
        doc.dateModified = record.value(7).toDateTime();
        doc.dateAdded = record.value(8).toDateTime();
        doc.pageCount = record.value(9).toInt();
        doc.starRating = record.value(10).toInt();
        doc.isOffline = record.value(11).toBool();
        doc.lastOpened = record.value(12).toLongLong();
        doc.isFolder = false;
        doc.itemCount = 0;

        QString tagsStr = record.value(13).toString();
        if (!tagsStr.isEmpty()) {
            doc.tags = tagsStr.split(",", Qt::SkipEmptyParts);
        } else {
            doc.tags = QStringList();
        }

        doc.textSnippet = record.value(14).toString();
        doc.notes = record.value(15).toString();

        // Calculate expected thumbnail location
        QCryptographicHash hasher(QCryptographicHash::Sha256);
        hasher.addData(doc.absolutePath.toUtf8());
        QString hashStr = hasher.result().toHex();
        QString thumbFile = cacheDir + hashStr + ".png";

        if (QFile::exists(thumbFile)) {
            doc.thumbnailPath = "file://" + thumbFile;
        } else {
            doc.thumbnailPath = "";
        }

        newDocs.append(doc);

        // Track folders recursively
        QString root = watchedRoots.value(doc.folderId);
        if (!root.isEmpty()) {
            QString parentDir = getParentDirectory(doc.absolutePath);
            QString dir = parentDir;
            while (dir.startsWith(root) && dir.length() > root.length()) {
                uniqueFolders.insert(dir);
                folderIdMap[dir] = doc.folderId;
                folderFileCounts[dir]++;
                dir = getParentDirectory(dir);
            }
        }
    }

    // Append virtual folders
    int virtualFolderId = -2;
    for (const QString &folderPath : uniqueFolders) {
        DocumentInfo folderDoc;
        folderDoc.id = virtualFolderId--;
        folderDoc.folderId = folderIdMap.value(folderPath);
        folderDoc.fileName = getFileName(folderPath);
        folderDoc.absolutePath = folderPath;
        folderDoc.isFolder = true;
        folderDoc.itemCount = folderFileCounts.value(folderPath, 0);
        folderDoc.fileSize = 0;
        folderDoc.starRating = 0;
        folderDoc.isOffline = false;
        folderDoc.dateModified = QFileInfo(folderPath).lastModified();
        newDocs.append(folderDoc);
    }

    return newDocs;
}

void DocumentModel::forceRefresh()
{
    m_refreshTimer->stop();

    if (m_isRefreshing) {
        m_refreshTimer->start(200);
        return;
    }

    m_isRefreshing = true;
    QFuture<QList<DocumentInfo>> future =
        QtConcurrent::run(&computeRefreshSnapshotOffThread, m_dbMgr);
    m_refreshWatcher->setFuture(future);
}

void DocumentModel::onRefreshFinished()
{
    m_isRefreshing = false;
    QList<DocumentInfo> newDocs = m_refreshWatcher->result();

    // Calculate count changes
    int tempPdf = 0;
    int tempImage = 0;
    int tempText = 0;
    int tempLocal = 0;
    int tempUnavailable = 0;

    for (const auto &doc : newDocs) {
        if (doc.isFolder) {
            continue;
        }

        if (doc.isOffline) {
            tempUnavailable++;
        } else {
            tempLocal++;
        }

        QString ext = doc.fileName.split('.').last().toLower();
        if (ext == "pdf") {
            tempPdf++;
        } else if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "gif" || ext == "bmp" ||
                   ext == "tiff") {
            tempImage++;
        } else {
            tempText++;
        }
    }

    bool changed = (m_pdfCount != tempPdf || m_imageCount != tempImage || m_textCount != tempText ||
                    m_localCount != tempLocal || m_unavailableCount != tempUnavailable);

    m_pdfCount = tempPdf;
    m_imageCount = tempImage;
    m_textCount = tempText;
    m_localCount = tempLocal;
    m_unavailableCount = tempUnavailable;

    // Check if the current model is completely empty
    if (m_documents.isEmpty()) {
        beginResetModel();
        m_documents = newDocs;
        endResetModel();
    } else {
        bool isReconciling = false;
        auto ensureReconciling = [this, &isReconciling]() {
            if (!isReconciling) {
                isReconciling = true;
                emit aboutToReconcile();
            }
        };

        // Reconcile changes incrementally
        QMap<QString, DocumentInfo> currentMap;
        for (const auto &doc : m_documents) {
            currentMap[doc.absolutePath] = doc;
        }

        QMap<QString, DocumentInfo> newMap;
        for (const auto &doc : newDocs) {
            newMap[doc.absolutePath] = doc;
        }

        // 1. Remove items no longer present (descending order to preserve indices)
        for (int i = m_documents.size() - 1; i >= 0; --i) {
            const QString &path = m_documents.at(i).absolutePath;
            if (!newMap.contains(path)) {
                ensureReconciling();
                beginRemoveRows(QModelIndex(), i, i);
                m_documents.removeAt(i);
                endRemoveRows();
            }
        }

        // 2. Update existing items
        for (int i = 0; i < m_documents.size(); ++i) {
            const QString &path = m_documents.at(i).absolutePath;
            if (newMap.contains(path)) {
                const DocumentInfo &newDoc = newMap.value(path);
                const DocumentInfo &oldDoc = m_documents.at(i);

                bool itemChanged =
                    (oldDoc.id != newDoc.id || oldDoc.folderId != newDoc.folderId ||
                     oldDoc.fileName != newDoc.fileName || oldDoc.fileSize != newDoc.fileSize ||
                     oldDoc.fileHash != newDoc.fileHash ||
                     oldDoc.dateCreated != newDoc.dateCreated ||
                     oldDoc.dateModified != newDoc.dateModified ||
                     oldDoc.dateAdded != newDoc.dateAdded || oldDoc.pageCount != newDoc.pageCount ||
                     oldDoc.starRating != newDoc.starRating ||
                     oldDoc.isOffline != newDoc.isOffline || oldDoc.tags != newDoc.tags ||
                     oldDoc.textSnippet != newDoc.textSnippet || oldDoc.notes != newDoc.notes ||
                     oldDoc.thumbnailPath != newDoc.thumbnailPath ||
                     oldDoc.lastOpened != newDoc.lastOpened || oldDoc.isFolder != newDoc.isFolder ||
                     oldDoc.itemCount != newDoc.itemCount);

                if (itemChanged) {
                    ensureReconciling();
                    m_documents[i] = newDoc;
                    QModelIndex idx = index(i);
                    emit dataChanged(idx, idx);
                }
            }
        }

        // 3. Add new items
        for (const auto &newDoc : newDocs) {
            if (!currentMap.contains(newDoc.absolutePath)) {
                ensureReconciling();
                int insertPos = m_documents.size();
                beginInsertRows(QModelIndex(), insertPos, insertPos);
                m_documents.append(newDoc);
                endInsertRows();
            }
        }

        if (isReconciling) {
            emit reconciled();
        }
    }

    if (changed) {
        emit countsChanged();
    }
}

void DocumentModel::updateThumbnail(int docId, const QString &thumbnailPath)
{
    for (int i = 0; i < m_documents.size(); ++i) {
        if (m_documents.at(i).id == docId) {
            m_documents[i].thumbnailPath = "file://" + thumbnailPath;
            QModelIndex idx = index(i);
            emit dataChanged(idx, idx, {ThumbnailPathRole});
            break;
        }
    }
}

QVariantMap DocumentModel::getDocument(int docId) const
{
    for (const auto &doc : m_documents) {
        if (doc.id == docId) {
            QVariantMap map;
            map["docId"] = doc.id;
            map["fileName"] = doc.fileName;
            map["absolutePath"] = doc.absolutePath;
            map["fileSize"] = doc.fileSize;
            map["pageCount"] = doc.pageCount;
            map["starRating"] = doc.starRating;
            map["isOffline"] = doc.isOffline;
            map["tags"] = doc.tags;
            map["textSnippet"] = doc.textSnippet;
            map["notes"] = doc.notes;
            map["thumbnailPath"] = doc.thumbnailPath;
            map["dateCreated"] = doc.dateCreated;
            map["dateModified"] = doc.dateModified;
            map["dateAdded"] = doc.dateAdded;
            map["lastOpened"] = doc.lastOpened;
            map["isFolder"] = doc.isFolder;
            map["itemCount"] = doc.itemCount;

            // Formatted fields
            qint64 kb = doc.fileSize / 1024;
            if (kb > 1024) {
                map["fileSizeStr"] = QString("%1 MB").arg(double(kb) / 1024.0, 0, 'f', 1);
            } else {
                map["fileSizeStr"] = QString("%1 KB").arg(kb);
            }

            if (doc.isFolder) {
                map["itemCountStr"] =
                    QString("%1 item%2").arg(doc.itemCount).arg(doc.itemCount == 1 ? "" : "s");
            } else {
                map["itemCountStr"] = "";
            }

            return map;
        }
    }
    return QVariantMap();
}

int DocumentModel::findDocIdByPath(const QString &path) const
{
    for (const auto &doc : m_documents) {
        if (doc.absolutePath == path) {
            return doc.id;
        }
    }
    return -1;
}
