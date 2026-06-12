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

#ifndef DOCUMENTMODEL_H
#define DOCUMENTMODEL_H

#include <QAbstractListModel>
#include <QDateTime>
#include <QList>
#include <QStringList>
#include <QTimer>

#include "../database/DatabaseManager.h"

struct DocumentInfo
{
    int id;
    int folderId;
    QString fileName;
    QString absolutePath;
    qint64 fileSize;
    QString fileHash;
    QDateTime dateCreated;
    QDateTime dateModified;
    QDateTime dateAdded;
    int pageCount;
    int starRating;
    bool isOffline;
    QStringList tags;
    QString textSnippet;
    QString notes;
    QString thumbnailPath;
    qint64 lastOpened;
    bool isFolder = false;
    int itemCount = 0;
};

class DocumentModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int totalCount READ totalCount NOTIFY countsChanged)
    Q_PROPERTY(int pdfCount READ pdfCount NOTIFY countsChanged)
    Q_PROPERTY(int imageCount READ imageCount NOTIFY countsChanged)
    Q_PROPERTY(int textCount READ textCount NOTIFY countsChanged)
    Q_PROPERTY(int localCount READ localCount NOTIFY countsChanged)
    Q_PROPERTY(int unavailableCount READ unavailableCount NOTIFY countsChanged)
public:
    enum DocumentRoles
    {
        IdRole = Qt::UserRole + 1,
        FolderIdRole,
        FileNameRole,
        AbsolutePathRole,
        FileSizeRole,
        FileHashRole,
        DateCreatedRole,
        DateModifiedRole,
        DateAddedRole,
        PageCountRole,
        StarRatingRole,
        IsOfflineRole,
        TagsRole,
        TextSnippetRole,
        NotesRole,
        ThumbnailPathRole,
        FileSizeStrRole,
        StarRatingStrRole,
        OfflineColorRole,
        DateModifiedStrRole,
        TagsStrRole,
        LastOpenedRole,
        IsFolderRole,
        ItemCountRole,
        ItemCountStrRole
    };

    explicit DocumentModel(DatabaseManager *dbMgr, QObject *parent = nullptr);
    ~DocumentModel();

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int totalCount() const { return m_documents.size(); }
    int pdfCount() const { return m_pdfCount; }
    int imageCount() const { return m_imageCount; }
    int textCount() const { return m_textCount; }
    int localCount() const { return m_localCount; }
    int unavailableCount() const { return m_unavailableCount; }

signals:
    void countsChanged();

public slots:
    void refresh();
    void forceRefresh();
    void updateThumbnail(int docId, const QString &thumbnailPath);

private:
    DatabaseManager *m_dbMgr;
    QList<DocumentInfo> m_documents;
    QTimer *m_refreshTimer;
    int m_pdfCount = 0;
    int m_imageCount = 0;
    int m_textCount = 0;
    int m_localCount = 0;
    int m_unavailableCount = 0;
};

#endif  // DOCUMENTMODEL_H
