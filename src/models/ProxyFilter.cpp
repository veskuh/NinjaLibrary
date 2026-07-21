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

#include "ProxyFilter.h"

#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <algorithm>

#include "DocumentModel.h"

static QString getParentDirectory(const QString &absolutePath)
{
    if (absolutePath.isEmpty()) {
        return QString();
    }
    int lastSlash = absolutePath.lastIndexOf('/');
    int lastBack = absolutePath.lastIndexOf('\\');
    int last = (lastSlash > lastBack) ? lastSlash : lastBack;
    if (last > 0) {
        return absolutePath.left(last);
    } else if (last == 0) {
        return absolutePath.left(1);
    }
    return "";
}

static QString getFileSuffix(const QString &fileName)
{
    int lastDot = fileName.lastIndexOf('.');
    if (lastDot >= 0) {
        int lastSlash = fileName.lastIndexOf('/');
        int lastBack = fileName.lastIndexOf('\\');
        int lastSeparator = (lastSlash > lastBack) ? lastSlash : lastBack;
        if (lastDot > lastSeparator) {
            return fileName.mid(lastDot + 1).toUpper();
        }
    }
    return "";
}

ProxyFilter::ProxyFilter(DatabaseManager *dbMgr, QObject *parent)
    : QSortFilterProxyModel(parent),
      m_dbMgr(dbMgr),
      m_minRating(0),
      m_showUnavailable(true),
      m_duplicatesOnly(false),
      m_categoryFilter("All"),
      m_scopeFilter("All"),
      m_activeScopes(QStringList{"All"}),
      m_includeSubfolderContents(false),
      m_showSubfolderIcons(true),
      m_searchActive(false)
{
    setDynamicSortFilter(true);
    // Sort by file name ascending by default
    setSortRole(DocumentModel::FileNameRole);
    sort(0, Qt::AscendingOrder);

    // Debounce heavy recomputation triggered by source model changes, so bursts of
    // background indexing signals don't each trigger a full filter/scope recalculation
    m_modelChangeTimer = new QTimer(this);
    m_modelChangeTimer->setSingleShot(true);
    m_modelChangeTimer->setInterval(400);
    connect(m_modelChangeTimer, &QTimer::timeout, this, &ProxyFilter::processModelDrivenUpdate);
}

void ProxyFilter::setSortRole(int role) { QSortFilterProxyModel::setSortRole(role); }

ProxyFilter::~ProxyFilter() {}

void ProxyFilter::setSourceModel(QAbstractItemModel *sourceModel)
{
    if (this->sourceModel()) {
        disconnect(this->sourceModel(), SIGNAL(aboutToReconcile()), this, SLOT(onAboutToReconcile()));
        disconnect(this->sourceModel(), SIGNAL(reconciled()), this, SLOT(onReconciled()));
        
        disconnect(this->sourceModel(), SIGNAL(refreshCompleted()), this,
                   SLOT(scheduleModelDrivenUpdate()));
        
        disconnect(this->sourceModel(), &QAbstractItemModel::modelReset, this,
                   &ProxyFilter::scheduleModelDrivenUpdate);
        disconnect(this->sourceModel(), &QAbstractItemModel::rowsInserted, this,
                   &ProxyFilter::scheduleModelDrivenUpdate);
        disconnect(this->sourceModel(), &QAbstractItemModel::rowsRemoved, this,
                   &ProxyFilter::scheduleModelDrivenUpdate);
        disconnect(this->sourceModel(), &QAbstractItemModel::dataChanged, this,
                   &ProxyFilter::scheduleModelDrivenUpdate);
    }
    QSortFilterProxyModel::setSourceModel(sourceModel);
    if (sourceModel) {
        connect(sourceModel, SIGNAL(aboutToReconcile()), this, SLOT(onAboutToReconcile()));
        connect(sourceModel, SIGNAL(reconciled()), this, SLOT(onReconciled()));
        
        connect(sourceModel, SIGNAL(refreshCompleted()), this, SLOT(scheduleModelDrivenUpdate()));

        connect(sourceModel, &QAbstractItemModel::modelReset, this,
                &ProxyFilter::scheduleModelDrivenUpdate);
        connect(sourceModel, &QAbstractItemModel::rowsInserted, this,
                &ProxyFilter::scheduleModelDrivenUpdate);
        connect(sourceModel, &QAbstractItemModel::rowsRemoved, this,
                &ProxyFilter::scheduleModelDrivenUpdate);
        connect(sourceModel, &QAbstractItemModel::dataChanged, this,
                &ProxyFilter::scheduleModelDrivenUpdate);
    }
    updateSearchMatches();
}

