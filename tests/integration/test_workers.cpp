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
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "../../src/controllers/LibraryController.h"
#include "../../src/database/DatabaseManager.h"
#include "../../src/utils/MacBookmarks.h"
#include "../../src/workers/OcrTask.h"
#include "../../src/workers/ScannerTask.h"
#include "../../src/workers/ThumbnailTask.h"

class TestWorkers : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();
    void testIngestionAndOfflineDetection();
    void testPdfAndOcr();
    void testLibraryControllerAPIs();
    void testTextAndDocIngestion();
    void testMacBookmarksAndEdgeCases();
    void testOcrNonsenseRejection();
    void testPackageDirectorySkipping();
    void testImageThumbnailAndOcrWithExif();
    void testSubdirectoryDeletionDetection();
    void testCooperativeCancellation();

private:
    DatabaseManager *m_dbMgr;
    LibraryController *m_controller;
};

void TestWorkers::initTestCase()
{
    // Use memory DB with shared cache so background thread and main thread access same DB
    m_dbMgr = new DatabaseManager(":memory:", this);
    QVERIFY(m_dbMgr->initializeDatabase());
    m_controller = new LibraryController(m_dbMgr, this);
}

void TestWorkers::cleanupTestCase()
{
    delete m_controller;
    delete m_dbMgr;
}

