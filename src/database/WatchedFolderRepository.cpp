/*
 * Copyright (c) 2026, NinjaLibrary
 * All rights reserved.
 */

#include "WatchedFolderRepository.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>

bool WatchedFolderRepository::addWatchedFolder(QSqlDatabase &db, const QString &absPath, const QByteArray &bookmark)
{
    if (!db.isOpen() || absPath.isEmpty()) return false;

    QSqlQuery query(db);
    query.prepare(
        "INSERT OR IGNORE INTO watched_folders (absolute_path, macos_bookmark) VALUES (:path, "
        ":bookmark);");
    query.bindValue(":path", absPath);
    query.bindValue(":bookmark", bookmark);

    if (!query.exec()) {
        qWarning() << "WatchedFolderRepository: Failed to add watched folder:" << query.lastError().text();
        return false;
    }
    return true;
}

bool WatchedFolderRepository::removeWatchedFolder(QSqlDatabase &db, const QString &absPath)
{
    if (!db.isOpen() || absPath.isEmpty()) return false;

    QSqlQuery query(db);
    query.prepare("DELETE FROM watched_folders WHERE absolute_path = :path;");
    query.bindValue(":path", absPath);

    if (!query.exec()) {
        qWarning() << "WatchedFolderRepository: Failed to remove watched folder:" << query.lastError().text();
        return false;
    }
    return true;
}

QStringList WatchedFolderRepository::getWatchedFolders(QSqlDatabase &db)
{
    QStringList folders;
    if (!db.isOpen()) return folders;

    QSqlQuery query("SELECT absolute_path FROM watched_folders;", db);
    while (query.next()) {
        folders.append(query.value(0).toString());
    }
    return folders;
}

int WatchedFolderRepository::findFolderIdAndMatchedPath(QSqlDatabase &db, const QStringList &possiblePaths, QString &outMatchedPath)
{
    if (!db.isOpen()) return -1;

    for (const QString &pathCandidate : possiblePaths) {
        QSqlQuery idQuery(db);
        idQuery.prepare("SELECT id, absolute_path FROM watched_folders WHERE absolute_path = :path;");
        idQuery.bindValue(":path", pathCandidate);
        if (idQuery.exec() && idQuery.next()) {
            int folderId = idQuery.value(0).toInt();
            outMatchedPath = idQuery.value(1).toString();
            return folderId;
        }
    }
    return -1;
}

bool WatchedFolderRepository::updateFolderPath(QSqlDatabase &db, const QString &oldPath, const QString &newPath)
{
    if (!db.isOpen()) return false;

    QSqlQuery updateQuery(db);
    updateQuery.prepare("UPDATE watched_folders SET absolute_path = :newPath WHERE absolute_path = :oldPath;");
    updateQuery.bindValue(":newPath", newPath);
    updateQuery.bindValue(":oldPath", oldPath);

    if (!updateQuery.exec()) {
        qWarning() << "WatchedFolderRepository: Failed to update watched folder path:" << updateQuery.lastError().text();
        return false;
    }
    return true;
}

bool WatchedFolderRepository::recordActiveScan(QSqlDatabase &db, const QString &absPath)
{
    if (!db.isOpen() || absPath.isEmpty()) return false;

    QSqlQuery recordScan(db);
    recordScan.prepare("INSERT OR IGNORE INTO active_scans (folder_path) VALUES (:path);");
    recordScan.bindValue(":path", absPath);
    if (!recordScan.exec()) {
        qWarning() << "WatchedFolderRepository: Failed to record active scan:" << recordScan.lastError().text();
        return false;
    }
    return true;
}

bool WatchedFolderRepository::removeActiveScan(QSqlDatabase &db, const QString &folderPath)
{
    if (!db.isOpen() || folderPath.isEmpty()) return false;

    QSqlQuery removeScan(db);
    removeScan.prepare("DELETE FROM active_scans WHERE folder_path = :path;");
    removeScan.bindValue(":path", folderPath);
    if (!removeScan.exec()) {
        qWarning() << "WatchedFolderRepository: Failed to remove active scan record:" << removeScan.lastError().text();
        return false;
    }
    return true;
}

QStringList WatchedFolderRepository::getActiveScans(QSqlDatabase &db)
{
    QStringList paths;
    if (!db.isOpen()) return paths;

    QSqlQuery query("SELECT folder_path FROM active_scans;", db);
    while (query.next()) {
        paths.append(query.value(0).toString());
    }
    return paths;
}
