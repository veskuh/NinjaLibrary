/*
 * Copyright (c) 2026, NinjaLibrary
 * All rights reserved.
 */

#ifndef DOCUMENTBATCHSERVICE_H
#define DOCUMENTBATCHSERVICE_H

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

class DatabaseManager;
class SidecarManager;

class DocumentBatchService : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(DocumentBatchService)

public:
    explicit DocumentBatchService(DatabaseManager *dbMgr, SidecarManager *sidecarMgr, QObject *parent = nullptr);
    ~DocumentBatchService() override = default;

    bool batchUpdateTags(const QList<int> &documentIds, const QStringList &tags);
    bool batchAddTags(const QList<int> &documentIds, const QStringList &tags);
    bool batchRemoveTags(const QList<int> &documentIds, const QStringList &tags);
    bool batchUpdateRating(const QList<int> &documentIds, int rating);
    bool updateNotes(int docId, const QString &notes);
    bool moveToTrash(int documentId, const QString &filePath);

private:
    DatabaseManager *m_dbMgr;
    SidecarManager *m_sidecarMgr;
};

#endif // DOCUMENTBATCHSERVICE_H
