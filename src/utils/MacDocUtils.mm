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

#include "DocUtils.h"
#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QDebug>

namespace DocUtils {

bool isSupportedTextDocument(const QString &filePath)
{
    QString ext = QFileInfo(filePath).suffix().toLower();
    return ext == "txt" || ext == "md" || ext == "doc" || ext == "docx" ||
           ext == "xls" || ext == "xlsx" || ext == "ppt" || ext == "pptx";
}

QString extractText(const QString &filePath)
{
    QString ext = QFileInfo(filePath).suffix().toLower();
    
    if (ext == "txt" || ext == "md") {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return QString();
        }
        QTextStream stream(&file);
        return stream.readAll();
    }
    
    if (ext == "doc" || ext == "docx") {
        @autoreleasepool {
            NSString *nsPath = filePath.toNSString();
            NSURL *url = [NSURL fileURLWithPath:nsPath];
            NSError *error = nil;
            NSDictionary *docAttrs = nil;
            
            NSAttributedString *attrStr = [[NSAttributedString alloc] initWithURL:url
                                                                          options:@{}
                                                               documentAttributes:&docAttrs
                                                                            error:&error];
            if (attrStr) {
                NSString *plainText = [attrStr string];
                return QString::fromNSString(plainText);
            } else {
                qWarning() << "DocUtils: Failed to extract text from" << filePath
                           << ":" << (error ? [error localizedDescription].UTF8String : "Unknown error");
                return QString();
            }
        }
    }
    
    return QString();
}

} // namespace DocUtils
