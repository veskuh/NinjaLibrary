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

    m_refreshWatcher = new QFutureWatcher<ModelDiff>(this);
    connect(m_refreshWatcher, &QFutureWatcher<ModelDiff>::finished, this,
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

static ModelDiff computeRefreshSnapshotOffThread(DatabaseManager *dbMgr, const QList<DocumentInfo>& oldDocs)
{
    ModelDiff diff;
    QList<DocumentInfo> newDocs;
    QSqlDatabase db = dbMgr->getDatabaseConnection();
    if (!db.isOpen()) return diff;

    QList<QSqlRecord> records;
    {
        QSqlQuery query(
            "SELECT d.id, d.folder_id, d.file_name, d.absolute_path, d.file_size, d.file_hash, "
            "       d.date_created, d.date_modified, d.date_added, d.page_count, d.star_rating, "
            "d.is_offline, "
            "       d.last_opened, "
            "       (SELECT group_concat(t.name, ',') FROM tags t JOIN document_tags dt ON t.id = "
            "dt.tag_id WHERE dt.document_id = d.id) as tags_list "
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

        doc.textSnippet = QString();
        doc.notes = QString();

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

    diff.finalDocuments = newDocs;

    if (oldDocs.isEmpty()) {
        diff.requiresReset = true;
        return diff;
    }

    QMap<QString, DocumentInfo> currentMap;
    for (const auto &doc : oldDocs) {
        currentMap[doc.absolutePath] = doc;
    }

    QMap<QString, DocumentInfo> newMap;
    for (const auto &doc : newDocs) {
        newMap[doc.absolutePath] = doc;
    }

    // 1. Remove items no longer present (descending order)
    for (int i = oldDocs.size() - 1; i >= 0; --i) {
        if (!newMap.contains(oldDocs.at(i).absolutePath)) {
            diff.rowsToRemove.append(i);
        }
    }

    // 2. Update existing items
    for (int i = 0; i < oldDocs.size(); ++i) {
        const QString &path = oldDocs.at(i).absolutePath;
        if (newMap.contains(path)) {
            const DocumentInfo &newDoc = newMap.value(path);
            const DocumentInfo &oldDoc = oldDocs.at(i);

            bool itemChanged =
                (oldDoc.id != newDoc.id || oldDoc.folderId != newDoc.folderId ||
                 oldDoc.fileName != newDoc.fileName || oldDoc.fileSize != newDoc.fileSize ||
                 oldDoc.fileHash != newDoc.fileHash ||
                 oldDoc.dateCreated != newDoc.dateCreated ||
                 oldDoc.dateModified != newDoc.dateModified ||
                 oldDoc.dateAdded != newDoc.dateAdded || oldDoc.pageCount != newDoc.pageCount ||
                 oldDoc.starRating != newDoc.starRating ||
                 oldDoc.isOffline != newDoc.isOffline || oldDoc.tags != newDoc.tags ||
                 oldDoc.thumbnailPath != newDoc.thumbnailPath ||
                 oldDoc.lastOpened != newDoc.lastOpened || oldDoc.isFolder != newDoc.isFolder ||
                 oldDoc.itemCount != newDoc.itemCount);

            if (itemChanged) {
                diff.rowsToUpdate.append(qMakePair(i, newDoc));
            }
        }
    }

    // 3. Add new items
    for (const auto &newDoc : newDocs) {
        if (!currentMap.contains(newDoc.absolutePath)) {
            diff.rowsToInsert.append(qMakePair(-1, newDoc));
        }
    }
    
    // Fallback to reset if changes are too massive (e.g. over 1000 individual operations)
    if (diff.rowsToRemove.size() + diff.rowsToUpdate.size() + diff.rowsToInsert.size() > 1000) {
        diff.requiresReset = true;
    }

    return diff;
}

void DocumentModel::forceRefresh()
{
    m_refreshTimer->stop();

    if (m_isRefreshing) {
        m_refreshTimer->start(200);
        return;
    }

    m_isRefreshing = true;
    QList<DocumentInfo> currentDocs = m_documents;
    QFuture<ModelDiff> future =
        QtConcurrent::run(&computeRefreshSnapshotOffThread, m_dbMgr, currentDocs);
    m_refreshWatcher->setFuture(future);
}

void DocumentModel::onRefreshFinished()
{
    m_isRefreshing = false;
    ModelDiff diff = m_refreshWatcher->result();
    const QList<DocumentInfo> &newDocs = diff.finalDocuments;

    // Calculate count changes
    int tempPdf = 0;
    int tempImage = 0;
    int tempText = 0;
    int tempLocal = 0;
    int tempUnavailable = 0;

    for (const auto &doc : newDocs) {
        if (doc.isFolder) continue;

        if (doc.isOffline) tempUnavailable++;
        else tempLocal++;

        QString ext = doc.fileName.split('.').last().toLower();
        if (ext == "pdf") tempPdf++;
        else if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "gif" || ext == "bmp" || ext == "tiff") tempImage++;
        else tempText++;
    }

    bool countsChanged = (m_pdfCount != tempPdf || m_imageCount != tempImage || m_textCount != tempText ||
                          m_localCount != tempLocal || m_unavailableCount != tempUnavailable);
    m_pdfCount = tempPdf;
    m_imageCount = tempImage;
    m_textCount = tempText;
    m_localCount = tempLocal;
    m_unavailableCount = tempUnavailable;

    if (diff.requiresReset) {
        beginResetModel();
        m_documents = newDocs;
        endResetModel();
        rebuildIndexes();
    } else {
        bool isReconciling = false;
        auto ensureReconciling = [this, &isReconciling]() {
            if (!isReconciling) {
                isReconciling = true;
                emit aboutToReconcile();
            }
        };

        // 1. Update rows first so original indices from diff.rowsToUpdate are valid
        int updateStart = -1;
        int updateEnd = -1;
        for (const auto &pair : diff.rowsToUpdate) {
            ensureReconciling();
            int row = pair.first;
            m_documents[row] = pair.second;
            if (updateStart == -1) {
                updateStart = updateEnd = row;
            } else if (row == updateEnd + 1) {
                updateEnd = row;
            } else {
                emit dataChanged(index(updateStart), index(updateEnd));
                updateStart = updateEnd = row;
            }
        }
        if (updateStart != -1) {
            emit dataChanged(index(updateStart), index(updateEnd));
        }

        // 2. Remove rows in contiguous batches (rowsToRemove is descending)
        int removeEnd = -1;
        int removeStart = -1;
        for (int row : diff.rowsToRemove) {
            if (removeEnd == -1) {
                removeEnd = removeStart = row;
            } else if (row == removeStart - 1) {
                removeStart = row;
            } else {
                ensureReconciling();
                beginRemoveRows(QModelIndex(), removeStart, removeEnd);
                m_documents.erase(m_documents.begin() + removeStart, m_documents.begin() + removeEnd + 1);
                endRemoveRows();
                removeEnd = removeStart = row;
            }
        }
        if (removeEnd != -1) {
            ensureReconciling();
            beginRemoveRows(QModelIndex(), removeStart, removeEnd);
            m_documents.erase(m_documents.begin() + removeStart, m_documents.begin() + removeEnd + 1);
            endRemoveRows();
        }

        // 3. Insert new rows in a single batch
        if (!diff.rowsToInsert.isEmpty()) {
            ensureReconciling();
            int startRow = m_documents.size();
            beginInsertRows(QModelIndex(), startRow, startRow + diff.rowsToInsert.size() - 1);
            for (const auto &pair : diff.rowsToInsert) {
                m_documents.append(pair.second);
            }
            endInsertRows();
        }

        if (isReconciling) {
            rebuildIndexes();
            emit reconciled();
        }
    }

    if (countsChanged || !diff.rowsToRemove.isEmpty() || !diff.rowsToUpdate.isEmpty() || !diff.rowsToInsert.isEmpty() || diff.requiresReset) {
        emit this->countsChanged();
    }
    
    emit refreshCompleted();
}

void DocumentModel::rebuildIndexes()
{
    m_idToRow.clear();
    m_pathToRow.clear();
    for (int i = 0; i < m_documents.size(); ++i) {
        m_idToRow[m_documents.at(i).id] = i;
        m_pathToRow[m_documents.at(i).absolutePath] = i;
    }
}

void DocumentModel::updateThumbnail(int docId, const QString &thumbnailPath)
{
    if (!m_idToRow.contains(docId)) return;
    int row = m_idToRow.value(docId);
    m_documents[row].thumbnailPath = "file://" + thumbnailPath;
    QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {ThumbnailPathRole});
}

QVariantMap DocumentModel::getDocument(int docId) const
{
    if (!m_idToRow.contains(docId)) return QVariantMap();
    int row = m_idToRow.value(docId);
    const auto &doc = m_documents.at(row);

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
    return QVariantMap();
}

int DocumentModel::findDocIdByPath(const QString &path) const
{
    if (!m_pathToRow.contains(path)) return -1;
    int row = m_pathToRow.value(path);
    return m_documents.at(row).id;
}
