// Copyright (c) 2026 NinjaLibrary. All rights reserved.
// Licensed under the BSD-3-Clause License.

#include <tesseract/baseapi.h>

#include <QColorSpace>
#include <QCoreApplication>
#include <QDebug>
#include <QFile>

#include "OcrUtils.h"

namespace OcrUtils {

OcrResult recognizeText(const QImage &image)
{
    OcrResult result;
    result.confidence = 0;

    if (image.isNull()) {
        return result;
    }

    // Ensure grayscale for Tesseract without triggering non-thread-safe color space conversions
    QImage grayImg = image;
    grayImg.setColorSpace(QColorSpace());
    if (grayImg.format() != QImage::Format_Grayscale8) {
        grayImg = grayImg.convertToFormat(QImage::Format_Grayscale8);
    }

    tesseract::TessBaseAPI *api = new tesseract::TessBaseAPI();

    // Check standard paths; default Init uses TESSDATA_PREFIX or system paths
    bool initialized = (api->Init(nullptr, "eng") == 0);

    if (!initialized) {
        qWarning() << "OcrUtils: Could not initialize Tesseract OCR engine.";
        delete api;
        return result;
    }

    api->SetImage(grayImg.bits(), grayImg.width(), grayImg.height(), 1, grayImg.bytesPerLine());

    char *outText = api->GetUTF8Text();
    result.text = QString::fromUtf8(outText).trimmed();
    result.confidence = api->MeanTextConf();

    delete[] outText;
    api->End();
    delete api;

    return result;
}

}  // namespace OcrUtils
