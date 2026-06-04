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

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include "../database/DatabaseManager.h"
#include "../controllers/LibraryController.h"
#include "../models/DocumentModel.h"
#include "../models/ProxyFilter.h"

int main(int argc, char *argv[])
{
    // Enable High DPI scaling
    QGuiApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QGuiApplication app(argc, argv);

    app.setOrganizationName("NinjaLibrary");
    app.setApplicationName("NinjaLibrary");

    // Initialize Database
    DatabaseManager *dbMgr = new DatabaseManager(QString(), &app);
    if (!dbMgr->initializeDatabase()) {
        qCritical() << "Failed to initialize NinjaLibrary database.";
        return -1;
    }

    // Initialize Controller
    LibraryController *controller = new LibraryController(dbMgr, &app);

    // Initialize Models
    DocumentModel *docModel = new DocumentModel(dbMgr, &app);
    ProxyFilter *proxyFilter = new ProxyFilter(dbMgr, &app);
    proxyFilter->setSourceModel(docModel);

    // Connect controller signals to model slots
    QObject::connect(controller, &LibraryController::libraryChanged, docModel, &DocumentModel::refresh, Qt::QueuedConnection);
    QObject::connect(controller, &LibraryController::thumbnailGenerated, docModel, &DocumentModel::updateThumbnail);

    QQmlApplicationEngine engine;

    // Register context properties for QML views
    engine.rootContext()->setContextProperty("libraryController", controller);
    engine.rootContext()->setContextProperty("documentModel", docModel);
    engine.rootContext()->setContextProperty("proxyFilter", proxyFilter);

    // Add Kaakao import path so it is resolved properly
    engine.addImportPath("qrc:/qt/qml");

    const QUrl url(QStringLiteral("qrc:/qt/qml/NinjaLibrary/src/views/MainWindow.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
