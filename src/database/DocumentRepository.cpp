/*
 * Copyright (c) 2026, NinjaLibrary
 * All rights reserved.
 */

#include "DocumentRepository.h"

#include <QDateTime>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariantMap>

#include "TagRepository.h"

QList<int> DocumentRepository::getDocIdsByFolderId(QSqlDatabase &db, int folderId)
{
    QList<int> docIds;
    if (!db.isOpen() || folderId == -1) return docIds;

    QSqlQuery selectDocs(db);
    selectDocs.prepare("SELECT id FROM documents WHERE folder_id = :folderId;");
    selectDocs.bindValue(":folderId", folderId);
    if (selectDocs.exec()) {
        while (selectDocs.next()) {
            docIds.append(selectDocs.value(0).toInt());
        }
    } else {
        qWarning() << "DocumentRepository: Failed to query documents by folder_id:" << selectDocs.lastError().text();
    }
    return docIds;
}

int DocumentRepository::getDocIdByPath(QSqlDatabase &db, const QString &absPath)
{
    if (!db.isOpen() || absPath.isEmpty()) return -1;

    QSqlQuery query(db);
    query.prepare("SELECT id FROM documents WHERE absolute_path = :path;");
    query.bindValue(":path", absPath);
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    return -1;
}

bool DocumentRepository::getDocDetailsForSidecar(QSqlDatabase &db, int docId, QString &outPath, int &outRating)
{
    if (!db.isOpen() || docId <= 0) return false;

    QSqlQuery query(db);
    query.prepare("SELECT absolute_path, star_rating FROM documents WHERE id = :docId;");
    query.bindValue(":docId", docId);
    if (query.exec() && query.next()) {
        outPath = query.value(0).toString();
        outRating = query.value(1).toInt();
        return true;
    }
    return false;
}

bool DocumentRepository::updateRating(QSqlDatabase &db, int docId, int rating)
{
    if (!db.isOpen() || docId <= 0) return false;

    QSqlQuery query(db);
    query.prepare("UPDATE documents SET star_rating = :rating WHERE id = :docId;");
    query.bindValue(":rating", rating);
    query.bindValue(":docId", docId);
    if (!query.exec()) {
        qWarning() << "DocumentRepository: Failed to update rating:" << query.lastError().text();
        return false;
    }
    return true;
}

bool DocumentRepository::updateNotes(QSqlDatabase &db, int docId, const QString &notes)
{
    if (!db.isOpen() || docId <= 0) return false;

    QSqlQuery query(db);
    query.prepare("UPDATE document_search SET notes = :notes WHERE rowid = :docId;");
    query.bindValue(":notes", notes);
    query.bindValue(":docId", docId);
    if (!query.exec()) {
        qWarning() << "DocumentRepository: Failed to update notes:" << query.lastError().text();
        return false;
    }
    return true;
}

bool DocumentRepository::markDocumentOpened(QSqlDatabase &db, int docId)
{
    if (!db.isOpen() || docId <= 0) return false;

    QSqlQuery query(db);
    query.prepare("UPDATE documents SET last_opened = :lastOpened WHERE id = :docId;");
    query.bindValue(":lastOpened", QDateTime::currentSecsSinceEpoch());
    query.bindValue(":docId", docId);
    if (!query.exec()) {
        qWarning() << "DocumentRepository: Failed to mark document opened:" << query.lastError().text();
        return false;
    }
    return true;
}

bool DocumentRepository::batchUpdateTags(QSqlDatabase &db, const QList<int> &docIds, const QStringList &tags)
{
    if (!db.isOpen()) return false;
    if (docIds.isEmpty()) return true;

    for (int docId : docIds) {
        QSqlQuery clearTags(db);
        clearTags.prepare("DELETE FROM document_tags WHERE document_id = :docId;");
        clearTags.bindValue(":docId", docId);
        if (!clearTags.exec()) {
            qWarning() << "DocumentRepository: Failed to clear document tags:" << clearTags.lastError().text();
            return false;
        }

        for (const QString &tag : tags) {
            if (!TagRepository::ensureTagLinked(db, docId, tag)) {
                return false;
            }
        }
    }
    return true;
}

bool DocumentRepository::batchUpdateRating(QSqlDatabase &db, const QList<int> &docIds, int rating)
{
    if (!db.isOpen()) return false;
    if (docIds.isEmpty()) return true;
    if (rating < 0 || rating > 5) return false;

    QSqlQuery query(db);
    for (int docId : docIds) {
        query.prepare("UPDATE documents SET star_rating = :rating WHERE id = :docId;");
        query.bindValue(":rating", rating);
        query.bindValue(":docId", docId);
        if (!query.exec()) {
            qWarning() << "DocumentRepository: Failed to batch update rating:" << query.lastError().text();
            return false;
        }
    }
    return true;
}

bool DocumentRepository::batchAddTags(QSqlDatabase &db, const QList<int> &docIds, const QStringList &tags)
{
    if (!db.isOpen()) return false;
    if (docIds.isEmpty()) return true;

    for (int docId : docIds) {
        for (const QString &tag : tags) {
            if (!TagRepository::ensureTagLinked(db, docId, tag)) {
                return false;
            }
        }
    }
    return true;
}

