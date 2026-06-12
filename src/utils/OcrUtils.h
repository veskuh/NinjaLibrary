#ifndef OCRUTILS_H
#define OCRUTILS_H

// Copyright (c) 2026 NinjaLibrary. All rights reserved.
// Licensed under the BSD-3-Clause License.

#include <QImage>
#include <QString>

namespace OcrUtils {

struct OcrResult
{
    QString text;
    int confidence;  // 0-100, matching Tesseract convention
};

// Perform OCR on a pre-processed grayscale QImage.
// Uses macOS Vision framework on Apple platforms, Tesseract on Linux.
OcrResult recognizeText(const QImage &image);

}  // namespace OcrUtils

#endif  // OCRUTILS_H
