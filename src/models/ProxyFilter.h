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

#ifndef PROXYFILTER_H
#define PROXYFILTER_H

#include <QSet>
#include <QSortFilterProxyModel>
#include <QStringList>

#include "../database/DatabaseManager.h"

class ProxyFilter : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(
        QString filterString READ filterString WRITE setFilterString NOTIFY filterStringChanged)
    Q_PROPERTY(
        QStringList selectedTags READ selectedTags WRITE setSelectedTags NOTIFY selectedTagsChanged)
    Q_PROPERTY(int minRating READ minRating WRITE setMinRating NOTIFY minRatingChanged)
    Q_PROPERTY(bool showUnavailable READ showUnavailable WRITE setShowUnavailable NOTIFY
                   showUnavailableChanged)
    Q_PROPERTY(bool duplicatesOnly READ duplicatesOnly WRITE setDuplicatesOnly NOTIFY
                   duplicatesOnlyChanged)
    Q_PROPERTY(QString categoryFilter READ categoryFilter WRITE setCategoryFilter NOTIFY
                   categoryFilterChanged)
    Q_PROPERTY(
        QString folderFilter READ folderFilter WRITE setFolderFilter NOTIFY folderFilterChanged)
    Q_PROPERTY(QString scopeFilter READ scopeFilter WRITE setScopeFilter NOTIFY scopeFilterChanged)
    Q_PROPERTY(QStringList activeScopes READ activeScopes NOTIFY activeScopesChanged)
    Q_PROPERTY(bool includeSubfolderContents READ includeSubfolderContents WRITE
                   setIncludeSubfolderContents NOTIFY includeSubfolderContentsChanged)
    Q_PROPERTY(bool showSubfolderIcons READ showSubfolderIcons WRITE setShowSubfolderIcons NOTIFY
                   showSubfolderIconsChanged)

public:
    explicit ProxyFilter(DatabaseManager *dbMgr, QObject *parent = nullptr);
    ~ProxyFilter();

    QString filterString() const { return m_filterString; }
    QStringList selectedTags() const { return m_selectedTags; }
    int minRating() const { return m_minRating; }
    bool showUnavailable() const { return m_showUnavailable; }
    bool duplicatesOnly() const { return m_duplicatesOnly; }
    QString categoryFilter() const { return m_categoryFilter; }
    QString folderFilter() const { return m_folderFilter; }
    QString scopeFilter() const { return m_scopeFilter; }
    QStringList activeScopes() const { return m_activeScopes; }
    bool includeSubfolderContents() const { return m_includeSubfolderContents; }
    bool showSubfolderIcons() const { return m_showSubfolderIcons; }

    Q_INVOKABLE QVariant get(int row, const QString &roleName) const;
    Q_INVOKABLE void setSortRole(int role);
    void setSourceModel(QAbstractItemModel *sourceModel) override;

public slots:
    void setFilterString(const QString &filter);
    void setSelectedTags(const QStringList &tags);
    void setMinRating(int rating);
    void setShowUnavailable(bool show);
    void setDuplicatesOnly(bool only);
    void setCategoryFilter(const QString &category);
    void setFolderFilter(const QString &folder);  // "All", "Recent", "Favorites" etc.
    void setScopeFilter(const QString &scope);
    void setIncludeSubfolderContents(bool enable);
    void setShowSubfolderIcons(bool enable);
    void recalculateScopes();
    bool filterAcceptsRowWithoutScope(int source_row, const QModelIndex &source_parent) const;

    void updateDuplicateHashes();
    void updateSearchMatches();

signals:
    void filterStringChanged();
    void selectedTagsChanged();
    void minRatingChanged();
    void showUnavailableChanged();
    void duplicatesOnlyChanged();
    void categoryFilterChanged();
    void folderFilterChanged();
    void scopeFilterChanged();
    void activeScopesChanged();
    void includeSubfolderContentsChanged();
    void showSubfolderIconsChanged();

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;
    bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const override;

private:
    DatabaseManager *m_dbMgr;
    QString m_filterString;
    QStringList m_selectedTags;
    int m_minRating;
    bool m_showUnavailable;
    bool m_duplicatesOnly;
    QString m_categoryFilter;
    QString m_folderFilter;
    QString m_scopeFilter;
    QStringList m_activeScopes;
    bool m_includeSubfolderContents;
    bool m_showSubfolderIcons;

    QSet<int> m_matchedDocIds;
    QSet<QString> m_duplicateHashes;
    bool m_searchActive;

    void invalidateAndRecalculate();
};

#endif  // PROXYFILTER_H
