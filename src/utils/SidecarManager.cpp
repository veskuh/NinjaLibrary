/*
 * Copyright (c) 2026, NinjaLibrary
 * All rights reserved.
 */

#include "SidecarManager.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSqlQuery>
#include <QStandardPaths>

#include "../database/DocumentRepository.h"

SidecarManager::SidecarManager(const QString &sidecarDir)
{
    if (sidecarDir.isEmpty()) {
        m_sidecarDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/sidecars/";
    } else {
        m_sidecarDir = sidecarDir;
    }
    if (!m_sidecarDir.endsWith("/")) {
        m_sidecarDir += "/";
    }
    QDir().mkpath(m_sidecarDir);
}

QString SidecarManager::getSidecarPath(const QString &documentPath) const
{
    if (documentPath.isEmpty()) return QString();
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(documentPath.toUtf8());
    QString hashStr = hash.result().toHex();
    return m_sidecarDir + hashStr + ".ninja";
}

bool SidecarManager::writeSidecar(const QString &documentPath, const QStringList &tags, int rating, const QString &notes)
{
    QString sidecarPath = getSidecarPath(documentPath);
    if (sidecarPath.isEmpty()) return false;

    QJsonObject obj;
    obj["document_path"] = documentPath;
    obj["star_rating"] = rating;
    obj["notes"] = notes;

    QJsonArray tagsArray;
    for (const QString &tag : tags) {
        tagsArray.append(tag);
    }
    obj["tags"] = tagsArray;

    QJsonDocument doc(obj);
    QSaveFile file(sidecarPath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(doc.toJson());
    return file.commit();
}

bool SidecarManager::readSidecar(const QString &documentPath, QStringList &tags, int &rating, QString &notes)
{
    QString sidecarPath = getSidecarPath(documentPath);
    if (sidecarPath.isEmpty()) return false;

    QFile file(sidecarPath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) return false;
    QJsonObject obj = doc.object();

    rating = obj["star_rating"].toInt(0);
    notes = obj["notes"].toString();

    tags.clear();
    QJsonArray tagsArray = obj["tags"].toArray();
    for (int i = 0; i < tagsArray.size(); ++i) {
        tags.append(tagsArray.at(i).toString());
    }

    return true;
}

void SidecarManager::cleanupOrphanSidecars(QSqlDatabase &db)
{
    if (!db.isOpen()) return;

    QSet<QString> activeHashes;
    {
        QSqlQuery query("SELECT absolute_path FROM documents;", db);
        while (query.next()) {
            QString docPath = query.value(0).toString();
            QCryptographicHash hasher(QCryptographicHash::Sha256);
            hasher.addData(docPath.toUtf8());
            activeHashes.insert(hasher.result().toHex());
        }
    }

    QDir dir(m_sidecarDir);
    QStringList filters;
    filters << "*.ninja";
    QStringList files = dir.entryList(filters, QDir::Files);
    for (const QString &filename : files) {
        QString baseName = QFileInfo(filename).baseName();
        if (!activeHashes.contains(baseName)) {
            dir.remove(filename);
        }
    }
}

SidecarInputs SidecarManager::loadSidecarInputs(int docId, QSqlDatabase &db)
{
    SidecarInputs inputs;
    if (!DocumentRepository::getDocDetailsForSidecar(db, docId, inputs.docPath, inputs.rating)) {
        return inputs;
    }

    QSqlQuery tagQuery(db);
    tagQuery.prepare(
        "SELECT t.name FROM tags t JOIN document_tags dt ON t.id = dt.tag_id WHERE "
        "dt.document_id = :docId;");
    tagQuery.bindValue(":docId", docId);
    if (tagQuery.exec()) {
        while (tagQuery.next()) {
            inputs.tags << tagQuery.value(0).toString();
        }
    }

    QSqlQuery notesQuery(db);
    notesQuery.prepare("SELECT notes FROM document_search WHERE rowid = :docId;");
    notesQuery.bindValue(":docId", docId);
    if (notesQuery.exec() && notesQuery.next()) {
        inputs.notes = notesQuery.value(0).toString();
    }

    inputs.success = true;
    return inputs;
}
