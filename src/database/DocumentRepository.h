/*
 * Copyright (c) 2026, NinjaLibrary
 * All rights reserved.
 */

#ifndef DOCUMENTREPOSITORY_H
#define DOCUMENTREPOSITORY_H

#include <QList>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QVariantList>

class DocumentRepository
{
public:
    static QList<int> getDocIdsByFolderId(QSqlDatabase &db, int folderId);
    static int getDocIdByPath(QSqlDatabase &db, const QString &absPath);
    static bool getDocDetailsForSidecar(QSqlDatabase &db, int docId, QString &outPath, int &outRating);
    
    static bool updateRating(QSqlDatabase &db, int docId, int rating);
    static bool updateNotes(QSqlDatabase &db, int docId, const QString &notes);
    static bool markDocumentOpened(QSqlDatabase &db, int docId);
    
    static bool batchUpdateTags(QSqlDatabase &db, const QList<int> &docIds, const QStringList &tags);
    static bool batchUpdateRating(QSqlDatabase &db, const QList<int> &docIds, int rating);
    static bool batchAddTags(QSqlDatabase &db, const QList<int> &docIds, const QStringList &tags);
    static bool batchRemoveTags(QSqlDatabase &db, const QList<int> &docIds, const QStringList &tags);
    
    static QStringList getUniqueTags(QSqlDatabase &db);
    static QString getDocumentText(QSqlDatabase &db, int docId);
    static QVariantList searchDocuments(QSqlDatabase &db, const QString &queryStr);
    static QVariantList searchDocumentContent(QSqlDatabase &db, int docId, const QString &queryStr);
};

#endif // DOCUMENTREPOSITORY_H