void TestWorkers::testIngestionAndOfflineDetection()
{
    // Create temporary directory
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString tempPath = QFileInfo(tempDir.path()).canonicalFilePath();
    QVERIFY(!tempPath.isEmpty());

    // Create a supported file (PNG)
    QString filePath = QDir(tempPath).filePath("documentA.png");
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));

    // Write standard tiny 1x1 PNG data to make it a valid image
    static const unsigned char pngData[] = {
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48,
        0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x06, 0x00, 0x00,
        0x00, 0x1f, 0x15, 0xc4, 0x89, 0x00, 0x00, 0x00, 0x0a, 0x49, 0x44, 0x41, 0x54, 0x78,
        0x9c, 0x63, 0x00, 0x01, 0x00, 0x00, 0x05, 0x00, 0x01, 0x0d, 0x0a, 0x2d, 0xb4, 0x00,
        0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82};
    QCOMPARE(file.write(reinterpret_cast<const char *>(pngData), sizeof(pngData)),
             (qint64)sizeof(pngData));
    file.close();

    // Resolve canonical file path to match DB
    filePath = QFileInfo(filePath).canonicalFilePath();
    QVERIFY(!filePath.isEmpty());

    // Add watched folder
    QVERIFY(m_controller->addWatchedFolder(tempPath));

    // Wait for the scanner to finish
    QSignalSpy spyLibrary(m_controller, &LibraryController::libraryChanged);
    QVERIFY(spyLibrary.wait(5000));
    QThreadPool::globalInstance()->waitForDone();
    while (m_controller->isScanning()) {
        QTest::qWait(50);
    }

    // Check if file was inserted
    {
        QSqlDatabase db = m_dbMgr->getDatabaseConnection();
        QSqlQuery query(db);
        QVERIFY(query.exec("SELECT absolute_path, is_offline, file_name FROM documents;"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), filePath);
        QCOMPARE(query.value(1).toInt(), 0);
        QCOMPARE(query.value(2).toString(), QString("documentA.png"));
    }

    // Delete file to test physical deletion from index (volume/folder still exists)
    QVERIFY(QFile::remove(filePath));

    // Request scanner again
    spyLibrary.clear();
    emit m_controller->scanRequested(tempPath);
    QVERIFY(spyLibrary.wait(5000));
    QThreadPool::globalInstance()->waitForDone();
    while (m_controller->isScanning()) {
        QTest::qWait(50);
    }

    // Check if document was deleted from database
    {
        QSqlDatabase db = m_dbMgr->getDatabaseConnection();
        QSqlQuery query(db);
        QVERIFY(query.exec("SELECT COUNT(*) FROM documents;"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 0);  // Should be removed from database/index
    }

    // Re-create the file to ingest it again
    QFile file2(filePath);
    QVERIFY(file2.open(QIODevice::WriteOnly));
    QCOMPARE(file2.write(reinterpret_cast<const char *>(pngData), sizeof(pngData)),
             (qint64)sizeof(pngData));
    file2.close();

    // Run scanner again to ingest
    spyLibrary.clear();
    emit m_controller->scanRequested(tempPath);
    QVERIFY(spyLibrary.wait(5000));
    QThreadPool::globalInstance()->waitForDone();
    while (m_controller->isScanning()) {
        QTest::qWait(50);
    }

    // Check that it's back in DB online
    int newDocId = -1;
    {
        QSqlDatabase db = m_dbMgr->getDatabaseConnection();
        QSqlQuery query(db);
        QVERIFY(query.exec("SELECT id, is_offline FROM documents;"));
        QVERIFY(query.next());
        newDocId = query.value(0).toInt();
        QCOMPARE(query.value(1).toInt(), 0);
    }

    // Now simulate volume disconnect by renaming the temp directory
    QString offlineTempPath = tempPath + "_offline";
    QVERIFY(QDir().rename(tempPath, offlineTempPath));

    // Run scanner again (on the original path, which is now missing)
    spyLibrary.clear();
    emit m_controller->scanRequested(tempPath);
    spyLibrary.wait(1000);
    QThreadPool::globalInstance()->waitForDone();
    while (m_controller->isScanning()) {
        QTest::qWait(50);
    }

    // Check if marked offline (not deleted, because folder was missing)
    {
        QSqlDatabase db = m_dbMgr->getDatabaseConnection();
        QSqlQuery query(db);
        QVERIFY(query.exec("SELECT id, is_offline FROM documents;"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), newDocId);
        QCOMPARE(query.value(1).toInt(), 1);  // should be offline
    }

    // Rename back to simulate volume reconnect
    QVERIFY(QDir().rename(offlineTempPath, tempPath));

    // Run scanner again
    spyLibrary.clear();
    emit m_controller->scanRequested(tempPath);
    spyLibrary.wait(1000);
    QThreadPool::globalInstance()->waitForDone();
    while (m_controller->isScanning()) {
        QTest::qWait(50);
    }

    // Check if back online
    {
        QSqlDatabase db = m_dbMgr->getDatabaseConnection();
        QSqlQuery query(db);
        QVERIFY(query.exec("SELECT id, is_offline FROM documents;"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), newDocId);
        QCOMPARE(query.value(1).toInt(), 0);  // should be online again
    }

    // Now simulate volume disconnect again
    QVERIFY(QDir().rename(tempPath, offlineTempPath));

    // Scan to mark it offline
    spyLibrary.clear();
    emit m_controller->scanRequested(tempPath);
    spyLibrary.wait(1000);
    QThreadPool::globalInstance()->waitForDone();
    while (m_controller->isScanning()) {
        QTest::qWait(50);
    }

    // Confirm it's offline again
    {
        QSqlDatabase db = m_dbMgr->getDatabaseConnection();
        QSqlQuery query(db);
        QVERIFY(query.exec("SELECT id, is_offline FROM documents;"));
        QVERIFY(query.next());
        QCOMPARE(query.value(1).toInt(), 1);  // should be offline
    }

    // Now remove the file while volume is offline
    QString offlineFilePath = offlineTempPath + "/documentA.png";
    QVERIFY(QFile::remove(offlineFilePath));

    // Reconnect the volume (so tempPath exists, but documentA.png is missing)
    QVERIFY(QDir().rename(offlineTempPath, tempPath));

    // Scan. Since the volume/folder is now online, but the file is missing on disk,
    // the scanner should treat it as deleted (not offline) and remove it from database.
    spyLibrary.clear();
    emit m_controller->scanRequested(tempPath);
    spyLibrary.wait(1000);
    QThreadPool::globalInstance()->waitForDone();
    while (m_controller->isScanning()) {
        QTest::qWait(50);
    }

    // Check if document was deleted from database
    {
        QSqlDatabase db = m_dbMgr->getDatabaseConnection();
        QSqlQuery query(db);
        QVERIFY(query.exec("SELECT COUNT(*) FROM documents;"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 0);  // Should be completely removed from database/index
    }
}

void TestWorkers::testPdfAndOcr()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString tempPath = QFileInfo(tempDir.path()).canonicalFilePath();

    // Insert tempPath into watched_folders table so ScannerTask doesn't skip it
    QSqlDatabase initDb = m_dbMgr->getDatabaseConnection();
    QSqlQuery insertFolderQuery(initDb);
    insertFolderQuery.prepare("INSERT INTO watched_folders (absolute_path) VALUES (:path);");
    insertFolderQuery.bindValue(":path", tempPath);
    QVERIFY(insertFolderQuery.exec());

    // Generate a valid PDF file using QPdfWriter
    QString pdfPath = QDir(tempPath).filePath("documentB.pdf");
    {
        QPdfWriter pdfWriter(pdfPath);
        pdfWriter.setPageSize(QPageSize(QPageSize::A4));
        QPainter painter(&pdfWriter);
        painter.drawText(100, 100, "This is a test PDF document for NinjaLibrary.");
        painter.end();
    }
    pdfPath = QFileInfo(pdfPath).canonicalFilePath();

    // Write sidecar metadata for documentB.pdf before scanning it to cover sidecar ingestion
    {
        QString sidecarDir = QDir::homePath() + "/.local/share/NinjaLibrary/sidecars/";
        QDir().mkpath(sidecarDir);
        QCryptographicHash hasher(QCryptographicHash::Sha256);
        hasher.addData(pdfPath.toUtf8());
        QString sidecarPath = sidecarDir + hasher.result().toHex() + ".ninja";

        QFile file(sidecarPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QJsonObject obj;
        obj["star_rating"] = 5;
        obj["notes"] = "Yosemite style rules.";
        QJsonArray arr = {"pdf-tag1", "pdf-tag2"};
        obj["tags"] = arr;
        file.write(QJsonDocument(obj).toJson());
        file.close();
    }

    // Create nested subdirectory and file to cover recursive folder crawling
    QString nestedDir = tempPath + "/nested";
    QDir().mkpath(nestedDir);
    QString nestedFilePath = QDir(nestedDir).filePath("nested_image.jpg");
    QFile nestedFile(nestedFilePath);
    QVERIFY(nestedFile.open(QIODevice::WriteOnly));
    nestedFile.write("nested jpg dummy");
    nestedFile.close();

    // Run ScannerTask synchronously
    ScannerTask scanner(m_dbMgr, tempPath);
    int ocrReqDocId = -1;
    QString ocrReqPath;
    int thumbReqDocId = -1;
    QString thumbReqPath;

    connect(&scanner, &ScannerTask::ocrRequested, [&](int docId, const QString &filePath) {
        if (filePath == pdfPath) {
            ocrReqDocId = docId;
            ocrReqPath = filePath;
        }
    });
    connect(&scanner, &ScannerTask::thumbnailRequested, [&](int docId, const QString &filePath) {
        if (filePath == pdfPath) {
            thumbReqDocId = docId;
            thumbReqPath = filePath;
        }
    });

    scanner.run();

    // Check that scanner inserted the PDF
    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    QSqlQuery query(db);
    query.prepare(
        "SELECT id, absolute_path, page_count FROM documents WHERE file_name = 'documentB.pdf';");
    QVERIFY(query.exec());
    QVERIFY(query.next());
    int docId = query.value(0).toInt();
    QString dbPath = query.value(1).toString();
    int pageCount = query.value(2).toInt();

    QCOMPARE(dbPath, pdfPath);
    QVERIFY(pageCount > 0);

    // Verify thumbnail and OCR request signals were emitted by ScannerTask
    QCOMPARE(ocrReqDocId, docId);
    QCOMPARE(ocrReqPath, pdfPath);
    QCOMPARE(thumbReqDocId, docId);
    QCOMPARE(thumbReqPath, pdfPath);

    // Run ThumbnailTask synchronously
    ThumbnailTask thumbTask(m_dbMgr, docId, pdfPath);
    thumbTask.run();

    // Run OcrTask synchronously
    OcrTask ocrTask(m_dbMgr, docId, pdfPath);
    ocrTask.run();

    // Verify OCR text has been saved (or task completed)
    QSqlQuery ftsQuery(db);
    ftsQuery.prepare("SELECT text_snippet FROM document_search WHERE document_id = :id;");
    ftsQuery.bindValue(":id", docId);
    QVERIFY(ftsQuery.exec());
    if (ftsQuery.next()) {
        QString text = ftsQuery.value(0).toString();
        qDebug() << "OCR extracted text:" << text;
    }

    // Modify documentB.pdf to trigger modification detection in ScannerTask
    QTest::qWait(2000);  // wait so file modification time is different
    {
        QPdfWriter pdfWriter(pdfPath);
        pdfWriter.setPageSize(QPageSize(QPageSize::A4));
        QPainter painter(&pdfWriter);
        painter.drawText(100, 100, "This is a test PDF document for NinjaLibrary.");
        painter.drawText(100, 150, "Added extra text to modify the file content and size.");
        painter.end();
    }

    // Re-run scanner synchronously to cover file modification detection block
    scanner.run();
}

void TestWorkers::testLibraryControllerAPIs()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString tempPath = QFileInfo(tempDir.path()).canonicalFilePath();

    // Create a mock image file
    QString imgPath = QDir(tempPath).filePath("image.jpg");
    QFile file(imgPath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("fake image data");
    file.close();
    imgPath = QFileInfo(imgPath).canonicalFilePath();

    // 1. Test addWatchedFolder with empty/invalid inputs
    QVERIFY(!m_controller->addWatchedFolder(""));
    QVERIFY(!m_controller->addWatchedFolder("/non/existent/path/for/sure/1234"));

    QSignalSpy scanningSpy(m_controller, &LibraryController::isScanningChanged);

    // 2. Test addWatchedFolder with valid path
    QVERIFY(m_controller->addWatchedFolder(tempPath));
    QVERIFY(m_controller->watchedFolders().contains(tempPath));

    // Test UI getters while active
    if (!m_controller->isScanning()) {
        QVERIFY(scanningSpy.wait(2000));
    }
    QVERIFY(m_controller->isScanning());
    QVERIFY(m_controller->scanProgress() >= 0.0);
    QVERIFY(!m_controller->scanStatusText().isEmpty());

    // Wait for the scanner to finish so it inserts the document
    QSignalSpy spyLibrary(m_controller, &LibraryController::libraryChanged);
    QVERIFY(spyLibrary.wait(5000));

    // Process queued signals and wait for background OCR/Thumbnail tasks to complete
    QCoreApplication::processEvents();
    QThreadPool::globalInstance()->waitForDone();

    // Test UI getters while idle
    if (m_controller->isScanning()) {
        QVERIFY(scanningSpy.wait(5000));
    }
    QVERIFY(!m_controller->isScanning());
    QCOMPARE(m_controller->scanProgress(), 0.0);
    QVERIFY(m_controller->scanStatusText().isEmpty());

    // Check DB for the document
    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    QSqlQuery query(db);
    QVERIFY(query.exec("SELECT id FROM documents WHERE file_name = 'image.jpg';"));
    QVERIFY(query.next());
    int docId = query.value(0).toInt();

    // 3. Test batchUpdateTags
    QStringList tags = {"tagA", "tagB", ""};
    QVERIFY(m_controller->batchUpdateTags({docId}, tags));

    // Verify in DB
    query.prepare(
        "SELECT t.name FROM tags t JOIN document_tags dt ON t.id = dt.tag_id WHERE dt.document_id "
        "= :id;");
    query.bindValue(":id", docId);
    QVERIFY(query.exec());
    QStringList dbTags;
    while (query.next()) {
        dbTags << query.value(0).toString();
    }
    QVERIFY(dbTags.contains("tagA"));

    // Test search APIs
    m_controller->searchDocuments("image");
    m_controller->searchDocuments("dummy text");
    m_controller->searchDocumentContent(docId, imgPath, "image");
    QVERIFY(dbTags.contains("tagB"));
    QVERIFY(!dbTags.contains(""));  // empty tag skipped

    // Test batchUpdateTags empty inputs (graceful return)
    QVERIFY(m_controller->batchUpdateTags({}, {"tagC"}));

    // Test batchAddTags
    QVERIFY(m_controller->batchAddTags({docId}, {"addedTag"}));
    QVERIFY(m_controller->getUniqueTags().contains("addedTag"));

    // Test batchRemoveTags
    QVERIFY(m_controller->batchRemoveTags({docId}, {"addedTag"}));
    QVERIFY(!m_controller->getUniqueTags().contains("addedTag"));

    // 3b. Test batchAddTags, batchRemoveTags, and getUniqueTags
    // Insert a second document for batch operations
    int docId2 = -1;
    {
        QSqlQuery insertDoc2(db);
        insertDoc2.prepare(
            "INSERT INTO documents (folder_id, file_name, absolute_path, file_size, is_offline) "
            "VALUES (1, 'image2.jpg', :path, 100, 0);");
        insertDoc2.bindValue(":path", tempPath + "/image2.jpg");
        QVERIFY(insertDoc2.exec());
        docId2 = insertDoc2.lastInsertId().toInt();
    }

    // Verify initial unique tags
    QStringList uniqueTags = m_controller->getUniqueTags();
    QVERIFY(uniqueTags.contains("tagA"));
    QVERIFY(uniqueTags.contains("tagB"));

    // Batch Add Tags "tagB" and "tagC" to both docId and docId2
    QVERIFY(m_controller->batchAddTags({docId, docId2}, {"tagB", "tagC"}));

    // Verify tags in DB
    query.prepare(
        "SELECT t.name FROM tags t JOIN document_tags dt ON t.id = dt.tag_id WHERE dt.document_id "
        "= :id;");
    query.bindValue(":id", docId);
    QVERIFY(query.exec());
    QStringList doc1Tags;
    while (query.next()) {
        doc1Tags << query.value(0).toString();
    }
    QVERIFY(doc1Tags.contains("tagA"));  // preserved!
    QVERIFY(doc1Tags.contains("tagB"));
    QVERIFY(doc1Tags.contains("tagC"));

    query.bindValue(":id", docId2);
    QVERIFY(query.exec());
    QStringList doc2Tags;
    while (query.next()) {
        doc2Tags << query.value(0).toString();
    }
    QVERIFY(!doc2Tags.contains("tagA"));
    QVERIFY(doc2Tags.contains("tagB"));
    QVERIFY(doc2Tags.contains("tagC"));

    // Batch Remove tagB from both docId and docId2
    QVERIFY(m_controller->batchRemoveTags({docId, docId2}, {"tagB"}));

    query.bindValue(":id", docId);
    QVERIFY(query.exec());
    doc1Tags.clear();
    while (query.next()) {
        doc1Tags << query.value(0).toString();
    }
    QVERIFY(!doc1Tags.contains("tagB"));
    QVERIFY(doc1Tags.contains("tagC"));

    query.bindValue(":id", docId2);
    QVERIFY(query.exec());
    doc2Tags.clear();
    while (query.next()) {
        doc2Tags << query.value(0).toString();
    }
    QVERIFY(!doc2Tags.contains("tagB"));
    QVERIFY(doc2Tags.contains("tagC"));

    // Verify getUniqueTags contains tagC and tagA, but not tagB (since it was removed from all
    // documents)
    uniqueTags = m_controller->getUniqueTags();
    QVERIFY(uniqueTags.contains("tagA"));
    QVERIFY(uniqueTags.contains("tagC"));
    QVERIFY(!uniqueTags.contains("tagB"));

    // 4. Test batchUpdateRating
    QVERIFY(m_controller->batchUpdateRating({docId}, 4));
    // Verify in DB
    query.prepare("SELECT star_rating FROM documents WHERE id = :id;");
    query.bindValue(":id", docId);
    QVERIFY(query.exec());
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 4);

    // Rating boundaries
    QVERIFY(!m_controller->batchUpdateRating({docId}, -1));
    QVERIFY(!m_controller->batchUpdateRating({docId}, 6));
    QVERIFY(m_controller->batchUpdateRating({}, 3));

    // 5. Test updateNotes
    QVERIFY(m_controller->updateNotes(docId, "Notes for image file."));
    // Verify in DB
    query.prepare("SELECT notes FROM document_search WHERE document_id = :id;");
    query.bindValue(":id", docId);
    QVERIFY(query.exec());
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QString("Notes for image file."));

    // 6. Test sidecar reading and writing directly
    QStringList readTags;
    int readRating = 0;
    QString readNotes;
    QVERIFY(m_controller->readSidecar(imgPath, readTags, readRating, readNotes));
    QVERIFY(readTags.contains("tagA"));
    QCOMPARE(readRating, 4);
    QCOMPARE(readNotes, QString("Notes for image file."));

    // Test readSidecar for non-existent sidecar
    QStringList dummyTags;
    int dummyRating = 0;
    QString dummyNotes;
    QVERIFY(!m_controller->readSidecar("/no/such/document/path.pdf", dummyTags, dummyRating,
                                       dummyNotes));

    // 7. Test handleDroppedUrl
    // A. Dropping a folder URL
    QString dirUrl = "file://" + tempPath;
    QVariantMap dirResult = m_controller->handleDroppedUrl(dirUrl);
    QCOMPARE(dirResult["status"].toString(), QString("success"));
    QCOMPARE(dirResult["isFolder"].toBool(), true);
    QCOMPARE(dirResult["watchedFolder"].toString(), tempPath);
    QCOMPARE(dirResult["docPath"].toString(), QString(""));

    // B. Dropping a file URL that already exists in database
    QString fileUrl = "file://" + imgPath;
    QVariantMap fileResult = m_controller->handleDroppedUrl(fileUrl);
    QCOMPARE(fileResult["status"].toString(), QString("success"));
    QCOMPARE(fileResult["isFolder"].toBool(), false);
    QCOMPARE(fileResult["watchedFolder"].toString(), tempPath);
    QCOMPARE(fileResult["docPath"].toString(), imgPath);
    QCOMPARE(fileResult["docId"].toInt(), docId);

    // C. Dropping a file URL that is not yet in the database (new watched folder)
    QTemporaryDir tempDir2;
    QVERIFY(tempDir2.isValid());
    QString newFolderPath = QFileInfo(tempDir2.path()).canonicalFilePath();
    QString newFilePath = QDir(newFolderPath).filePath("new_document.pdf");
    QFile newFile(newFilePath);
    QVERIFY(newFile.open(QIODevice::WriteOnly));
    newFile.write("dummy content");
    newFile.close();
    newFilePath = QFileInfo(newFilePath).canonicalFilePath();

    QVariantMap newFileResult = m_controller->handleDroppedUrl("file://" + newFilePath);
    QCOMPARE(newFileResult["status"].toString(), QString("success"));
    QCOMPARE(newFileResult["isFolder"].toBool(), false);
    QCOMPARE(newFileResult["watchedFolder"].toString(), newFolderPath);
    QCOMPARE(newFileResult["docPath"].toString(), newFilePath);
    QCOMPARE(newFileResult["docId"].toInt(),
             -1);  // not in database yet because background scanner hasn't run for it

    // 8. Test markDocumentOpened
    spyLibrary.clear();
    QVERIFY(m_controller->markDocumentOpened(docId));
    QCOMPARE(spyLibrary.count(), 1);  // should emit libraryChanged

    // Verify last_opened updated in DB
    query.prepare("SELECT last_opened FROM documents WHERE id = :id;");
    query.bindValue(":id", docId);
    QVERIFY(query.exec());
    QVERIFY(query.next());
    qint64 lastOpenedVal = query.value(0).toLongLong();
    QVERIFY(lastOpenedVal > 0);
    qint64 nowSecs = QDateTime::currentSecsSinceEpoch();
    QVERIFY(qAbs(nowSecs - lastOpenedVal) < 5);  // within 5 seconds

    query.finish();

    // 9. Test moveToTrash
    QString testTrashPath = QDir(tempPath).filePath("trash_test.pdf");
    QFile trashFile(testTrashPath);
    QVERIFY(trashFile.open(QIODevice::WriteOnly));
    trashFile.write("trash content");
    trashFile.close();
    testTrashPath = QFileInfo(testTrashPath).canonicalFilePath();

    int trashDocId = -1;
    {
        QSqlQuery insertTrashDoc(db);
        insertTrashDoc.prepare(
            "INSERT INTO documents (folder_id, file_name, absolute_path, file_size, is_offline) "
            "VALUES (1, 'trash_test.pdf', :path, 100, 0);");
        insertTrashDoc.bindValue(":path", testTrashPath);
        QVERIFY(insertTrashDoc.exec());
        trashDocId = insertTrashDoc.lastInsertId().toInt();
    }

    // Create sidecar
    QVERIFY(m_controller->writeSidecar(testTrashPath, {"tagA"}, 3, "trash-notes"));

    // Insert FTS search record manually
    {
        QSqlQuery ftsQuery(db);
        ftsQuery.prepare(
            "INSERT INTO document_search (document_id, file_name, text_snippet, notes) VALUES "
            "(:id, 'trash_test.pdf', 'dummy text', 'trash-notes');");
        ftsQuery.bindValue(":id", trashDocId);
        QVERIFY(ftsQuery.exec());
    }

    // Insert tag link manually
    {
        QSqlQuery tagLinkQuery(db);
        tagLinkQuery.prepare("INSERT INTO document_tags (document_id, tag_id) VALUES (:docId, 1);");
        tagLinkQuery.bindValue(":docId", trashDocId);
        QVERIFY(tagLinkQuery.exec());
    }

    // Call moveToTrash
    spyLibrary.clear();
    QVERIFY(m_controller->moveToTrash(trashDocId, testTrashPath));
    QCOMPARE(spyLibrary.count(), 1);  // should emit libraryChanged

    // Verify physical file is gone (moved to trash)
    QVERIFY(!QFile::exists(testTrashPath));

    // Verify sidecar is gone
    QString checkTrashTagsList;
    int checkTrashRating = 0;
    QString checkTrashNotes;
    QStringList dummyTagsList;
    QVERIFY(!m_controller->readSidecar(testTrashPath, dummyTagsList, checkTrashRating,
                                       checkTrashNotes));

    // Verify DB records are deleted
    {
        QSqlQuery checkDocQuery(db);
        checkDocQuery.prepare("SELECT COUNT(*) FROM documents WHERE id = :id;");
        checkDocQuery.bindValue(":id", trashDocId);
        QVERIFY(checkDocQuery.exec());
        QVERIFY(checkDocQuery.next());
        QCOMPARE(checkDocQuery.value(0).toInt(), 0);

        QSqlQuery checkTagsQuery(db);
        checkTagsQuery.prepare("SELECT COUNT(*) FROM document_tags WHERE document_id = :id;");
        checkTagsQuery.bindValue(":id", trashDocId);
        QVERIFY(checkTagsQuery.exec());
        QVERIFY(checkTagsQuery.next());
        QCOMPARE(checkTagsQuery.value(0).toInt(), 0);

        QSqlQuery checkSearchQuery(db);
        checkSearchQuery.prepare("SELECT COUNT(*) FROM document_search WHERE document_id = :id;");
        checkSearchQuery.bindValue(":id", trashDocId);
        QVERIFY(checkSearchQuery.exec());
        QVERIFY(checkSearchQuery.next());
        QCOMPARE(checkSearchQuery.value(0).toInt(), 0);
    }

    // Wait for any pending thread pool tasks to complete before modifying tables
    QCoreApplication::processEvents();
    QThreadPool::globalInstance()->waitForDone();

    // Clean up watched folder for tempDir2
    QVERIFY(m_controller->removeWatchedFolder(newFolderPath));

    // Verify sidecar exists before removing watched folder
    QStringList checkTags;
    int checkRating = 0;
    QString checkNotes;
    QVERIFY(m_controller->readSidecar(imgPath, checkTags, checkRating, checkNotes));

    // Test removeWatchedFolder with empty/invalid inputs
    QVERIFY(!m_controller->removeWatchedFolder(""));
    QVERIFY(m_controller->removeWatchedFolder(tempPath));
    QVERIFY(!m_controller->watchedFolders().contains(tempPath));

    // Test Unicode normalization edge cases for removeWatchedFolder
    {
        QTemporaryDir testUnicodeDir;
        QVERIFY(testUnicodeDir.isValid());
        // Use a path with 'ö' constructed directly with NFC
        QString baseDirPath = QFileInfo(testUnicodeDir.path()).canonicalFilePath();
        QString nfcDirName = QString::fromUtf8("h\xC3\xB6lm\xC3\xB6_kansio");
        QString nfcDirPath = baseDirPath + "/" + nfcDirName;
        QVERIFY(QDir().mkpath(nfcDirPath));

        QVERIFY(m_controller->addWatchedFolder(nfcDirPath));
        // Note: the exact stored form might differ depending on OS (e.g. macOS native filesystem might convert it to NFD)
        // By checking both forms in our removal function, we are resilient against this.

        QCoreApplication::processEvents();
        QThreadPool::globalInstance()->waitForDone();

        // Pass the explicitly decomposed (NFD) string to removeWatchedFolder
        QString nfdDirPath = nfcDirPath.normalized(QString::NormalizationForm_D);
        QVERIFY(m_controller->removeWatchedFolder(nfdDirPath));
        
        // Verify it was successfully removed regardless of how the OS returned the directory name
        bool found = false;
        const QStringList folders = m_controller->watchedFolders();
        for (const QString &f : folders) {
            if (f.normalized(QString::NormalizationForm_C) == nfcDirPath.normalized(QString::NormalizationForm_C)) {
                found = true;
                break;
            }
        }
        QVERIFY(!found);
    }

    // Verify sidecar was deleted (garbage collected) because the document was deleted
    QVERIFY(!m_controller->readSidecar(imgPath, checkTags, checkRating, checkNotes));

    // Test scan control APIs
    bool initialPause = m_controller->isScanPaused();
    m_controller->toggleScanPause(); 
    QVERIFY(m_controller->isScanPaused() != initialPause);
    m_controller->toggleScanPause();
    QVERIFY(m_controller->isScanPaused() == initialPause);
    
    // Verify getters don't crash and return valid strings
    QVERIFY(!m_controller->scanStatusText().isNull());

    // Test removing a physically deleted folder
    QTemporaryDir deletedDir;
    QVERIFY(deletedDir.isValid());
    QString deletedDirPath = QFileInfo(deletedDir.path()).canonicalFilePath();
    QVERIFY(m_controller->addWatchedFolder(deletedDirPath));
    QVERIFY(QDir(deletedDirPath).removeRecursively());
    QVERIFY(m_controller->removeWatchedFolder(deletedDirPath));
    QVERIFY(!m_controller->watchedFolders().contains(deletedDirPath));

    // Test Feature 4: Folder conflict and merging
    {
        QTemporaryDir testConflictDir;
        QVERIFY(testConflictDir.isValid());
        QString parentDir = QFileInfo(testConflictDir.path()).canonicalFilePath();
        QString subDir = parentDir + "/subFolder";
        QVERIFY(QDir().mkpath(subDir));

        // Add subfolder first
        QVERIFY(m_controller->addWatchedFolder(subDir));
        QVERIFY(m_controller->watchedFolders().contains(subDir));

        QCoreApplication::processEvents();
        QThreadPool::globalInstance()->waitForDone();

        // Try to add sub-subfolder (should conflict)
        QString subSubDir = subDir + "/subSubFolder";
        QVERIFY(QDir().mkpath(subSubDir));
        QSignalSpy spyConflict(m_controller, &LibraryController::folderConflictDetected);
        QVERIFY(!m_controller->addWatchedFolder(subSubDir));
        QCOMPARE(spyConflict.count(), 1);
        QVERIFY(spyConflict.at(0).at(0).toString().contains("Folder is already monitored under"));

        // Add parent folder (should merge and remove child folder watch)
        QVERIFY(m_controller->addWatchedFolder(parentDir));
        QVERIFY(m_controller->watchedFolders().contains(parentDir));
        QVERIFY(!m_controller->watchedFolders().contains(subDir));

        QCoreApplication::processEvents();
        QThreadPool::globalInstance()->waitForDone();

        // Clean up
        QVERIFY(m_controller->removeWatchedFolder(parentDir));

        QCoreApplication::processEvents();
        QThreadPool::globalInstance()->waitForDone();
    }

    // Test Feature 5: Low disk space detection and pause
    {
        ScannerTask::s_lowDiskSpace = true;
        ScannerTask::s_scanPaused = true;

        QTemporaryDir testDiskSpaceDir;
        QVERIFY(testDiskSpaceDir.isValid());
        QString diskSpacePath = QFileInfo(testDiskSpaceDir.path()).canonicalFilePath();

        QVERIFY(m_controller->addWatchedFolder(diskSpacePath));
        QVERIFY(m_controller->isScanning());
        QCOMPARE(m_controller->scanStatusText(),
                 QString("Scanning Paused: Low Disk Space (< 500MB)"));

        // Reset and clean up
        ScannerTask::s_lowDiskSpace = false;
        ScannerTask::s_scanPaused = false;
        {
            QMutexLocker locker(&ScannerTask::s_pauseMutex);
            ScannerTask::s_pauseCondition.wakeAll();
        }

        QCoreApplication::processEvents();
        QThreadPool::globalInstance()->waitForDone();

        QVERIFY(m_controller->removeWatchedFolder(diskSpacePath));

        QCoreApplication::processEvents();
        QThreadPool::globalInstance()->waitForDone();
    }
}

