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

#include "PdfUtils.h"
#include <poppler-qt6.h>
#include <memory>
#include <QDebug>

namespace PdfUtils {

    QImage renderPdfThumbnail(const QString &pdfPath, int targetWidth)
    {
        std::unique_ptr<Poppler::Document> doc(Poppler::Document::load(pdfPath));
        if (!doc || doc->isLocked() || doc->numPages() == 0) {
            return QImage();
        }
        std::unique_ptr<Poppler::Page> page(doc->page(0));
        if (!page) return QImage();

        QSize pageSize = page->pageSize();
        if (pageSize.width() <= 0) return QImage();

        double dpi = 72.0 * targetWidth / pageSize.width();
        return page->renderToImage(dpi, dpi);
    }

    QString extractPdfText(const QString &pdfPath)
    {
        std::unique_ptr<Poppler::Document> doc(Poppler::Document::load(pdfPath));
        if (!doc || doc->isLocked()) return QString();

        QString text;
        int pages = doc->numPages();
        for (int i = 0; i < pages; ++i) {
            std::unique_ptr<Poppler::Page> page(doc->page(i));
            if (page) {
                text += page->text(QRectF()) + "\n";
            }
        }
        return text;
    }

    int getPdfPageCount(const QString &pdfPath)
    {
        std::unique_ptr<Poppler::Document> doc(Poppler::Document::load(pdfPath));
        if (!doc || doc->isLocked()) return 0;
        return doc->numPages();
    }
}
