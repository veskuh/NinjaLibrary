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
#include <QSqlQuery>
#include <QSqlError>
#include <QDir>
#include <QFile>
#include <QCryptographicHash>
#include <QDebug>

DocumentModel::DocumentModel(DatabaseManager *dbMgr, QObject *parent)
    : QAbstractListModel(parent)
    , m_dbMgr(dbMgr)
{
    refresh();
}

DocumentModel::~DocumentModel()
{
}

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
        case IdRole: return doc.id;
        case FolderIdRole: return doc.folderId;
        case FileNameRole: return doc.fileName;
        case AbsolutePathRole: return doc.absolutePath;
        case FileSizeRole: return doc.fileSize;
        case FileHashRole: return doc.fileHash;
        case DateCreatedRole: return doc.dateCreated;
        case DateModifiedRole: return doc.dateModified;
        case DateAddedRole: return doc.dateAdded;
        case PageCountRole: return doc.pageCount;
        case StarRatingRole: return doc.starRating;
        case IsOfflineRole: return doc.isOffline;
        case TagsRole: return doc.tags;
        case TextSnippetRole: return doc.textSnippet;
        case NotesRole: return doc.notes;
        case ThumbnailPathRole: return doc.thumbnailPath;
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
        case LastOpenedRole: return doc.lastOpened;
        default: return QVariant();
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
    return roles;
}

void DocumentModel::refresh()
{
    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    if (!db.isOpen()) return;

    beginResetModel();
    m_documents.clear();

    QSqlQuery query(
        "SELECT d.id, d.folder_id, d.file_name, d.absolute_path, d.file_size, d.file_hash, "
        "       d.date_created, d.date_modified, d.date_added, d.page_count, d.star_rating, d.is_offline, "
        "       d.last_opened, "
        "       (SELECT group_concat(t.name, ',') FROM tags t JOIN document_tags dt ON t.id = dt.tag_id WHERE dt.document_id = d.id) as tags_list, "
        "       (SELECT text_snippet FROM document_search WHERE document_id = d.id) as text, "
        "       (SELECT notes FROM document_search WHERE document_id = d.id) as notes "
        "FROM documents d;", db
    );

    QString cacheDir = QDir::homePath() + "/.cache/NinjaLibrary/thumbnails/";

    while (query.next()) {
        DocumentInfo doc;
        doc.id = query.value(0).toInt();
        doc.folderId = query.value(1).toInt();
        doc.fileName = query.value(2).toString();
        doc.absolutePath = query.value(3).toString();
        doc.fileSize = query.value(4).toLongLong();
        doc.fileHash = query.value(5).toString();
        doc.dateCreated = query.value(6).toDateTime();
        doc.dateModified = query.value(7).toDateTime();
        doc.dateAdded = query.value(8).toDateTime();
        doc.pageCount = query.value(9).toInt();
        doc.starRating = query.value(10).toInt();
        doc.isOffline = query.value(11).toBool();
        doc.lastOpened = query.value(12).toLongLong();

        QString tagsStr = query.value(13).toString();
        if (!tagsStr.isEmpty()) {
            doc.tags = tagsStr.split(",", Qt::SkipEmptyParts);
        } else {
            doc.tags = QStringList();
        }

        doc.textSnippet = query.value(14).toString();
        doc.notes = query.value(15).toString();

        // Calculate expected thumbnail location
        QCryptographicHash hasher(QCryptographicHash::Sha256);
        hasher.addData(doc.absolutePath.toUtf8());
        QString hashStr = hasher.result().toHex();
        QString thumbFile = cacheDir + hashStr + ".png";

        if (QFile::exists(thumbFile)) {
            doc.thumbnailPath = "file://" + thumbFile;
        } else {
            // default or empty (let UI handle fallback thumbnail)
            doc.thumbnailPath = "";
        }

        m_documents.append(doc);
    }

    endResetModel();
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
