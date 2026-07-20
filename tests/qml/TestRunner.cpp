#include <QAbstractListModel>
#include <QObject>
#include <QQmlContext>
#include <QQmlEngine>
#include <QStringList>
#include <QVariant>
#include <QtQuickTest>
#include "../../src/utils/DocUtils.h"

class MockController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList watchedFolders READ watchedFolders NOTIFY watchedFoldersChanged)
    Q_PROPERTY(bool isScanning READ isScanning WRITE setIsScanning NOTIFY isScanningChanged)
    Q_PROPERTY(
        double scanProgress READ scanProgress WRITE setScanProgress NOTIFY scanProgressChanged)
    Q_PROPERTY(QString scanStatusText READ scanStatusText WRITE setScanStatusText NOTIFY
                   scanStatusTextChanged)
    Q_PROPERTY(QStringList mockTags READ mockTags WRITE setMockTags NOTIFY mockTagsChanged)
public:
    explicit MockController(QObject *parent = nullptr) : QObject(parent), m_mockTags({"work", "2026"}) {}
    QStringList watchedFolders() const { return QStringList(); }
    bool isScanning() const { return m_isScanning; }
    void setIsScanning(bool s)
    {
        if (m_isScanning != s) {
            m_isScanning = s;
            emit isScanningChanged();
            updateMockStatusText();
        }
    }
    double scanProgress() const { return m_scanProgress; }
    void setScanProgress(double p)
    {
        if (m_scanProgress != p) {
            m_scanProgress = p;
            emit scanProgressChanged();
            updateMockStatusText();
        }
    }
    QString scanStatusText() const { return m_scanStatusText; }
    void setScanStatusText(const QString &t)
    {
        if (m_scanStatusText != t) {
            m_scanStatusText = t;
            emit scanStatusTextChanged();
        }
    }
    void updateMockStatusText()
    {
        QString text;
        if (m_isScanning) {
            text = QString("Scanning: %1%").arg(qRound(m_scanProgress * 100));
        } else {
            text = "";
        }
        if (m_scanStatusText != text) {
            m_scanStatusText = text;
            emit scanStatusTextChanged();
        }
    }
    Q_INVOKABLE void requestThumbnail(int, const QString &, bool = false) {}
    Q_INVOKABLE void batchUpdateTags(const QVariantList &, const QStringList &) {}
    Q_INVOKABLE void updateNotes(int docId, const QString &notes)
    {
        emit notesUpdated(docId, notes);
    }
    Q_INVOKABLE void addWatchedFolder(const QString &path) { emit folderAdded(path); }
    Q_INVOKABLE void removeWatchedFolder(const QString &) {}
    Q_INVOKABLE QStringList getUniqueTags() const { return m_mockTags; }
    QStringList mockTags() const { return m_mockTags; }
    void setMockTags(const QStringList &tags)
    {
        if (m_mockTags != tags) {
            m_mockTags = tags;
            emit mockTagsChanged();
        }
    }
    Q_INVOKABLE QVariantMap handleDroppedUrl(const QString &) { return QVariantMap(); }
    Q_INVOKABLE void batchUpdateRating(const QVariantList &, int) { emit libraryChanged(); }
    Q_INVOKABLE void batchRemoveTags(const QVariantList &, const QStringList &) {}
    Q_INVOKABLE void batchAddTags(const QVariantList &, const QStringList &) {}
    Q_INVOKABLE void markDocumentOpened(int docId) { emit documentOpened(docId); }
    Q_INVOKABLE void copyToClipboard(const QString &text) { emit pathCopied(text); }
    Q_INVOKABLE QVariantList searchDocumentContent(int, const QString &, const QString &) { return QVariantList(); }
    Q_INVOKABLE QVariantList searchDocuments(const QString &) { return QVariantList(); }
    Q_INVOKABLE QString readTextFile(const QString &) { return "Mock Text Content"; }
    Q_INVOKABLE QString fileTypeDescription(const QString &fileName) const
    {
        return DocUtils::fileTypeDescription(fileName);
    }
signals:
    void libraryChanged();
    void watchedFoldersChanged();
    void folderAdded(const QString &folderPath);
    void notesUpdated(int docId, const QString &notes);
    void documentOpened(int docId);
    void pathCopied(const QString &text);
    void isScanningChanged();
    void scanProgressChanged();
    void scanStatusTextChanged();
    void scanRequested(const QString &folderPath);

signals:
    void mockTagsChanged();

private:
    bool m_isScanning = false;
    double m_scanProgress = 0.0;
    QString m_scanStatusText;
    QStringList m_mockTags;
};

#include <QByteArray>
#include <QHash>

class MockDocumentModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int totalCount READ totalCount NOTIFY countsChanged)
    Q_PROPERTY(int pdfCount READ pdfCount NOTIFY countsChanged)
    Q_PROPERTY(int imageCount READ imageCount NOTIFY countsChanged)
    Q_PROPERTY(int textCount READ textCount NOTIFY countsChanged)
    Q_PROPERTY(int localCount READ localCount NOTIFY countsChanged)
    Q_PROPERTY(int unavailableCount READ unavailableCount NOTIFY countsChanged)
    QList<QVariantMap> m_rows;

public:
    explicit MockDocumentModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex & = QModelIndex()) const override { return m_rows.size(); }

    int totalCount() const { return m_rows.size(); }

    int pdfCount() const
    {
        int count = 0;
        for (const auto &row : m_rows) {
            QString name = row.value("fileName").toString();
            if (name.endsWith(".pdf", Qt::CaseInsensitive)) {
                count++;
            }
        }
        return count;
    }

    int imageCount() const
    {
        int count = 0;
        for (const auto &row : m_rows) {
            QString name = row.value("fileName").toString();
            QString ext = name.split('.').last().toLower();
            if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "gif" || ext == "bmp" ||
                ext == "tiff") {
                count++;
            }
        }
        return count;
    }

    int textCount() const
    {
        int count = 0;
        for (const auto &row : m_rows) {
            QString name = row.value("fileName").toString();
            if (name.isEmpty()) continue;
            QString ext = name.split('.').last().toLower();
            if (ext != "pdf" && ext != "png" && ext != "jpg" && ext != "jpeg" && ext != "gif" &&
                ext != "bmp" && ext != "tiff") {
                count++;
            }
        }
        return count;
    }

    int localCount() const
    {
        int count = 0;
        for (const auto &row : m_rows) {
            bool isOffline = row.value("isOffline").toBool() || row.value("268").toBool();
            if (!isOffline) {
                count++;
            }
        }
        return count;
    }

    int unavailableCount() const
    {
        int count = 0;
        for (const auto &row : m_rows) {
            bool isOffline = row.value("isOffline").toBool() || row.value("268").toBool();
            if (isOffline) {
                count++;
            }
        }
        return count;
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) return QVariant();

        const auto &rowMap = m_rows.at(index.row());
        QString roleStr = QString::number(role);
        if (rowMap.contains(roleStr)) return rowMap.value(roleStr);

        static QHash<int, QString> roleNameMap = {{257, "id"},           {258, "folderId"},
                                                  {259, "fileName"},     {260, "absolutePath"},
                                                  {261, "fileSize"},     {262, "fileHash"},
                                                  {263, "dateCreated"},  {264, "dateModified"},
                                                  {265, "dateAdded"},    {266, "pageCount"},
                                                  {267, "starRating"},   {268, "isOffline"},
                                                  {269, "tags"},         {270, "textSnippet"},
                                                  {271, "notes"},        {272, "thumbnailPath"},
                                                  {273, "fileSizeStr"},  {274, "starRatingStr"},
                                                  {275, "offlineColor"}, {276, "dateModifiedStr"},
                                                  {277, "tagsStr"},      {278, "lastOpened"}};
        QString name = roleNameMap.value(role);
        if (!name.isEmpty() && rowMap.contains(name)) {
            return rowMap.value(name);
        }
        if (role == 261 || role == 267 || role == 266 || role == 278 || role == 257 ||
            role == 258) {
            return 0;
        }
        if (role == 268) {
            return false;
        }
        if (role == 269) {
            return QStringList();
        }
        if (role == 259 || role == 260 || role == 270 || role == 271 || role == 272 ||
            role == 273 || role == 274 || role == 275 || role == 276 || role == 277) {
            return QString("");
        }
        return QVariant();
    }

    QHash<int, QByteArray> roleNames() const override
    {
        return {{257, "id"},           {258, "folderId"},
                {259, "fileName"},     {260, "absolutePath"},
                {261, "fileSize"},     {262, "fileHash"},
                {263, "dateCreated"},  {264, "dateModified"},
                {265, "dateAdded"},    {266, "pageCount"},
                {267, "starRating"},   {268, "isOffline"},
                {269, "tags"},         {270, "textSnippet"},
                {271, "notes"},        {272, "thumbnailPath"},
                {273, "fileSizeStr"},  {274, "starRatingStr"},
                {275, "offlineColor"}, {276, "dateModifiedStr"},
                {277, "tagsStr"},      {278, "lastOpened"}};
    }

    Q_INVOKABLE void clear()
    {
        beginResetModel();
        m_rows.clear();
        endResetModel();
        emit countsChanged();
    }

    Q_INVOKABLE void append(const QVariantMap &row)
    {
        beginInsertRows(QModelIndex(), m_rows.size(), m_rows.size());
        m_rows.append(row);
        endInsertRows();
        emit countsChanged();
    }

    Q_INVOKABLE void updateRow(int rowIdx, const QVariantMap &values)
    {
        if (rowIdx >= 0 && rowIdx < m_rows.size()) {
            for (auto it = values.begin(); it != values.end(); ++it) {
                m_rows[rowIdx].insert(it.key(), it.value());
            }
            emit dataChanged(index(rowIdx), index(rowIdx));
            emit countsChanged();
        }
    }

    Q_INVOKABLE QVariantMap getDocument(int docId) const
    {
        for (const auto &row : m_rows) {
            bool idMatches = (row.value("id").toInt() == docId || row.value("docId").toInt() == docId);
            if (idMatches) {
                QVariantMap res = row;
                if (!res.contains("docId") && res.contains("id")) {
                    res["docId"] = res["id"];
                }
                if (!res.contains("fileSizeStr")) {
                    qint64 size = res.value("fileSize").toLongLong();
                    qint64 kb = size / 1024;
                    if (kb > 1024) {
                        res["fileSizeStr"] = QString("%1 MB").arg(double(kb) / 1024.0, 0, 'f', 1);
                    } else {
                        res["fileSizeStr"] = QString("%1 KB").arg(kb);
                    }
                }
                return res;
            }
        }
        return QVariantMap();
    }

    Q_INVOKABLE int findDocIdByPath(const QString &path) const
    {
        for (const auto &row : m_rows) {
            if (row.value("absolutePath").toString() == path) {
                return row.value("id").toInt();
            }
        }
        return -1;
    }