void ProxyFilter::onAboutToReconcile()
{
    setDynamicSortFilter(false);
}

void ProxyFilter::onReconciled()
{
    setDynamicSortFilter(true);
    invalidateAndRecalculate();
}

void ProxyFilter::setFilterString(const QString &filter)
{
    if (m_filterString == filter) return;
    m_filterString = filter;
    m_modelChangeTimer->stop();  // User-driven search supersedes pending debounced work
    updateSearchMatches();
    emit filterStringChanged();
}

void ProxyFilter::setSelectedTags(const QStringList &tags)
{
    if (m_selectedTags == tags) return;
    m_selectedTags = tags;
    invalidateAndRecalculate();
    emit selectedTagsChanged();
}

void ProxyFilter::setMinRating(int rating)
{
    if (m_minRating == rating) return;
    m_minRating = rating;
    invalidateAndRecalculate();
    emit minRatingChanged();
}

void ProxyFilter::setShowUnavailable(bool show)
{
    if (m_showUnavailable == show) return;
    m_showUnavailable = show;
    invalidateAndRecalculate();
    emit showUnavailableChanged();
}

void ProxyFilter::setDuplicatesOnly(bool only)
{
    if (m_duplicatesOnly == only) return;
    m_duplicatesOnly = only;
    if (m_duplicatesOnly) {
        updateDuplicateHashes();
    }
    invalidateAndRecalculate();
    emit duplicatesOnlyChanged();
}

void ProxyFilter::setCategoryFilter(const QString &category)
{
    if (m_categoryFilter == category) return;
    m_categoryFilter = category;
    if (m_categoryFilter == "Duplicates") {
        setDuplicatesOnly(true);
    } else {
        setDuplicatesOnly(false);
    }

    if (m_categoryFilter == "Recent") {
        setSortRole(DocumentModel::LastOpenedRole);
        sort(0, Qt::DescendingOrder);
    } else {
        setSortRole(DocumentModel::FileNameRole);
        sort(0, Qt::AscendingOrder);
    }

    invalidateAndRecalculate();
    emit categoryFilterChanged();
}

void ProxyFilter::setFolderFilter(const QString &folder)
{
    if (m_folderFilter == folder) return;
    m_folderFilter = folder;
    invalidateAndRecalculate();
    emit folderFilterChanged();
}

void ProxyFilter::setIncludeSubfolderContents(bool enable)
{
    if (m_includeSubfolderContents == enable) return;
    m_includeSubfolderContents = enable;
    invalidateAndRecalculate();
    emit includeSubfolderContentsChanged();
}

void ProxyFilter::setShowSubfolderIcons(bool enable)
{
    if (m_showSubfolderIcons == enable) return;
    m_showSubfolderIcons = enable;
    invalidateAndRecalculate();
    emit showSubfolderIconsChanged();
}

void ProxyFilter::setScopeFilter(const QString &scope)
{
    if (m_scopeFilter == scope) return;
    m_scopeFilter = scope;
    emit scopeFilterChanged();
    invalidateFilter();
}

void ProxyFilter::updateDuplicateHashes()
{
    m_duplicateHashes.clear();
    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    if (!db.isOpen()) return;

    QSqlQuery query(
        "SELECT file_hash FROM documents WHERE file_hash IS NOT NULL AND file_hash != '' GROUP BY "
        "file_hash HAVING count(*) > 1;",
        db);
    while (query.next()) {
        m_duplicateHashes.insert(query.value(0).toString());
    }
}

