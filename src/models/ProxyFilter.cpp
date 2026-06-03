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
#include "DocumentModel.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QDebug>
#include <QFileInfo>
#include <QDate>
#include <algorithm>

ProxyFilter::ProxyFilter(DatabaseManager *dbMgr, QObject *parent)
    : QSortFilterProxyModel(parent)
    , m_dbMgr(dbMgr)
    , m_minRating(0)
    , m_showOffline(true)
    , m_duplicatesOnly(false)
    , m_categoryFilter("All")
    , m_scopeFilter("All")
    , m_activeScopes(QStringList{"All"})
    , m_searchActive(false)
{
    setDynamicSortFilter(true);
    // Sort by file name ascending by default
    setSortRole(DocumentModel::FileNameRole);
    sort(0, Qt::AscendingOrder);
}

void ProxyFilter::setSortRole(int role)
{
    QSortFilterProxyModel::setSortRole(role);
}

ProxyFilter::~ProxyFilter()
{
}

void ProxyFilter::setSourceModel(QAbstractItemModel *sourceModel)
{
    if (this->sourceModel()) {
        disconnect(this->sourceModel(), &QAbstractItemModel::modelReset, this, &ProxyFilter::updateSearchMatches);
        disconnect(this->sourceModel(), &QAbstractItemModel::rowsInserted, this, &ProxyFilter::updateSearchMatches);
        disconnect(this->sourceModel(), &QAbstractItemModel::rowsRemoved, this, &ProxyFilter::updateSearchMatches);
        disconnect(this->sourceModel(), &QAbstractItemModel::dataChanged, this, &ProxyFilter::updateSearchMatches);
    }
    QSortFilterProxyModel::setSourceModel(sourceModel);
    if (sourceModel) {
        connect(sourceModel, &QAbstractItemModel::modelReset, this, &ProxyFilter::updateSearchMatches);
        connect(sourceModel, &QAbstractItemModel::rowsInserted, this, &ProxyFilter::updateSearchMatches);
        connect(sourceModel, &QAbstractItemModel::rowsRemoved, this, &ProxyFilter::updateSearchMatches);
        connect(sourceModel, &QAbstractItemModel::dataChanged, this, &ProxyFilter::updateSearchMatches);
    }
    updateSearchMatches();
}

