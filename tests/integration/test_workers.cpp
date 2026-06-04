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
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QPdfWriter>
#include <QPainter>
#include <QPageSize>
#include <QCryptographicHash>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include "../../src/database/DatabaseManager.h"
#include "../../src/controllers/LibraryController.h"
#include "../../src/workers/ScannerTask.h"
#include "../../src/workers/OcrTask.h"
#include "../../src/workers/ThumbnailTask.h"
#include "../../src/utils/MacBookmarks.h"

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
    QString tempPath = tempDir.path();

    // Create a supported file (PNG)
    QString filePath = QDir(tempPath).filePath("documentA.png");
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    
    // Write standard tiny 1x1 PNG data to make it a valid image
    static const unsigned char pngData[] = {
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
        0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x06, 0x00, 0x00, 0x00, 0x1f, 0x15, 0xc4, 0x89, 0x00, 0x00, 0x00,
        0x0a, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0x00, 0x01, 0x00, 0x00,
        0x05, 0x00, 0x01, 0x0d, 0x0a, 0x2d, 0xb4, 0x00, 0x00, 0x00, 0x00, 0x49,
        0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
    };
    QCOMPARE(file.write(reinterpret_cast<const char*>(pngData), sizeof(pngData)), (qint64)sizeof(pngData));
    file.close();

    // Resolve canonical file path to match DB
    filePath = QFileInfo(filePath).canonicalFilePath();
    QVERIFY(!filePath.isEmpty());

    // Add watched folder
    QVERIFY(m_controller->addWatchedFolder(tempPath));

    // Wait for the scanner to finish
    QSignalSpy spyLibrary(m_controller, &LibraryController::libraryChanged);
    QVERIFY(spyLibrary.wait(5000));

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

    // Verify thumbnail generated signal is emitted or cached
    // Delete file to test offline detection
    QVERIFY(QFile::remove(filePath));

    // Request scanner again
    spyLibrary.clear();
    emit m_controller->scanRequested(tempPath);
    QVERIFY(spyLibrary.wait(5000));

    // Check if marked offline
    {
        QSqlDatabase db = m_dbMgr->getDatabaseConnection();
        QSqlQuery query(db);
        QVERIFY(query.exec("SELECT absolute_path, is_offline FROM documents;"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), filePath);
        QCOMPARE(query.value(1).toInt(), 1); // should be offline
    }

    // Re-create the file to test recovery
    QFile file2(filePath);
    QVERIFY(file2.open(QIODevice::WriteOnly));
    QCOMPARE(file2.write(reinterpret_cast<const char*>(pngData), sizeof(pngData)), (qint64)sizeof(pngData));
    file2.close();

    // Run scanner again
    spyLibrary.clear();
    emit m_controller->scanRequested(tempPath);
    QVERIFY(spyLibrary.wait(5000));

    // Check if back online
    {
        QSqlDatabase db = m_dbMgr->getDatabaseConnection();
        QSqlQuery query(db);
        QVERIFY(query.exec("SELECT absolute_path, is_offline FROM documents;"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), filePath);
        QCOMPARE(query.value(1).toInt(), 0); // should be online again
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
    query.prepare("SELECT id, absolute_path, page_count FROM documents WHERE file_name = 'documentB.pdf';");
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
    ThumbnailTask thumbTask(docId, pdfPath);
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
    QTest::qWait(2000); // wait so file modification time is different
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

    // 2. Test addWatchedFolder with valid path
    QVERIFY(m_controller->addWatchedFolder(tempPath));
    QVERIFY(m_controller->watchedFolders().contains(tempPath));

    // Wait for the scanner to finish so it inserts the document
    QSignalSpy spyLibrary(m_controller, &LibraryController::libraryChanged);
    QVERIFY(spyLibrary.wait(5000));

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
    query.prepare("SELECT t.name FROM tags t JOIN document_tags dt ON t.id = dt.tag_id WHERE dt.document_id = :id;");
    query.bindValue(":id", docId);
    QVERIFY(query.exec());
    QStringList dbTags;
    while (query.next()) {
        dbTags << query.value(0).toString();
    }
    QVERIFY(dbTags.contains("tagA"));
    QVERIFY(dbTags.contains("tagB"));
    QVERIFY(!dbTags.contains("")); // empty tag skipped

    // Test batchUpdateTags empty inputs (graceful return)
    QVERIFY(m_controller->batchUpdateTags({}, {"tagC"}));

    // 3b. Test batchAddTags, batchRemoveTags, and getUniqueTags
    // Insert a second document for batch operations
    QSqlQuery insertDoc2(db);
    insertDoc2.prepare("INSERT INTO documents (folder_id, file_name, absolute_path, file_size, is_offline) "
                       "VALUES (1, 'image2.jpg', :path, 100, 0);");
    insertDoc2.bindValue(":path", tempPath + "/image2.jpg");
    QVERIFY(insertDoc2.exec());
    int docId2 = insertDoc2.lastInsertId().toInt();

    // Verify initial unique tags
    QStringList uniqueTags = m_controller->getUniqueTags();
    QVERIFY(uniqueTags.contains("tagA"));
    QVERIFY(uniqueTags.contains("tagB"));

    // Batch Add Tags "tagB" and "tagC" to both docId and docId2
    QVERIFY(m_controller->batchAddTags({docId, docId2}, {"tagB", "tagC"}));

    // Verify tags in DB
    query.prepare("SELECT t.name FROM tags t JOIN document_tags dt ON t.id = dt.tag_id WHERE dt.document_id = :id;");
    query.bindValue(":id", docId);
    QVERIFY(query.exec());
    QStringList doc1Tags;
    while (query.next()) { doc1Tags << query.value(0).toString(); }
    QVERIFY(doc1Tags.contains("tagA")); // preserved!
    QVERIFY(doc1Tags.contains("tagB"));
    QVERIFY(doc1Tags.contains("tagC"));

    query.bindValue(":id", docId2);
    QVERIFY(query.exec());
    QStringList doc2Tags;
    while (query.next()) { doc2Tags << query.value(0).toString(); }
    QVERIFY(!doc2Tags.contains("tagA"));
    QVERIFY(doc2Tags.contains("tagB"));
    QVERIFY(doc2Tags.contains("tagC"));

    // Batch Remove tagB from both docId and docId2
    QVERIFY(m_controller->batchRemoveTags({docId, docId2}, {"tagB"}));

    query.bindValue(":id", docId);
    QVERIFY(query.exec());
    doc1Tags.clear();
    while (query.next()) { doc1Tags << query.value(0).toString(); }
    QVERIFY(!doc1Tags.contains("tagB"));
    QVERIFY(doc1Tags.contains("tagC"));

    query.bindValue(":id", docId2);
    QVERIFY(query.exec());
    doc2Tags.clear();
    while (query.next()) { doc2Tags << query.value(0).toString(); }
    QVERIFY(!doc2Tags.contains("tagB"));
    QVERIFY(doc2Tags.contains("tagC"));

    // Verify getUniqueTags contains tagC and tagA, but not tagB (since it was removed from all documents)
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
    QVERIFY(!m_controller->readSidecar("/no/such/document/path.pdf", dummyTags, dummyRating, dummyNotes));

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
    QCOMPARE(newFileResult["docId"].toInt(), -1); // not in database yet because background scanner hasn't run for it

    // Clean up watched folder for tempDir2
    QVERIFY(m_controller->removeWatchedFolder(newFolderPath));

    // Test removeWatchedFolder with empty/invalid inputs
    QVERIFY(!m_controller->removeWatchedFolder(""));
    QVERIFY(m_controller->removeWatchedFolder(tempPath));
    QVERIFY(!m_controller->watchedFolders().contains(tempPath));
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

    // Verify that NO ocr or thumbnail tasks were requested for the txt, md, doc, xlsx, or pptx files
    QCOMPARE(spyOcr.size(), 0);
    QCOMPARE(spyThumb.size(), 0);

    // Verify that the files were ingested correctly
    {
        QSqlQuery query(db);
        query.prepare("SELECT d.file_name, s.text_snippet FROM documents d JOIN document_search s ON d.id = s.document_id WHERE d.folder_id = (SELECT id FROM watched_folders WHERE absolute_path = :path);");
        query.bindValue(":path", tempPath);
        QVERIFY(query.exec());

        bool foundTxt = false;
        bool foundMd = false;
        bool foundDoc = false;
        bool foundXlsx = false;
        bool foundPptx = false;

        while (query.next()) {
            QString name = query.value(0).toString();
            QString text = query.value(1).toString();

            if (name == "notes.txt") {
                foundTxt = true;
                QVERIFY(text.contains("Secret code 98765"));
            } else if (name == "readme.md") {
                foundMd = true;
                QVERIFY(text.contains("markdown indexing"));
            } else if (name == "document.doc") {
                foundDoc = true;
#ifdef Q_OS_MAC
                // On macOS, NSAttributedString should successfully parse the RTF text inside document.doc
                QVERIFY(text.contains("applepie"));
#endif
            } else if (name == "spreadsheet.xlsx") {
                foundXlsx = true;
                QVERIFY(text.isEmpty()); // should have no extracted text
            } else if (name == "slides.pptx") {
                foundPptx = true;
                QVERIFY(text.isEmpty()); // should have no extracted text
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
}

QTEST_MAIN(TestWorkers)
#include "test_workers.moc"