void ProxyFilter::updateSearchMatches()
{
    qWarning() << "updateSearchMatches called, filter=" << m_filterString;
    m_searchActive = !m_filterString.trimmed().isEmpty();
    if (!m_searchActive) {
        m_matchedDocIds.clear();
        invalidateAndRecalculate();
        return;
    }

    uint64_t currentGen = ++m_searchGeneration;
    
    if (!m_searchWatcher) {
        m_searchWatcher = new QFutureWatcher<QSet<int>>(this);
    }
    
    // Disconnect old finished signals to avoid multiple/stale lambda invocations
    disconnect(m_searchWatcher, &QFutureWatcher<QSet<int>>::finished, nullptr, nullptr);
    connect(m_searchWatcher, &QFutureWatcher<QSet<int>>::finished, this, [this, currentGen]() {
        if (m_searchGeneration == currentGen) {
            m_matchedDocIds = m_searchWatcher->result();
            invalidateAndRecalculate();
        }
    });

    QString queryStr = m_filterString;
    DatabaseManager* dbMgr = m_dbMgr;

    m_searchWatcher->setFuture(QtConcurrent::run([dbMgr, queryStr]() {
        QSet<int> matched;
        QSqlDatabase db = dbMgr->getDatabaseConnection();
        if (db.isOpen()) {
            QStringList terms = queryStr.split(" ", Qt::SkipEmptyParts);
            bool first = true;
            for (const QString &term : terms) {
                QSet<int> termMatched;

                // 1. FTS match (file_name, text_snippet, notes)
                QSqlQuery qFts(db);
                qFts.prepare("SELECT rowid FROM document_search WHERE document_search MATCH :ftsQuery");
                qFts.bindValue(":ftsQuery", "\"" + term + "\"*");
                if (qFts.exec()) {
                    while (qFts.next()) termMatched.insert(qFts.value(0).toInt());
                } else {
                    qWarning() << "FTS query failed:" << qFts.lastError().text();
                }

                // 2. Tag match
                QSqlQuery qTags(db);
                qTags.prepare("SELECT document_id FROM document_tags dt JOIN tags t ON dt.tag_id = t.id WHERE t.name LIKE :likeQuery");
                qTags.bindValue(":likeQuery", "%" + term + "%");
                if (qTags.exec()) {
                    while (qTags.next()) termMatched.insert(qTags.value(0).toInt());
                }
                
                qWarning() << "Search thread term:" << term << "matches:" << termMatched.size();

                if (first) {
                    matched = termMatched;
                    first = false;
                } else {
                    matched.intersect(termMatched);
                }

                if (matched.isEmpty()) break;
            }
        }
        return matched;
    }));
}

void ProxyFilter::scheduleModelDrivenUpdate()
{
    qWarning() << "scheduleModelDrivenUpdate called";
    m_modelChangeTimer->start();
}

void ProxyFilter::processModelDrivenUpdate()
{
    qWarning() << "processModelDrivenUpdate called, m_searchActive=" << m_searchActive;
    if (m_searchActive) {
        // Match set may have changed along with the data; recompute everything
        updateSearchMatches();
    } else {
        // With dynamicSortFilter the base class already keeps row membership and
        // ordering up to date per changed row, so only the scope bar needs a recount
        recalculateScopes();
    }
}

