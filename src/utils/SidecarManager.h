/*
 * Copyright (c) 2026, NinjaLibrary
 * All rights reserved.
 */

#ifndef SIDECARMANAGER_H
#define SIDECARMANAGER_H

#include <QSqlDatabase>
#include <QString>
#include <QStringList>

struct SidecarInputs {
    QString docPath;
    QStringList tags;
    int rating = 0;
    QString notes;
    bool success = false;
};

class SidecarManager
{
public:
    explicit SidecarManager(const QString &sidecarDir = QString());

    QString sidecarDir() const { return m_sidecarDir; }
    QString getSidecarPath(const QString &documentPath) const;

    bool writeSidecar(const QString &documentPath, const QStringList &tags, int rating, const QString &notes);
    bool readSidecar(const QString &documentPath, QStringList &tags, int &rating, QString &notes);
    void cleanupOrphanSidecars(QSqlDatabase &db);

    static SidecarInputs loadSidecarInputs(int docId, QSqlDatabase &db);

private:
    QString m_sidecarDir;
};

#endif // SIDECARMANAGER_H
