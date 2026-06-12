// Copyright (c) 2026 NinjaLibrary. All rights reserved.
// Licensed under the BSD-3-Clause License.

#include "OcrUtils.h"

#import <CoreGraphics/CoreGraphics.h>
#import <Vision/Vision.h>
#include <QDebug>

#include <QColorSpace>

namespace OcrUtils {

OcrResult recognizeText(const QImage &image)
{
    OcrResult result;
    result.confidence = 0;

    if (image.isNull()) {
        return result;
    }

    @autoreleasepool {
        // Convert QImage to CGImage after clearing color space profile metadata
        // to bypass non-thread-safe QColorTransform conversions.
        QImage rgbImage = image;
        rgbImage.setColorSpace(QColorSpace());
        rgbImage = rgbImage.convertToFormat(QImage::Format_RGBA8888);
        CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
        CGContextRef context = CGBitmapContextCreate((void *)rgbImage.bits(), rgbImage.width(),
                                                     rgbImage.height(), 8, rgbImage.bytesPerLine(),
                                                     colorSpace, kCGImageAlphaPremultipliedLast);

        if (!context) {
            CGColorSpaceRelease(colorSpace);
            qWarning() << "OcrUtils: Failed to create CGBitmapContext for Vision OCR.";
            return result;
        }

        CGImageRef cgImage = CGBitmapContextCreateImage(context);
        CGContextRelease(context);
        CGColorSpaceRelease(colorSpace);

        if (!cgImage) {
            qWarning() << "OcrUtils: Failed to create CGImage for Vision OCR.";
            return result;
        }

        // Create Vision request handler
        VNImageRequestHandler *handler = [[VNImageRequestHandler alloc] initWithCGImage:cgImage
                                                                                options:@{}];

        // Create text recognition request
        __block NSMutableArray<NSString *> *recognizedStrings = [NSMutableArray array];
        __block CGFloat totalConfidence = 0.0;
        __block NSInteger observationCount = 0;

        VNRecognizeTextRequest *request = [[VNRecognizeTextRequest alloc]
            initWithCompletionHandler:^(VNRequest *req, NSError *error) {
              if (error) {
                  qWarning() << "OcrUtils: Vision OCR error:"
                             << QString::fromNSString(error.localizedDescription);
                  return;
              }

              for (VNRecognizedTextObservation *observation in req.results) {
                  VNRecognizedText *candidate = [[observation topCandidates:1] firstObject];
                  if (candidate) {
                      [recognizedStrings addObject:candidate.string];
                      totalConfidence += candidate.confidence;
                      observationCount++;
                  }
              }
            }];

        request.recognitionLevel = VNRequestTextRecognitionLevelAccurate;
        request.usesLanguageCorrection = YES;

        // Perform the request synchronously
        NSError *error = nil;
        [handler performRequests:@[ request ] error:&error];

        CGImageRelease(cgImage);

        if (error) {
            qWarning() << "OcrUtils: Vision request failed:"
                       << QString::fromNSString(error.localizedDescription);
            return result;
        }

        // Assemble result
        if (observationCount > 0) {
            NSString *joinedText = [recognizedStrings componentsJoinedByString:@"\n"];
            result.text = QString::fromNSString(joinedText).trimmed();
            // Scale confidence from 0.0-1.0 to 0-100 to match Tesseract convention
            result.confidence = static_cast<int>((totalConfidence / observationCount) * 100.0);
        }
    }

    return result;
}

}  // namespace OcrUtils