bool ProxyFilter::filterAcceptsRowWithoutScope(int source_row,
                                               const QModelIndex &source_parent) const
{
    if (!sourceModel()) return false;

    DocumentModel *docModel = qobject_cast<DocumentModel *>(sourceModel());
    if (docModel) {
        if (source_row < 0 || source_row >= docModel->documents().size()) return false;
        const DocumentInfo &doc = docModel->documents().at(source_row);

        bool isFolder = doc.isFolder;
        if (isFolder && (!m_filterString.isEmpty() || m_folderFilter.isEmpty())) {
            return false;
        }

        // 1. Offline filter
        bool isOffline = doc.isOffline;
        if (isOffline && !m_showUnavailable && m_categoryFilter != "Unavailable") {
            return false;
        }

        // 2. FTS5 Search filter
        if (m_searchActive) {
            int docId = doc.id;
            if (!m_matchedDocIds.contains(docId)) {
                return false;
            }
        }

        // 3. Min Rating filter
        int starRating = doc.starRating;
        if (starRating < m_minRating) {
            return false;
        }

        // 4. Tags intersection filter (AND)
        if (!m_selectedTags.isEmpty()) {
            const QStringList &rowTags = doc.tags;
            for (const QString &tag : m_selectedTags) {
                bool found = false;
                for (const QString &rt : rowTags) {
                    if (rt.compare(tag, Qt::CaseInsensitive) == 0) {
                        found = true;
                        break;
                    }
                }
                if (!found) return false;
            }
        }

        // 5. Duplicates filter
        if (m_duplicatesOnly) {
            const QString &fileHash = doc.fileHash;
            if (fileHash.isEmpty() || !m_duplicateHashes.contains(fileHash)) {
                return false;
            }
        }

        // 6. Category filters
        if (m_categoryFilter == "Recent") {
            qint64 lastOpened = doc.lastOpened;
            if (lastOpened <= 0) {
                return false;
            }
        } else if (m_categoryFilter == "Favorites") {
            if (starRating < 4) {
                return false;
            }
        } else if (m_categoryFilter == "Unavailable") {
            if (!isOffline) {
                return false;
            }
        }

        // 7. Folder filter
        if (!m_folderFilter.isEmpty()) {
            const QString &absolutePath = doc.absolutePath;

            if (m_includeSubfolderContents) {
                if (isFolder) {
                    return false;
                }
                if (!absolutePath.startsWith(m_folderFilter)) {
                    return false;
                }
            } else {
                QString parentDir = getParentDirectory(absolutePath);
                if (isFolder) {
                    if (!m_showSubfolderIcons) {
                        return false;
                    }
                    if (parentDir != m_folderFilter) {
                        return false;
                    }
                } else {
                    if (parentDir != m_folderFilter) {
                        return false;
                    }
                }
            }
        }

        return true;
    }

    // Fallback slow path for tests
    QModelIndex idx = sourceModel()->index(source_row, 0, source_parent);

    bool isFolder = sourceModel()->data(idx, DocumentModel::IsFolderRole).toBool();
    if (isFolder && (!m_filterString.isEmpty() || m_folderFilter.isEmpty())) {
        return false;
    }

    // 1. Offline filter
    bool isOffline = sourceModel()->data(idx, DocumentModel::IsOfflineRole).toBool();
    if (isOffline && !m_showUnavailable && m_categoryFilter != "Unavailable") {
        return false;
    }

    // 2. FTS5 Search filter
    if (m_searchActive) {
        int docId = sourceModel()->data(idx, DocumentModel::IdRole).toInt();
        if (!m_matchedDocIds.contains(docId)) {
            return false;
        }
    }

    // 3. Min Rating filter
    int starRating = sourceModel()->data(idx, DocumentModel::StarRatingRole).toInt();
    if (starRating < m_minRating) {
        return false;
    }

    // 4. Tags intersection filter (AND)
    if (!m_selectedTags.isEmpty()) {
        QStringList rowTags = sourceModel()->data(idx, DocumentModel::TagsRole).toStringList();
        for (const QString &tag : m_selectedTags) {
            bool found = false;
            for (const QString &rt : rowTags) {
                if (rt.compare(tag, Qt::CaseInsensitive) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) return false;
        }
    }

    // 5. Duplicates filter
    if (m_duplicatesOnly) {
        QString fileHash = sourceModel()->data(idx, DocumentModel::FileHashRole).toString();
        if (fileHash.isEmpty() || !m_duplicateHashes.contains(fileHash)) {
            return false;
        }
    }

    // 6. Category filters
    if (m_categoryFilter == "Recent") {
        qint64 lastOpened = sourceModel()->data(idx, DocumentModel::LastOpenedRole).toLongLong();
        if (lastOpened <= 0) {
            return false;
        }
    } else if (m_categoryFilter == "Favorites") {
        if (starRating < 4) {
            return false;
        }
    } else if (m_categoryFilter == "Unavailable") {
        if (!isOffline) {
            return false;
        }
    }

    // 7. Folder filter
    if (!m_folderFilter.isEmpty()) {
        QString absolutePath = sourceModel()->data(idx, DocumentModel::AbsolutePathRole).toString();

        if (m_includeSubfolderContents) {
            if (isFolder) {
                return false;
            }
            if (!absolutePath.startsWith(m_folderFilter)) {
                return false;
            }
        } else {
            QString parentDir = getParentDirectory(absolutePath);
            if (isFolder) {
                if (!m_showSubfolderIcons) {
                    return false;
                }
                if (parentDir != m_folderFilter) {
                    return false;
                }
            } else {
                if (parentDir != m_folderFilter) {
                    return false;
                }
            }
        }
    }

    return true;
}

bool ProxyFilter::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    if (!filterAcceptsRowWithoutScope(source_row, source_parent)) {
        return false;
    }

    if (m_scopeFilter.isEmpty() || m_scopeFilter == "All") {
        return true;
    }

    DocumentModel *docModel = qobject_cast<DocumentModel *>(sourceModel());
    if (docModel) {
        if (source_row < 0 || source_row >= docModel->documents().size()) return false;
        const DocumentInfo &doc = docModel->documents().at(source_row);

        QDateTime dateModified = doc.dateModified;
        bool isOffline = doc.isOffline;
        QString fileName = doc.fileName;

        if (m_scopeFilter == "Today") {
            return dateModified.date() == QDate::currentDate();
        } else if (m_scopeFilter == "This Week") {
            QDate today = QDate::currentDate();
            int days = dateModified.date().daysTo(today);
            return days >= 0 && days < 7;
        } else if (m_scopeFilter == "This Month") {
            QDate today = QDate::currentDate();
            return dateModified.date().year() == today.year() &&
                   dateModified.date().month() == today.month();
        } else if (m_scopeFilter == "Local") {
            return !isOffline;
        } else if (m_scopeFilter == "Unavailable") {
            return isOffline;
        } else {
            bool isYear = false;
            int yearVal = m_scopeFilter.toInt(&isYear);
            if (isYear && m_scopeFilter.length() == 4) {
                return dateModified.date().year() == yearVal;
            } else {
                QString ext = getFileSuffix(fileName);
                return ext == m_scopeFilter;
            }
        }
    }

    // Fallback slow path for tests
    QModelIndex idx = sourceModel()->index(source_row, 0, source_parent);
    QDateTime dateModified = sourceModel()->data(idx, DocumentModel::DateModifiedRole).toDateTime();
    bool isOffline = sourceModel()->data(idx, DocumentModel::IsOfflineRole).toBool();
    QString fileName = sourceModel()->data(idx, DocumentModel::FileNameRole).toString();

    if (m_scopeFilter == "Today") {
        return dateModified.date() == QDate::currentDate();
    } else if (m_scopeFilter == "This Week") {
        QDate today = QDate::currentDate();
        int days = dateModified.date().daysTo(today);
        return days >= 0 && days < 7;
    } else if (m_scopeFilter == "This Month") {
        QDate today = QDate::currentDate();
        return dateModified.date().year() == today.year() &&
               dateModified.date().month() == today.month();
    } else if (m_scopeFilter == "Local") {
        return !isOffline;
    } else if (m_scopeFilter == "Unavailable") {
        return isOffline;
    } else {
        bool isYear = false;
        int yearVal = m_scopeFilter.toInt(&isYear);
        if (isYear && m_scopeFilter.length() == 4) {
            return dateModified.date().year() == yearVal;
        } else {
            QString ext = getFileSuffix(fileName);
            return ext == m_scopeFilter;
        }
    }
}