void ProxyFilter::setFilterString(const QString &filter)
{
    if (m_filterString == filter) return;
    m_filterString = filter;
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

void ProxyFilter::setShowOffline(bool show)
{
    if (m_showOffline == show) return;
    m_showOffline = show;
    invalidateAndRecalculate();
    emit showOfflineChanged();
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

    QSqlQuery query("SELECT file_hash FROM documents WHERE file_hash IS NOT NULL AND file_hash != '' GROUP BY file_hash HAVING count(*) > 1;", db);
    while (query.next()) {
        m_duplicateHashes.insert(query.value(0).toString());
    }
}

void ProxyFilter::updateSearchMatches()
{
    m_matchedDocIds.clear();
    m_searchActive = !m_filterString.trimmed().isEmpty();
    if (!m_searchActive) {
        invalidateAndRecalculate();
        return;
    }

    QAbstractItemModel *model = sourceModel();
    if (!model) {
        invalidateAndRecalculate();
        return;
    }

    QStringList terms = m_filterString.split(" ", Qt::SkipEmptyParts);
    if (terms.isEmpty()) {
        m_searchActive = false;
        invalidateAndRecalculate();
        return;
    }

    int rows = model->rowCount();
    for (int i = 0; i < rows; ++i) {
        QModelIndex idx = model->index(i, 0);
        int docId = model->data(idx, DocumentModel::IdRole).toInt();
        QString fileName = model->data(idx, DocumentModel::FileNameRole).toString();
        QString textSnippet = model->data(idx, DocumentModel::TextSnippetRole).toString();
        QString notes = model->data(idx, DocumentModel::NotesRole).toString();
        QStringList tags = model->data(idx, DocumentModel::TagsRole).toStringList();

        bool allTermsMatch = true;
        for (const QString &term : terms) {
            bool termMatches = false;

            if (fileName.contains(term, Qt::CaseInsensitive)) {
                termMatches = true;
            } else if (textSnippet.contains(term, Qt::CaseInsensitive)) {
                termMatches = true;
            } else if (notes.contains(term, Qt::CaseInsensitive)) {
                termMatches = true;
            } else {
                for (const QString &tag : tags) {
                    if (tag.contains(term, Qt::CaseInsensitive)) {
                        termMatches = true;
                        break;
                    }
                }
            }

            if (!termMatches) {
                allTermsMatch = false;
                break;
            }
        }

        if (allTermsMatch) {
            m_matchedDocIds.insert(docId);
        }
    }

    invalidateAndRecalculate();
}

bool ProxyFilter::filterAcceptsRowWithoutScope(int source_row, const QModelIndex &source_parent) const
{
    if (!sourceModel()) return false;

    QModelIndex idx = sourceModel()->index(source_row, 0, source_parent);
    
    // 1. Offline filter
    bool isOffline = sourceModel()->data(idx, DocumentModel::IsOfflineRole).toBool();
    if (isOffline && !m_showOffline && m_categoryFilter != "Offline") {
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
        QDateTime dateAdded = sourceModel()->data(idx, DocumentModel::DateAddedRole).toDateTime();
        if (dateAdded.daysTo(QDateTime::currentDateTime()) > 7) {
            return false;
        }
    } else if (m_categoryFilter == "Favorites") {
        if (starRating < 4) {
            return false;
        }
    } else if (m_categoryFilter == "Offline") {
        if (!isOffline) {
            return false;
        }
    }

    // 7. Folder filter
    if (!m_folderFilter.isEmpty()) {
        QString absolutePath = sourceModel()->data(idx, DocumentModel::AbsolutePathRole).toString();
        if (!absolutePath.startsWith(m_folderFilter)) {
            return false;
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
        return dateModified.date().year() == today.year() && dateModified.date().month() == today.month();
    } else if (m_scopeFilter == "Online") {
        return !isOffline;
    } else if (m_scopeFilter == "Offline") {
        return isOffline;
    } else {
        bool isYear = false;
        int yearVal = m_scopeFilter.toInt(&isYear);
        if (isYear && m_scopeFilter.length() == 4) {
            return dateModified.date().year() == yearVal;
        } else {
            QString ext = QFileInfo(fileName).suffix().toUpper();
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
    bool hasOnline = false;
    bool hasOffline = false;
    QSet<int> yearsSet;
    QSet<QString> docTypesSet;

    QDate today = QDate::currentDate();
    int rows = model->rowCount();

    for (int i = 0; i < rows; ++i) {
        if (filterAcceptsRowWithoutScope(i, QModelIndex())) {
            QModelIndex idx = model->index(i, 0);
            QDateTime dateModified = model->data(idx, DocumentModel::DateModifiedRole).toDateTime();
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
                hasOffline = true;
            } else {
                hasOnline = true;
            }
            QString ext = QFileInfo(fileName).suffix().toUpper();
            if (!ext.isEmpty()) {
                docTypesSet.insert(ext);
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

    if (hasOnline) {
        newScopes.append("Online");
    }
    if (hasOffline) {
        newScopes.append("Offline");
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

bool ProxyFilter::lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const
{
    return QSortFilterProxyModel::lessThan(source_left, source_right);
}

QVariant ProxyFilter::get(int row, const QString &roleName) const
{
    if (row < 0 || row >= rowCount()) return QVariant();

    QHash<int, QByteArray> roles = roleNames();
    int role = -1;
    for (auto it = roles.begin(); it != roles.end(); ++it) {
        if (it.value() == roleName) {
            role = it.key();
            break;
        }
    }

    if (role == -1) return QVariant();
    return data(index(row, 0), role);
}
