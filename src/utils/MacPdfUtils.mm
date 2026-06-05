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
#import <PDFKit/PDFKit.h>
#import <AppKit/AppKit.h>
#include <QDebug>

namespace PdfUtils {

    QImage renderPdfThumbnail(const QString &pdfPath, int targetWidth)
    {
        @autoreleasepool {
            NSString *nsPath = pdfPath.toNSString();
            NSURL *url = [NSURL fileURLWithPath:nsPath];
            if (!url) return QImage();

            PDFDocument *pdfDoc = [[PDFDocument alloc] initWithURL:url];
            if (!pdfDoc || [pdfDoc pageCount] == 0) return QImage();

            PDFPage *page = [pdfDoc pageAtIndex:0];
            NSRect bounds = [page boundsForBox:kPDFDisplayBoxMediaBox];
            if (bounds.size.width <= 0 || bounds.size.height <= 0) return QImage();

            CGFloat scale = (CGFloat)targetWidth / bounds.size.width;
            NSSize targetSize = NSMakeSize(targetWidth, bounds.size.height * scale);

            NSImage *image = [page thumbnailOfSize:targetSize forBox:kPDFDisplayBoxMediaBox];
            if (!image) return QImage();

            CGImageRef cgImage = [image CGImageForProposedRect:NULL context:NULL hints:NULL];
            if (!cgImage) return QImage();

            int width = (int)targetSize.width;
            int height = (int)targetSize.height;

            QImage qimg(width, height, QImage::Format_ARGB32_Premultiplied);
            qimg.fill(Qt::transparent);

            CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
            CGContextRef context = CGBitmapContextCreate(qimg.bits(), width, height, 8, qimg.bytesPerLine(), colorSpace, kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host);
            if (!context) {
                CGColorSpaceRelease(colorSpace);
                return QImage();
            }

            CGContextDrawImage(context, CGRectMake(0, 0, width, height), cgImage);
            CGContextRelease(context);
            CGColorSpaceRelease(colorSpace);

            return qimg;
        }
    }

    QString extractPdfText(const QString &pdfPath)
    {
        @autoreleasepool {
            NSString *nsPath = pdfPath.toNSString();
            NSURL *url = [NSURL fileURLWithPath:nsPath];
            if (!url) return QString();

            PDFDocument *pdfDoc = [[PDFDocument alloc] initWithURL:url];
            if (!pdfDoc) return QString();

            NSString *allText = [pdfDoc string];
            if (!allText) return QString();

            return QString::fromNSString(allText);
        }
    }

    int getPdfPageCount(const QString &pdfPath)
    {
        @autoreleasepool {
            NSString *nsPath = pdfPath.toNSString();
            NSURL *url = [NSURL fileURLWithPath:nsPath];
            if (!url) return 0;

            PDFDocument *pdfDoc = [[PDFDocument alloc] initWithURL:url];
            if (!pdfDoc) return 0;

            return (int)[pdfDoc pageCount];
        }
    }
}
