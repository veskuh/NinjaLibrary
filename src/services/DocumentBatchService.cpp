/*
 * Copyright (c) 2026, NinjaLibrary
 * All rights reserved.
 */

#include "DocumentBatchService.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QFile>
#include <QSqlDatabase>
#include <QStandardPaths>
#include <QUrl>

#include "../database/DatabaseManager.h"
#include "../database/DocumentRepository.h"
#include "../utils/SidecarManager.h"

DocumentBatchService::DocumentBatchService(DatabaseManager *dbMgr, SidecarManager *sidecarMgr, QObject *parent)
    : QObject(parent), m_dbMgr(dbMgr), m_sidecarMgr(sidecarMgr)
{
}

bool DocumentBatchService::moveToTrash(int documentId, const QString &filePath)
{
    QUrl url(filePath);
    QString localPath = url.isLocalFile() ? url.toLocalFile() : filePath;

    if (localPath.isEmpty() || !QFile::exists(localPath)) {
        qWarning() << "moveToTrash: File does not exist or path is empty:" << localPath;
        return false;
    }

    if (!QFile::moveToTrash(localPath)) {
        qWarning() << "moveToTrash: Failed to move file to trash:" << localPath;
        return false;
    }

    // Clean up sidecar file if exists
    QString sidecarPath = m_sidecarMgr->getSidecarPath(localPath);
    if (!sidecarPath.isEmpty() && QFile::exists(sidecarPath)) {
        QFile::remove(sidecarPath);
    }

    // Clean up cached thumbnail file if exists
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(localPath.toUtf8());
    QString hashStr = hash.result().toHex();
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/thumbnails/";
    QString thumbPath = cacheDir + hashStr + ".png";
    if (QFile::exists(thumbPath)) {
        QFile::remove(thumbPath);
    }

    // Remove from database
    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    if (!db.isOpen()) return false;

    db.transaction();

    if (!DatabaseManager::deleteDocumentCascade(db, documentId)) {
        db.rollback();
        return false;
    }

    db.commit();
    return true;
}

bool DocumentBatchService::batchUpdateTags(const QList<int> &documentIds, const QStringList &tags)
{
    if (documentIds.isEmpty()) return true;

    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    if (!db.isOpen()) return false;

    db.transaction();
    if (!DocumentRepository::batchUpdateTags(db, documentIds, tags)) {
        db.rollback();
        return false;
    }

    for (int docId : documentIds) {
        SidecarInputs inputs = SidecarManager::loadSidecarInputs(docId, db);
        if (inputs.success) {
            m_sidecarMgr->writeSidecar(inputs.docPath, tags, inputs.rating, inputs.notes);
        }
    }

    db.commit();
    return true;
}

bool DocumentBatchService::batchAddTags(const QList<int> &documentIds, const QStringList &tags)
{
    if (documentIds.isEmpty() || tags.isEmpty()) return true;

    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    if (!db.isOpen()) return false;

    db.transaction();
    if (!DocumentRepository::batchAddTags(db, documentIds, tags)) {
        db.rollback();
        return false;
    }

    for (int docId : documentIds) {
        SidecarInputs inputs = SidecarManager::loadSidecarInputs(docId, db);
        if (inputs.success) {
            m_sidecarMgr->writeSidecar(inputs.docPath, inputs.tags, inputs.rating, inputs.notes);
        }
    }

    db.commit();
    return true;
}

bool DocumentBatchService::batchRemoveTags(const QList<int> &documentIds, const QStringList &tags)
{
    if (documentIds.isEmpty() || tags.isEmpty()) return true;

    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    if (!db.isOpen()) return false;

    db.transaction();
    if (!DocumentRepository::batchRemoveTags(db, documentIds, tags)) {
        db.rollback();
        return false;
    }

    for (int docId : documentIds) {
        SidecarInputs inputs = SidecarManager::loadSidecarInputs(docId, db);
        if (inputs.success) {
            m_sidecarMgr->writeSidecar(inputs.docPath, inputs.tags, inputs.rating, inputs.notes);
        }
    }

    db.commit();
    return true;
}

bool DocumentBatchService::batchUpdateRating(const QList<int> &documentIds, int rating)
{
    if (documentIds.isEmpty()) return true;
    if (rating < 0 || rating > 5) return false;

    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    if (!db.isOpen()) return false;

    db.transaction();
    if (!DocumentRepository::batchUpdateRating(db, documentIds, rating)) {
        db.rollback();
        return false;
    }

    for (int docId : documentIds) {
        SidecarInputs inputs = SidecarManager::loadSidecarInputs(docId, db);
        if (inputs.success) {
            m_sidecarMgr->writeSidecar(inputs.docPath, inputs.tags, inputs.rating, inputs.notes);
        }
    }

    db.commit();
    return true;
}

bool DocumentBatchService::updateNotes(int docId, const QString &notes)
{
    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    if (!db.isOpen()) return false;

    db.transaction();
    if (!DocumentRepository::updateNotes(db, docId, notes)) {
        db.rollback();
        return false;
    }

    SidecarInputs inputs = SidecarManager::loadSidecarInputs(docId, db);
    if (inputs.success) {
        m_sidecarMgr->writeSidecar(inputs.docPath, inputs.tags, inputs.rating, inputs.notes);
    }

    db.commit();
    return true;
}
