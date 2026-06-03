#include <QtQuickTest>
#include <QQmlEngine>
#include <QQmlContext>
#include <QObject>
#include <QStringList>
#include <QVariant>
#include <QAbstractListModel>

class MockController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList watchedFolders READ watchedFolders NOTIFY watchedFoldersChanged)
public:
    explicit MockController(QObject *parent = nullptr) : QObject(parent) {}
    QStringList watchedFolders() const { return QStringList(); }
    Q_INVOKABLE void requestThumbnail(int, const QString &, bool = false) {}
    Q_INVOKABLE void batchUpdateTags(const QVariantList &, const QStringList &) {}
    Q_INVOKABLE void updateNotes(int docId, const QString &notes) {
        emit notesUpdated(docId, notes);
    }
    Q_INVOKABLE void addWatchedFolder(const QString &) {}
    Q_INVOKABLE void removeWatchedFolder(const QString &) {}
    Q_INVOKABLE QStringList getUniqueTags() const { return QStringList(); }
    Q_INVOKABLE QVariantMap handleDroppedUrl(const QString &) { return QVariantMap(); }
    Q_INVOKABLE void batchUpdateRating(const QVariantList &, int) {}
    Q_INVOKABLE void batchRemoveTags(const QVariantList &, const QStringList &) {}
    Q_INVOKABLE void batchAddTags(const QVariantList &, const QStringList &) {}
signals:
    void watchedFoldersChanged();
    void notesUpdated(int docId, const QString &notes);
};

#include <QHash>
#include <QByteArray>

class MockDocumentModel : public QAbstractListModel
{
    Q_OBJECT
    QList<QVariantMap> m_rows;
public:
    explicit MockDocumentModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex &parent = QModelIndex()) const override {
        return m_rows.size();
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
            return QVariant();

        const auto &rowMap = m_rows.at(index.row());
        QString roleStr = QString::number(role);
        if (rowMap.contains(roleStr))
            return rowMap.value(roleStr);

        static QHash<int, QString> roleNameMap = {
            {257, "id"},
            {258, "folderId"},
            {259, "fileName"},
            {260, "absolutePath"},
            {261, "fileSize"},
            {262, "fileHash"},
            {263, "dateCreated"},
            {264, "dateModified"},
            {265, "dateAdded"},
            {266, "pageCount"},
            {267, "starRating"},
            {268, "isOffline"},
            {269, "tags"},
            {270, "textSnippet"},
            {271, "notes"},
            {272, "thumbnailPath"},
            {273, "fileSizeStr"},
            {274, "starRatingStr"},
            {275, "offlineColor"},
            {276, "dateModifiedStr"},
            {277, "tagsStr"}
        };
        QString name = roleNameMap.value(role);
        if (!name.isEmpty() && rowMap.contains(name)) {
            return rowMap.value(name);
        }
        return QVariant();
    }

    QHash<int, QByteArray> roleNames() const override {
        return {
            {257, "id"},
            {258, "folderId"},
            {259, "fileName"},
            {260, "absolutePath"},
            {261, "fileSize"},
            {262, "fileHash"},
            {263, "dateCreated"},
            {264, "dateModified"},
            {265, "dateAdded"},
            {266, "pageCount"},
            {267, "starRating"},
            {268, "isOffline"},
            {269, "tags"},
            {270, "textSnippet"},
            {271, "notes"},
            {272, "thumbnailPath"},
            {273, "fileSizeStr"},
            {274, "starRatingStr"},
            {275, "offlineColor"},
            {276, "dateModifiedStr"},
            {277, "tagsStr"}
        };
    }

    Q_INVOKABLE void clear() {
        beginResetModel();
        m_rows.clear();
        endResetModel();
    }

    Q_INVOKABLE void append(const QVariantMap &row) {
        beginInsertRows(QModelIndex(), m_rows.size(), m_rows.size());
        m_rows.append(row);
        endInsertRows();
    }

    Q_INVOKABLE void updateRow(int rowIdx, const QVariantMap &values) {
        if (rowIdx >= 0 && rowIdx < m_rows.size()) {
            for (auto it = values.begin(); it != values.end(); ++it) {
                m_rows[rowIdx].insert(it.key(), it.value());
            }
            emit dataChanged(index(rowIdx), index(rowIdx));
        }
    }
};

class MockProxyFilter : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString filterString READ filterString WRITE setFilterString NOTIFY filterStringChanged)
    Q_PROPERTY(QString folderFilter READ folderFilter WRITE setFolderFilter NOTIFY folderFilterChanged)
    Q_PROPERTY(QString categoryFilter READ categoryFilter WRITE setCategoryFilter NOTIFY categoryFilterChanged)
    Q_PROPERTY(QString scopeFilter READ scopeFilter WRITE setScopeFilter NOTIFY scopeFilterChanged)
    Q_PROPERTY(QStringList activeScopes READ activeScopes WRITE setActiveScopes NOTIFY activeScopesChanged)
public:
    explicit MockProxyFilter(QObject *parent = nullptr)
        : QAbstractListModel(parent)
        , m_scopeFilter("All")
        , m_activeScopes(QStringList{"All"})
    {}
    int rowCount(const QModelIndex &parent = QModelIndex()) const override { return 0; }
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override { return QVariant(); }

    QString filterString() const { return m_filterString; }
    void setFilterString(const QString &s) { if (m_filterString != s) { m_filterString = s; emit filterStringChanged(); } }
    QString folderFilter() const { return m_folderFilter; }
    void setFolderFilter(const QString &s) { if (m_folderFilter != s) { m_folderFilter = s; emit folderFilterChanged(); } }
    QString categoryFilter() const { return m_categoryFilter; }
    void setCategoryFilter(const QString &s) { if (m_categoryFilter != s) { m_categoryFilter = s; emit categoryFilterChanged(); } }
    QString scopeFilter() const { return m_scopeFilter; }
    void setScopeFilter(const QString &s) { if (m_scopeFilter != s) { m_scopeFilter = s; emit scopeFilterChanged(); } }
    QStringList activeScopes() const { return m_activeScopes; }
    void setActiveScopes(const QStringList &l) { if (m_activeScopes != l) { m_activeScopes = l; emit activeScopesChanged(); } }
    Q_INVOKABLE void setSortRole(int) {}
    Q_INVOKABLE void sort(int, int) {}
signals:
    void filterStringChanged();
    void folderFilterChanged();
    void categoryFilterChanged();
    void scopeFilterChanged();
    void activeScopesChanged();
private:
    QString m_filterString;
    QString m_folderFilter;
    QString m_categoryFilter;
    QString m_scopeFilter;
    QStringList m_activeScopes;
};

class Setup : public QObject
{
    Q_OBJECT
public slots:
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        engine->addImportPath("qrc:/qt/qml");
        engine->addImportPath("qrc:/");
        
        // Register mock context properties
        MockController *mockCtrl = new MockController(engine);
        MockDocumentModel *mockModel = new MockDocumentModel(engine);
        MockProxyFilter *mockFilter = new MockProxyFilter(engine);
        
        engine->rootContext()->setContextProperty("libraryController", mockCtrl);
        engine->rootContext()->setContextProperty("documentModel", mockModel);
        engine->rootContext()->setContextProperty("proxyFilter", mockFilter);
    }
};

QUICK_TEST_MAIN_WITH_SETUP(test_qml_ui, Setup)

#include "TestRunner.moc"

