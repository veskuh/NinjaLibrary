/*
 * Copyright (c) 2026, NinjaLibrary
 * All rights reserved.
 */

#ifndef WATCHEDFOLDERREPOSITORY_H
#define WATCHEDFOLDERREPOSITORY_H

#include <QByteArray>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>

class WatchedFolderRepository
{
public:
    static bool addWatchedFolder(QSqlDatabase &db, const QString &absPath, const QByteArray &bookmark);
    static bool removeWatchedFolder(QSqlDatabase &db, const QString &absPath);
    static QStringList getWatchedFolders(QSqlDatabase &db);
    static int findFolderIdAndMatchedPath(QSqlDatabase &db, const QStringList &possiblePaths, QString &outMatchedPath);
    static bool updateFolderPath(QSqlDatabase &db, const QString &oldPath, const QString &newPath);
    
    // Active scans tracking
    static bool recordActiveScan(QSqlDatabase &db, const QString &absPath);
    static bool removeActiveScan(QSqlDatabase &db, const QString &folderPath);
    static QStringList getActiveScans(QSqlDatabase &db);
};

#endif // WATCHEDFOLDERREPOSITORY_H