void ProxyFilter::recalculateScopes()
{
    QAbstractItemModel *model = sourceModel();
    if (!model) {
        QStringList defaultScopes = {"All"};
        if (m_activeScopes != defaultScopes) {
            m_activeScopes = defaultScopes;
            emit activeScopesChanged();
        }
        return;
    }

    bool hasToday = false;
    bool hasThisWeek = false;
    bool hasThisMonth = false;
    bool hasLocal = false;
    bool hasUnavailable = false;
    QSet<int> yearsSet;
    QSet<QString> docTypesSet;

    QDate today = QDate::currentDate();
    int rows = model->rowCount();

    DocumentModel *docModel = qobject_cast<DocumentModel *>(model);
    if (docModel) {
        const QList<DocumentInfo> &docs = docModel->documents();
        for (int i = 0; i < rows; ++i) {
            if (filterAcceptsRowWithoutScope(i, QModelIndex())) {
                if (i >= 0 && i < docs.size()) {
                    const DocumentInfo &doc = docs.at(i);
                    QDateTime dateModified = doc.dateModified;
                    bool isOffline = doc.isOffline;
                    QString fileName = doc.fileName;

                    QDate docDate = dateModified.date();
                    if (docDate == today) {
                        hasToday = true;
                    }
                    int days = docDate.daysTo(today);
                    if (days >= 0 && days < 7) {
                        hasThisWeek = true;
                    }
                    if (docDate.year() == today.year() && docDate.month() == today.month()) {
                        hasThisMonth = true;
                    }
                    if (docDate.isValid()) {
                        yearsSet.insert(docDate.year());
                    }
                    if (isOffline) {
                        hasUnavailable = true;
                    } else {
                        hasLocal = true;
                    }
                    QString ext = getFileSuffix(fileName);
                    if (!ext.isEmpty()) {
                        docTypesSet.insert(ext);
                    }
                }
            }
        }
    } else {
        // Fallback for tests using MockDocumentModel
        for (int i = 0; i < rows; ++i) {
            if (filterAcceptsRowWithoutScope(i, QModelIndex())) {
                QModelIndex idx = model->index(i, 0);
                QDateTime dateModified =
                    model->data(idx, DocumentModel::DateModifiedRole).toDateTime();
                bool isOffline = model->data(idx, DocumentModel::IsOfflineRole).toBool();
                QString fileName = model->data(idx, DocumentModel::FileNameRole).toString();

                QDate docDate = dateModified.date();
                if (docDate == today) {
                    hasToday = true;
                }
                int days = docDate.daysTo(today);
                if (days >= 0 && days < 7) {
                    hasThisWeek = true;
                }
                if (docDate.year() == today.year() && docDate.month() == today.month()) {
                    hasThisMonth = true;
                }
                if (docDate.isValid()) {
                    yearsSet.insert(docDate.year());
                }
                if (isOffline) {
                    hasUnavailable = true;
                } else {
                    hasLocal = true;
                }
                QString ext = getFileSuffix(fileName);
                if (!ext.isEmpty()) {
                    docTypesSet.insert(ext);
                }
            }
        }
    }

    QStringList newScopes;
    newScopes.append("All");
    if (hasToday) {
        newScopes.append("Today");
    }
    if (hasThisWeek) {
        newScopes.append("This Week");
    }
    if (hasThisMonth) {
        newScopes.append("This Month");
    }

    QList<int> sortedYears = yearsSet.values();
    std::sort(sortedYears.begin(), sortedYears.end(), std::greater<int>());
    for (int y : sortedYears) {
        newScopes.append(QString::number(y));
    }

    if (hasLocal) {
        newScopes.append("Local");
    }
    if (hasUnavailable) {
        newScopes.append("Unavailable");
    }

    QList<QString> sortedDocTypes = docTypesSet.values();
    std::sort(sortedDocTypes.begin(), sortedDocTypes.end());
    for (const QString &ext : sortedDocTypes) {
        newScopes.append(ext);
    }

    if (m_activeScopes != newScopes) {
        m_activeScopes = newScopes;
        emit activeScopesChanged();
    }

    if (!m_activeScopes.contains(m_scopeFilter)) {
        m_scopeFilter = "All";
        emit scopeFilterChanged();
        invalidateFilter();
    }
}