bool DocumentRepository::batchRemoveTags(QSqlDatabase &db, const QList<int> &docIds, const QStringList &tags)
{
    if (!db.isOpen()) return false;
    if (docIds.isEmpty()) return true;

    for (int docId : docIds) {
        for (const QString &tag : tags) {
            QString trimmed = tag.trimmed();
            if (trimmed.isEmpty()) continue;

            int tagId = TagRepository::getTagId(db, trimmed);
            if (tagId != -1) {
                QSqlQuery removeLink(db);
                removeLink.prepare("DELETE FROM document_tags WHERE document_id = :docId AND tag_id = :tagId;");
                removeLink.bindValue(":docId", docId);
                removeLink.bindValue(":tagId", tagId);
                if (!removeLink.exec()) {
                    qWarning() << "DocumentRepository: Failed to remove tag link:" << removeLink.lastError().text();
                    return false;
                }
            }
        }
    }
    return true;
}

QStringList DocumentRepository::getUniqueTags(QSqlDatabase &db)
{
    QStringList tags;
    if (!db.isOpen()) return tags;

    QSqlQuery query(db);
    query.prepare(
        "SELECT DISTINCT t.name FROM tags t JOIN document_tags dt ON t.id = dt.tag_id ORDER BY t.name "
        "ASC;");
    if (query.exec()) {
        while (query.next()) {
            tags.append(query.value(0).toString());
        }
    }
    return tags;
}

QString DocumentRepository::getDocumentText(QSqlDatabase &db, int docId)
{
    if (!db.isOpen() || docId <= 0) return QString();

    QSqlQuery q(db);
    q.prepare("SELECT text_snippet FROM document_search WHERE rowid = :docId;");
    q.bindValue(":docId", docId);
    if (q.exec() && q.next()) {
        return q.value(0).toString();
    }
    return QString();
}

QVariantList DocumentRepository::searchDocuments(QSqlDatabase &db, const QString &queryStr)
{
    QVariantList results;
    QString trimmed = queryStr.trimmed();
    if (trimmed.isEmpty() || !db.isOpen()) return results;

    QSqlQuery query(db);
    query.prepare(
        "SELECT d.id, d.file_name, d.absolute_path, d.file_size, d.is_offline, d.star_rating, ds.text_snippet, "
        "       (SELECT group_concat(t.name) FROM document_tags dt JOIN tags t ON dt.tag_id = t.id WHERE dt.document_id = d.id) as tags "
        "FROM documents d "
        "JOIN document_search ds ON d.id = ds.rowid "
        "WHERE d.id IN (SELECT rowid FROM document_search WHERE document_search MATCH :ftsQuery) "
        "ORDER BY d.file_name ASC;"
    );

    QStringList terms = trimmed.split(" ", Qt::SkipEmptyParts);
    QString ftsQuery;
    for (int i = 0; i < terms.size(); ++i) {
        if (i > 0) ftsQuery += " AND ";
        QString term = terms[i];
        term.replace("\"", "").replace("'", "").replace("*", "").replace(":", "");
        if (!term.isEmpty()) {
            ftsQuery += term + "*";
        }
    }

    if (ftsQuery.isEmpty()) return results;

    query.bindValue(":ftsQuery", ftsQuery);

    if (query.exec()) {
        while (query.next()) {
            QVariantMap doc;
            doc["docId"] = query.value(0).toInt();
            doc["id"] = query.value(0).toInt();
            doc["fileName"] = query.value(1).toString();
            doc["absolutePath"] = query.value(2).toString();
            
            qint64 size = query.value(3).toLongLong();
            doc["fileSize"] = size;
            QString sizeStr;
            if (size < 1024) sizeStr = QString("%1 B").arg(size);
            else if (size < 1024 * 1024) sizeStr = QString("%1 KB").arg(size / 1024);
            else sizeStr = QString("%1 MB").arg(double(size) / (1024 * 1024), 0, 'f', 1);
            doc["fileSizeStr"] = sizeStr;

            doc["isOffline"] = query.value(4).toBool();
            doc["starRating"] = query.value(5).toInt();
            
            QString textSnippet = query.value(6).toString();
            QString tagsConcat = query.value(7).toString();
            QStringList tags = tagsConcat.isEmpty() ? QStringList() : tagsConcat.split(",");
            doc["tags"] = tags;
            
            int matchCount = 0;
            if (!terms.isEmpty()) {
                QString firstTerm = terms[0];
                int idx = 0;
                while ((idx = textSnippet.indexOf(firstTerm, idx, Qt::CaseInsensitive)) != -1) {
                    matchCount++;
                    idx += firstTerm.length();
                }
            }
            doc["matchCount"] = matchCount;
            
            results.append(doc);
        }
    } else {
        qWarning() << "DocumentRepository: searchDocuments query failed:" << query.lastError().text();
    }

    return results;
}

QVariantList DocumentRepository::searchDocumentContent(QSqlDatabase &db, int docId, const QString &queryStr)
{
    QVariantList results;
    if (!db.isOpen() || docId <= 0 || queryStr.isEmpty()) return results;

    QSqlQuery q(db);
    q.prepare("SELECT text_snippet FROM document_search WHERE rowid = :docId;");
    q.bindValue(":docId", docId);
    if (q.exec() && q.next()) {
        QString pageText = q.value(0).toString();
        int index = 0;
        while ((index = pageText.indexOf(queryStr, index, Qt::CaseInsensitive)) != -1) {
            QVariantMap map;
            map["pageIndex"] = 0;
            
            int start = qMax(0, index - 100);
            int end = qMin(pageText.length(), index + queryStr.length() + 100);
            QString prefix = (start > 0) ? "..." : "";
            QString suffix = (end < pageText.length()) ? "..." : "";
            map["context"] = prefix + pageText.mid(start, end - start).replace('\n', ' ') + suffix;
            
            results.append(map);
            index += queryStr.length();
            if (results.size() >= 100) break;
        }
    }
    return results;
}