void TestWorkers::testTextAndDocIngestion()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString tempPath = QFileInfo(tempDir.path()).canonicalFilePath();

    // Add folder path to DB
    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    QSqlQuery query(db);
    query.prepare("INSERT INTO watched_folders (absolute_path) VALUES (:path);");
    query.bindValue(":path", tempPath);
    QVERIFY(query.exec());

    // 1. Create a .txt file
    QString txtPath = QDir(tempPath).filePath("notes.txt");
    {
        QFile file(txtPath);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&file);
        out << "Hello from NinjaLibrary plain text format. Secret code 98765.";
        file.close();
    }
    txtPath = QFileInfo(txtPath).canonicalFilePath();

    // 2. Create a .md file
    QString mdPath = QDir(tempPath).filePath("readme.md");
    {
        QFile file(mdPath);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&file);
        out << "# Project README\nSupport markdown indexing.";
        file.close();
    }
    mdPath = QFileInfo(mdPath).canonicalFilePath();

    // 3. Create a .doc file containing RTF syntax (which NSAttributedString will parse)
    QString docPath = QDir(tempPath).filePath("document.doc");
    {
        QFile file(docPath);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&file);
        out << "{\\rtf1\\ansi This is a test document in rich text format. Secret term: applepie.}";
        file.close();
    }
    docPath = QFileInfo(docPath).canonicalFilePath();

    // 4. Create dummy .xlsx and .pptx files (metadata-only)
    QString xlsxPath = QDir(tempPath).filePath("spreadsheet.xlsx");
    {
        QFile file(xlsxPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("dummy spreadsheet content");
        file.close();
    }
    xlsxPath = QFileInfo(xlsxPath).canonicalFilePath();

    QString pptxPath = QDir(tempPath).filePath("slides.pptx");
    {
        QFile file(pptxPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("dummy presentation content");
        file.close();
    }
    pptxPath = QFileInfo(pptxPath).canonicalFilePath();

    // Run ScannerTask synchronously
    ScannerTask scanner(m_dbMgr, tempPath);

    QSignalSpy spyFinished(&scanner, &ScannerTask::finished);
    QSignalSpy spyOcr(&scanner, &ScannerTask::ocrRequested);
    QSignalSpy spyThumb(&scanner, &ScannerTask::thumbnailRequested);

    scanner.run();

    QCOMPARE(spyFinished.size(), 1);

    // Verify that NO ocr or thumbnail tasks were requested for the txt, md, doc, xlsx, or pptx
    // files
    QCOMPARE(spyOcr.size(), 0);
    QCOMPARE(spyThumb.size(), 0);

    // Verify that the files were ingested correctly
    {
        QSqlQuery verifyQuery(db);
        verifyQuery.prepare(
            "SELECT d.file_name, s.text_snippet FROM documents d JOIN document_search s ON d.id = "
            "s.document_id WHERE d.folder_id = (SELECT id FROM watched_folders WHERE absolute_path "
            "= :path);");
        verifyQuery.bindValue(":path", tempPath);
        QVERIFY(verifyQuery.exec());

        bool foundTxt = false;
        bool foundMd = false;
        bool foundDoc = false;
        bool foundXlsx = false;
        bool foundPptx = false;

        while (verifyQuery.next()) {
            QString name = verifyQuery.value(0).toString();
            QString text = verifyQuery.value(1).toString();

            if (name == "notes.txt") {
                foundTxt = true;
                QVERIFY(text.contains("Secret code 98765"));
            } else if (name == "readme.md") {
                foundMd = true;
                QVERIFY(text.contains("markdown indexing"));
            } else if (name == "document.doc") {
                foundDoc = true;
#ifdef Q_OS_MAC
                // On macOS, NSAttributedString should successfully parse the RTF text inside
                // document.doc
                QVERIFY(text.contains("applepie"));
#endif
            } else if (name == "spreadsheet.xlsx") {
                foundXlsx = true;
                QVERIFY(text.isEmpty());  // should have no extracted text
            } else if (name == "slides.pptx") {
                foundPptx = true;
                QVERIFY(text.isEmpty());  // should have no extracted text
            }
        }

        QVERIFY(foundTxt);
        QVERIFY(foundMd);
        QVERIFY(foundDoc);
        QVERIFY(foundXlsx);
        QVERIFY(foundPptx);
    }
}

void TestWorkers::testMacBookmarksAndEdgeCases()
{
#ifdef Q_OS_MAC
    // Test MacBookmarks functions on macOS
    QString testPath = "/tmp";
    QByteArray bookmark = MacBookmarks::getBookmarkForUrl(testPath);
    if (!bookmark.isEmpty()) {
        QString resolved;
        MacBookmarks::resolveBookmark(bookmark, resolved);
    }

    // Test invalid bookmark resolution
    QString resolved;
    QByteArray invalidBookmark("invalid-bookmark-data-here-1234");
    MacBookmarks::resolveBookmark(invalidBookmark, resolved);

    // Cover empty path bookmark creation
    MacBookmarks::getBookmarkForUrl("");
#else
    QSKIP("MacBookmarks tests are macOS specific");
#endif
}

void TestWorkers::testOcrNonsenseRejection()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString tempPath = QFileInfo(tempDir.path()).canonicalFilePath();
    QString imgPath = QDir(tempPath).filePath("blank.jpg");

    // Save a blank white image
    QImage blankImg(200, 200, QImage::Format_RGB32);
    blankImg.fill(Qt::white);
    QVERIFY(blankImg.save(imgPath));
    imgPath = QFileInfo(imgPath).canonicalFilePath();

    QSqlDatabase db = m_dbMgr->getDatabaseConnection();

    // Insert a mock document record
    QSqlQuery query(db);
    query.prepare(
        "INSERT INTO documents (folder_id, file_name, absolute_path, file_size, is_offline) "
        "VALUES (1, 'blank.jpg', :path, 100, 0);");
    query.bindValue(":path", imgPath);
    QVERIFY(query.exec());
    int docId = query.lastInsertId().toInt();

    // Insert empty search snippet
    QSqlQuery insertSearch(db);
    insertSearch.prepare(
        "INSERT INTO document_search (document_id, file_name, text_snippet) VALUES (:docId, "
        "'blank.jpg', '');");
    insertSearch.bindValue(":docId", docId);
    QVERIFY(insertSearch.exec());

    // Run OcrTask on the blank image
    OcrTask ocrTask(m_dbMgr, docId, imgPath);
    ocrTask.run();

    // Verify search snippet remains empty or hasn't been populated with garbage
    QSqlQuery checkQuery(db);
    checkQuery.prepare("SELECT text_snippet FROM document_search WHERE document_id = :id;");
    checkQuery.bindValue(":id", docId);
    QVERIFY(checkQuery.exec());
    QVERIFY(checkQuery.next());
    QString snippet = checkQuery.value(0).toString().trimmed();
    QVERIFY(snippet.isEmpty());

    // Clean up blank image DB entries
    QSqlQuery cleanupQuery(db);
    cleanupQuery.prepare("DELETE FROM document_search WHERE document_id = :id;");
    cleanupQuery.bindValue(":id", docId);
    QVERIFY(cleanupQuery.exec());

    QSqlQuery cleanupDocQuery(db);
    cleanupDocQuery.prepare("DELETE FROM documents WHERE id = :id;");
    cleanupDocQuery.bindValue(":id", docId);
    QVERIFY(cleanupDocQuery.exec());

    // Test 2: Image with actual text to verify the OCR engine works
    QString textImgPath = QDir(tempPath).filePath("text_image.png");
    QImage textImg(400, 100, QImage::Format_RGB32);
    textImg.fill(Qt::white);
    {
        QPainter painter(&textImg);
        painter.setPen(Qt::black);
        QFont font = painter.font();
        font.setPointSize(24);
        font.setBold(true);
        painter.setFont(font);
        painter.drawText(textImg.rect(), Qt::AlignCenter, "NINJA OCR WORKED");
        painter.end();
    }
    QVERIFY(textImg.save(textImgPath));
    textImgPath = QFileInfo(textImgPath).canonicalFilePath();

    // Insert mock document record for text image
    QSqlQuery query2(db);
    query2.prepare(
        "INSERT INTO documents (folder_id, file_name, absolute_path, file_size, is_offline) "
        "VALUES (1, 'text_image.png', :path, 100, 0);");
    query2.bindValue(":path", textImgPath);
    QVERIFY(query2.exec());
    int docId2 = query2.lastInsertId().toInt();

    // Insert empty search snippet
    QSqlQuery insertSearch2(db);
    insertSearch2.prepare(
        "INSERT INTO document_search (document_id, file_name, text_snippet) VALUES (:docId, "
        "'text_image.png', '');");
    insertSearch2.bindValue(":docId", docId2);
    QVERIFY(insertSearch2.exec());

    // Run OcrTask on the text image
    OcrTask ocrTask2(m_dbMgr, docId2, textImgPath);
    ocrTask2.run();

    // Verify search snippet contains our text
    QSqlQuery checkQuery2(db);
    checkQuery2.prepare("SELECT text_snippet FROM document_search WHERE document_id = :id;");
    checkQuery2.bindValue(":id", docId2);
    QVERIFY(checkQuery2.exec());
    QVERIFY(checkQuery2.next());
    QString snippet2 = checkQuery2.value(0).toString().trimmed();
    qDebug() << "Extracted OCR text from image:" << snippet2;
    QVERIFY(snippet2.contains("NINJA", Qt::CaseInsensitive));
    QVERIFY(snippet2.contains("OCR", Qt::CaseInsensitive));
    QVERIFY(snippet2.contains("WORKED", Qt::CaseInsensitive));

    // Clean up text image DB entries
    QSqlQuery cleanupQuery2(db);
    cleanupQuery2.prepare("DELETE FROM document_search WHERE document_id = :id;");
    cleanupQuery2.bindValue(":id", docId2);
    QVERIFY(cleanupQuery2.exec());

    QSqlQuery cleanupDocQuery2(db);
    cleanupDocQuery2.prepare("DELETE FROM documents WHERE id = :id;");
    cleanupDocQuery2.bindValue(":id", docId2);
    QVERIFY(cleanupDocQuery2.exec());
}