signals:
    void countsChanged();
};

class MockProxyFilter : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(
        QString filterString READ filterString WRITE setFilterString NOTIFY filterStringChanged)
    Q_PROPERTY(
        QStringList selectedTags READ selectedTags WRITE setSelectedTags NOTIFY selectedTagsChanged)
    Q_PROPERTY(
        QString folderFilter READ folderFilter WRITE setFolderFilter NOTIFY folderFilterChanged)
    Q_PROPERTY(QString categoryFilter READ categoryFilter WRITE setCategoryFilter NOTIFY
                   categoryFilterChanged)
    Q_PROPERTY(QString scopeFilter READ scopeFilter WRITE setScopeFilter NOTIFY scopeFilterChanged)
    Q_PROPERTY(
        QStringList activeScopes READ activeScopes WRITE setActiveScopes NOTIFY activeScopesChanged)
    Q_PROPERTY(bool includeSubfolderContents READ includeSubfolderContents WRITE
                   setIncludeSubfolderContents NOTIFY includeSubfolderContentsChanged)
    Q_PROPERTY(bool showSubfolderIcons READ showSubfolderIcons WRITE setShowSubfolderIcons NOTIFY
                   showSubfolderIconsChanged)
    QList<QVariantMap> m_rows;

public:
    explicit MockProxyFilter(QObject *parent = nullptr)
        : QAbstractListModel(parent),
          m_scopeFilter("All"),
          m_activeScopes(QStringList{"All"}),
          m_includeSubfolderContents(false),
          m_showSubfolderIcons(true)
    {
    }

    int rowCount(const QModelIndex & = QModelIndex()) const override { return m_rows.size(); }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) return QVariant();

        const auto &rowMap = m_rows.at(index.row());
        QString roleStr = QString::number(role);
        if (rowMap.contains(roleStr)) return rowMap.value(roleStr);

        static QHash<int, QString> roleNameMap = {
            {257, "docId"},         {258, "folderId"},        {259, "fileName"},
            {260, "absolutePath"},  {261, "fileSize"},        {262, "fileHash"},
            {263, "dateCreated"},   {264, "dateModified"},    {265, "dateAdded"},
            {266, "pageCount"},     {267, "starRating"},      {268, "isOffline"},
            {269, "tags"},          {270, "textSnippet"},     {271, "notes"},
            {272, "thumbnailPath"}, {273, "fileSizeStr"},     {274, "starRatingStr"},
            {275, "offlineColor"},  {276, "dateModifiedStr"}, {277, "tagsStr"},
            {278, "lastOpened"}};
        QString name = roleNameMap.value(role);
        if (!name.isEmpty() && rowMap.contains(name)) {
            return rowMap.value(name);
        }
        if (role == 261 || role == 267 || role == 266 || role == 278 || role == 257 ||
            role == 258) {
            return 0;
        }
        if (role == 268) {
            return false;
        }
        if (role == 269) {
            return QStringList();
        }
        if (role == 259 || role == 260 || role == 270 || role == 271 || role == 272 ||
            role == 273 || role == 274 || role == 275 || role == 276 || role == 277) {
            return QString("");
        }
        return QVariant();
    }

    QHash<int, QByteArray> roleNames() const override
    {
        return {{257, "docId"},         {258, "folderId"},        {259, "fileName"},
                {260, "absolutePath"},  {261, "fileSize"},        {262, "fileHash"},
                {263, "dateCreated"},   {264, "dateModified"},    {265, "dateAdded"},
                {266, "pageCount"},     {267, "starRating"},      {268, "isOffline"},
                {269, "tags"},          {270, "textSnippet"},     {271, "notes"},
                {272, "thumbnailPath"}, {273, "fileSizeStr"},     {274, "starRatingStr"},
                {275, "offlineColor"},  {276, "dateModifiedStr"}, {277, "tagsStr"},
                {278, "lastOpened"}};
    }

    Q_INVOKABLE void clear()
    {
        beginResetModel();
        m_rows.clear();
        endResetModel();
    }

    Q_INVOKABLE void append(const QVariantMap &row)
    {
        beginInsertRows(QModelIndex(), m_rows.size(), m_rows.size());
        m_rows.append(row);
        endInsertRows();
    }

    Q_INVOKABLE QVariant get(int row, const QString &roleName) const
    {
        if (row < 0 || row >= m_rows.size()) return QVariant();
        const auto &rowMap = m_rows.at(row);
        if (rowMap.contains(roleName)) return rowMap.value(roleName);
        if (roleName == "docId" && rowMap.contains("id")) return rowMap.value("id");
        return QVariant();
    }

    Q_INVOKABLE void setFilters(const QString &category, const QString &folder, const QStringList &tags, const QString &scope)
    {
        setCategoryFilter(category);
        setFolderFilter(folder);
        setSelectedTags(tags);
        setScopeFilter(scope);
    }

    QString filterString() const { return m_filterString; }
    void setFilterString(const QString &s)
    {
        if (m_filterString != s) {
            m_filterString = s;
            emit filterStringChanged();
        }
    }
    QStringList selectedTags() const { return m_selectedTags; }
    void setSelectedTags(const QStringList &tags)
    {
        if (m_selectedTags != tags) {
            m_selectedTags = tags;
            emit selectedTagsChanged();
        }
    }
    QString folderFilter() const { return m_folderFilter; }
    void setFolderFilter(const QString &s)
    {
        if (m_folderFilter != s) {
            m_folderFilter = s;
            emit folderFilterChanged();
        }
    }
    QString categoryFilter() const { return m_categoryFilter; }
    void setCategoryFilter(const QString &s)
    {
        if (m_categoryFilter != s) {
            m_categoryFilter = s;
            emit categoryFilterChanged();
        }
    }
    QString scopeFilter() const { return m_scopeFilter; }
    void setScopeFilter(const QString &s)
    {
        if (m_scopeFilter != s) {
            m_scopeFilter = s;
            emit scopeFilterChanged();
        }
    }
    QStringList activeScopes() const { return m_activeScopes; }
    void setActiveScopes(const QStringList &l)
    {
        if (m_activeScopes != l) {
            m_activeScopes = l;
            emit activeScopesChanged();
        }
    }
    bool includeSubfolderContents() const { return m_includeSubfolderContents; }
    void setIncludeSubfolderContents(bool enable)
    {
        if (m_includeSubfolderContents != enable) {
            m_includeSubfolderContents = enable;
            emit includeSubfolderContentsChanged();
        }
    }
    bool showSubfolderIcons() const { return m_showSubfolderIcons; }
    void setShowSubfolderIcons(bool enable)
    {
        if (m_showSubfolderIcons != enable) {
            m_showSubfolderIcons = enable;
            emit showSubfolderIconsChanged();
        }
    }
    Q_INVOKABLE void setSortRole(int) {}
    Q_INVOKABLE void sort(int, Qt::SortOrder = Qt::AscendingOrder) override {}
    Q_INVOKABLE int rowOfDocId(int docId) const
    {
        for (int i = 0; i < m_rows.size(); ++i) {
            const auto &row = m_rows.at(i);
            if (row.value("id").toInt() == docId || row.value("docId").toInt() == docId) {
                return i;
            }
        }
        return -1;
    }
signals:
    void filterStringChanged();
    void selectedTagsChanged();
    void folderFilterChanged();
    void categoryFilterChanged();
    void scopeFilterChanged();
    void activeScopesChanged();
    void includeSubfolderContentsChanged();
    void showSubfolderIconsChanged();

private:
    QString m_filterString;
    QStringList m_selectedTags;
    QString m_folderFilter;
    QString m_categoryFilter;
    QString m_scopeFilter;
    QStringList m_activeScopes;
    bool m_includeSubfolderContents;
    bool m_showSubfolderIcons;
};

class Setup : public QObject
{
    Q_OBJECT
public slots:
    void qmlEngineAvailable(QQmlEngine *engine)
    {
#ifdef VERSION_STRING
        QCoreApplication::setApplicationVersion(VERSION_STRING);
#endif
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