void ProxyFilter::invalidateAndRecalculate()
{
    invalidateFilter();
    recalculateScopes();
}

void ProxyFilter::setFilters(const QString &category, const QString &folder,
                             const QStringList &tags, const QString &scope)
{
    bool changed = false;

    if (m_categoryFilter != category) {
        m_categoryFilter = category;
        bool dupOnly = (m_categoryFilter == "Duplicates");
        if (m_duplicatesOnly != dupOnly) {
            m_duplicatesOnly = dupOnly;
            if (m_duplicatesOnly) {
                updateDuplicateHashes();
            }
            emit duplicatesOnlyChanged();
        }

        if (m_categoryFilter == "Recent") {
            setSortRole(DocumentModel::LastOpenedRole);
            sort(0, Qt::DescendingOrder);
        } else {
            setSortRole(DocumentModel::FileNameRole);
            sort(0, Qt::AscendingOrder);
        }
        changed = true;
        emit categoryFilterChanged();
    }

    if (m_folderFilter != folder) {
        m_folderFilter = folder;
        changed = true;
        emit folderFilterChanged();
    }

    if (m_selectedTags != tags) {
        m_selectedTags = tags;
        changed = true;
        emit selectedTagsChanged();
    }

    if (m_scopeFilter != scope) {
        m_scopeFilter = scope;
        changed = true;
        emit scopeFilterChanged();
    }

    if (changed) {
        invalidateAndRecalculate();
    }
}