void TestWorkers::testPackageDirectorySkipping()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString tempPath = QFileInfo(tempDir.path()).canonicalFilePath();

    // Create a normal directory with a valid document
    QString normalDir = tempPath + "/normal_folder";
    QDir().mkpath(normalDir);
    QString normalFile = normalDir + "/valid_doc.txt";
    QFile file1(normalFile);
    QVERIFY(file1.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream stream1(&file1);
    stream1 << "Some text content for valid document.";
    file1.close();

    // Create an app package folder structure with a document inside it
    QString appDir = tempPath + "/MyApp.app/Contents/Resources";
    QDir().mkpath(appDir);
    QString appFile = appDir + "/packaged_doc.txt";
    QFile file2(appFile);
    QVERIFY(file2.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream stream2(&file2);
    stream2 << "Some text content for packaged document.";
    file2.close();

    // Create a Photos library package folder structure with a document inside it
    QString photosDir = tempPath + "/MyPhotos.photoslibrary/Masters/2026";
    QDir().mkpath(photosDir);
    QString photosFile = photosDir + "/photo_doc.png";
    QFile file3(photosFile);
    QVERIFY(file3.open(QIODevice::WriteOnly));
    file3.write("fake png data");
    file3.close();

    // Clear documents table before running task
    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    QSqlQuery clearQuery(db);
    QVERIFY(clearQuery.exec("DELETE FROM documents;"));
    QVERIFY(clearQuery.exec("DELETE FROM document_search;"));

    // Insert tempPath into watched_folders table so ScannerTask doesn't skip it
    QSqlQuery insertFolderQuery(db);
    insertFolderQuery.prepare(
        "INSERT OR IGNORE INTO watched_folders (absolute_path) VALUES (:path);");
    insertFolderQuery.bindValue(":path", tempPath);
    QVERIFY(insertFolderQuery.exec());

    // Run ScannerTask synchronously
    ScannerTask scanner(m_dbMgr, tempPath);
    scanner.run();

    // Check DB to verify only valid_doc.txt was ingested, and the others were skipped
    QSqlQuery query(db);
    QVERIFY(query.exec("SELECT file_name FROM documents;"));

    QStringList filenames;
    while (query.next()) {
        filenames << query.value(0).toString();
    }

    qDebug() << "Ingested filenames:" << filenames;

    QVERIFY(filenames.contains("valid_doc.txt"));
    QVERIFY(!filenames.contains("packaged_doc.txt"));
    QVERIFY(!filenames.contains("photo_doc.png"));

    // Clean up DB entries
    QSqlQuery cleanupFolderQuery(db);
    cleanupFolderQuery.prepare("DELETE FROM watched_folders WHERE absolute_path = :path;");
    cleanupFolderQuery.bindValue(":path", tempPath);
    QVERIFY(cleanupFolderQuery.exec());
    QVERIFY(clearQuery.exec("DELETE FROM documents;"));
    QVERIFY(clearQuery.exec("DELETE FROM document_search;"));
}

void TestWorkers::testImageThumbnailAndOcrWithExif()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString tempPath = QFileInfo(tempDir.path()).canonicalFilePath();
    QString imgPath = QDir(tempPath).filePath("test_exif.jpg");

    // Save a valid non-blank colored image
    QImage testImg(100, 200, QImage::Format_RGB32);
    testImg.fill(Qt::red);
    QVERIFY(testImg.save(imgPath, "JPG"));
    imgPath = QFileInfo(imgPath).canonicalFilePath();

    QSqlDatabase db = m_dbMgr->getDatabaseConnection();

    // Insert mock document record
    QSqlQuery query(db);
    query.prepare(
        "INSERT INTO documents (folder_id, file_name, absolute_path, file_size, is_offline) "
        "VALUES (1, 'test_exif.jpg', :path, 100, 0);");
    query.bindValue(":path", imgPath);
    QVERIFY(query.exec());
    int docId = query.lastInsertId().toInt();

    // Run ThumbnailTask synchronously
    ThumbnailTask thumbTask(m_dbMgr, docId, imgPath);

    // We spy on the finished signal
    QSignalSpy spyThumb(&thumbTask, &ThumbnailTask::finished);
    thumbTask.run();

    QCOMPARE(spyThumb.count(), 1);
    QList<QVariant> thumbArgs = spyThumb.takeFirst();
    QCOMPARE(thumbArgs.at(0).toInt(), docId);
    QString thumbPath = thumbArgs.at(1).toString();
    QVERIFY(!thumbPath.isEmpty());
    QVERIFY(QFile::exists(thumbPath));

    // Verify thumbnail image was scaled correctly
    QImage thumbImg;
    QVERIFY(thumbImg.load(thumbPath));
    QVERIFY(thumbImg.width() <= 256 && thumbImg.height() <= 256);

    // Clean up
    QFile::remove(thumbPath);
    QSqlQuery cleanupQuery(db);
    cleanupQuery.prepare("DELETE FROM documents WHERE id = :id;");
    cleanupQuery.bindValue(":id", docId);
    QVERIFY(cleanupQuery.exec());
}

void TestWorkers::testSubdirectoryDeletionDetection()
{
    // Ensure thread pool is completely idle from previous tests
    QThreadPool::globalInstance()->waitForDone();

    // Clear database to ensure a completely clean state
    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    QSqlQuery clearQuery(db);
    QVERIFY(clearQuery.exec("DELETE FROM watched_folders;"));
    QVERIFY(clearQuery.exec("DELETE FROM documents;"));
    QVERIFY(clearQuery.exec("DELETE FROM document_search;"));

    // Create temporary directory
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString tempPath = QFileInfo(tempDir.path()).canonicalFilePath();
    QVERIFY(!tempPath.isEmpty());

    // Create a subdirectory
    QString subDirPath = tempPath + "/Subdir";
    QVERIFY(QDir().mkpath(subDirPath));

    // Create a supported file in the subdirectory
    QString filePath = QDir(subDirPath).filePath("subdocument.png");
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));

    // Write tiny PNG data
    static const unsigned char pngData[] = {
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48,
        0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x06, 0x00, 0x00,
        0x00, 0x1f, 0x15, 0xc4, 0x89, 0x00, 0x00, 0x00, 0x0a, 0x49, 0x44, 0x41, 0x54, 0x78,
        0x9c, 0x63, 0x00, 0x01, 0x00, 0x00, 0x05, 0x00, 0x01, 0x0d, 0x0a, 0x2d, 0xb4, 0x00,
        0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82};
    QCOMPARE(file.write(reinterpret_cast<const char *>(pngData), sizeof(pngData)),
             (qint64)sizeof(pngData));
    file.close();

    // Canonicalize paths
    filePath = QFileInfo(filePath).canonicalFilePath();
    QVERIFY(!filePath.isEmpty());
    subDirPath = QFileInfo(subDirPath).canonicalFilePath();
    QVERIFY(!subDirPath.isEmpty());

    QSignalSpy spyPostScan(m_controller, &LibraryController::postScanFinished);

    // Add watched folder (top-level)
    QVERIFY(m_controller->addWatchedFolder(tempPath));

    // Wait for the scanner and all background tasks to finish completely
    QThreadPool::globalInstance()->waitForDone();
    while (m_controller->isScanning()) {
        QTest::qWait(50);
    }
    if (spyPostScan.isEmpty()) {
        QVERIFY(spyPostScan.wait(5000));
    }

    // Assert that the subdirectory was successfully watched
    QVERIFY(m_controller->watchedDirectories().contains(subDirPath));

    // Verify it is in database
    {
        QSqlQuery query(db);
        QVERIFY(query.exec("SELECT absolute_path, is_offline FROM documents;"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), filePath);
        QCOMPARE(query.value(1).toInt(), 0);
    }

    // Now delete the file inside the subdirectory
    QVERIFY(QFile::remove(filePath));

    // Manually trigger the event handler to simulate filesystem watcher event
    spyPostScan.clear();
    QMetaObject::invokeMethod(m_controller, "onDirectoryChanged", Q_ARG(QString, subDirPath));

    // Wait for the second scan to finish completely
    QThreadPool::globalInstance()->waitForDone();
    while (m_controller->isScanning()) {
        QTest::qWait(50);
    }
    if (spyPostScan.isEmpty()) {
        QVERIFY(spyPostScan.wait(5000));
    }

    // Check if document was deleted from database
    {
        QSqlQuery query(db);
        QVERIFY(query.exec("SELECT COUNT(*) FROM documents;"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 0);  // Should be removed from database
    }

    // Clean up watched folder
    QVERIFY(m_controller->removeWatchedFolder(tempPath));
    QThreadPool::globalInstance()->waitForDone();
}