int ProxyFilter::rowOfDocId(int docId) const
{
    DocumentModel *docModel = qobject_cast<DocumentModel *>(sourceModel());
    if (docModel) {
        int rows = rowCount();
        const QList<DocumentInfo> &docs = docModel->documents();
        for (int i = 0; i < rows; ++i) {
            QModelIndex srcIdx = mapToSource(index(i, 0));
            int srcRow = srcIdx.row();
            if (srcRow >= 0 && srcRow < docs.size()) {
                if (docs.at(srcRow).id == docId) {
                    return i;
                }
            }
        }
    }
    return -1;
}

bool ProxyFilter::lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const
{
    DocumentModel *docModel = qobject_cast<DocumentModel *>(sourceModel());
    if (docModel) {
        const QList<DocumentInfo> &docs = docModel->documents();
        int leftRow = source_left.row();
        int rightRow = source_right.row();
        if (leftRow >= 0 && leftRow < docs.size() && rightRow >= 0 && rightRow < docs.size()) {
            const DocumentInfo &leftDoc = docs.at(leftRow);
            const DocumentInfo &rightDoc = docs.at(rightRow);

            bool leftFolder = leftDoc.isFolder;
            bool rightFolder = rightDoc.isFolder;

            if (leftFolder && !rightFolder) {
                return sortOrder() == Qt::AscendingOrder;
            }
            if (!leftFolder && rightFolder) {
                return sortOrder() == Qt::DescendingOrder;
            }

            int role = sortRole();
            switch (role) {
                case DocumentModel::FileNameRole:
                    return leftDoc.fileName.compare(rightDoc.fileName, Qt::CaseInsensitive) < 0;
                case DocumentModel::FileSizeRole:
                    return leftDoc.fileSize < rightDoc.fileSize;
                case DocumentModel::PageCountRole:
                    return leftDoc.pageCount < rightDoc.pageCount;
                case DocumentModel::StarRatingRole:
                    return leftDoc.starRating < rightDoc.starRating;
                case DocumentModel::DateModifiedRole:
                    return leftDoc.dateModified < rightDoc.dateModified;
                case DocumentModel::LastOpenedRole:
                    return leftDoc.lastOpened < rightDoc.lastOpened;
                default:
                    break;
            }
        }
    }

    // Fallback slow path for tests and other models
    bool leftFolder = sourceModel()->data(source_left, DocumentModel::IsFolderRole).toBool();
    bool rightFolder = sourceModel()->data(source_right, DocumentModel::IsFolderRole).toBool();

    if (leftFolder && !rightFolder) {
        return sortOrder() == Qt::AscendingOrder;
    }
    if (!leftFolder && rightFolder) {
        return sortOrder() == Qt::DescendingOrder;
    }

    return QSortFilterProxyModel::lessThan(source_left, source_right);
}

QVariant ProxyFilter::get(int row, const QString &roleName) const
{
    if (row < 0 || row >= rowCount()) return QVariant();

    if (m_roleNameToKey.isEmpty()) {
        QHash<int, QByteArray> roles = roleNames();
        for (auto it = roles.begin(); it != roles.end(); ++it) {
            m_roleNameToKey[QString::fromUtf8(it.value())] = it.key();
        }
    }

    int role = m_roleNameToKey.value(roleName, -1);
    if (role == -1) return QVariant();
    return data(index(row, 0), role);
}