void TestWorkers::testCooperativeCancellation()
{
    // Ensure thread pool is completely idle
    QThreadPool::globalInstance()->waitForDone();

    // Clear database to ensure a completely clean state
    QSqlDatabase db = m_dbMgr->getDatabaseConnection();
    QSqlQuery clearQuery(db);
    QVERIFY(clearQuery.exec("DELETE FROM watched_folders;"));
    QVERIFY(clearQuery.exec("DELETE FROM documents;"));
    QVERIFY(clearQuery.exec("DELETE FROM document_search;"));

    // Create temporary directory
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString tempPath = QFileInfo(tempDir.path()).canonicalFilePath();
    QVERIFY(!tempPath.isEmpty());

    // Create 100 small files to make the scan take some time
    for (int i = 0; i < 100; ++i) {
        QString filePath = QDir(tempPath).filePath(QString("cancelDoc_%1.txt").arg(i));
        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("Just some text to make scanning take time.");
        file.close();
    }

    QSignalSpy spyPostScan(m_controller, &LibraryController::postScanFinished);

    // Add watched folder (top-level) which triggers scan
    QVERIFY(m_controller->addWatchedFolder(tempPath));

    // Wait a tiny bit for scan to start but not finish
    QTest::qWait(10);

    // Remove watched folder immediately, which should trigger cooperative cancellation
    QVERIFY(m_controller->removeWatchedFolder(tempPath));

    // Wait for the scanner to completely finish/abort
    QThreadPool::globalInstance()->waitForDone();
    while (m_controller->isScanning()) {
        QTest::qWait(50);
    }

    if (spyPostScan.isEmpty()) {
        spyPostScan.wait(1000);
    }

    // Verify it doesn't crash, and can be removed safely.
    // Also, trigger active scan cancel / queued tryTake test.
    // Try requesting scan, and then requesting it again immediately.
    QVERIFY(m_controller->addWatchedFolder(tempPath));
    emit m_controller->scanRequested(tempPath); // Re-request immediately, should cancel/queued take

    // Clean up
    QThreadPool::globalInstance()->waitForDone();
    while (m_controller->isScanning()) {
        QTest::qWait(50);
    }
    if (spyPostScan.isEmpty()) {
        spyPostScan.wait(1000);
    }

    QVERIFY(m_controller->removeWatchedFolder(tempPath));
    QThreadPool::globalInstance()->waitForDone();
}

QTEST_MAIN(TestWorkers)
#include "test_workers.moc"
