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

#include "MacBookmarks.h"
#import <Foundation/Foundation.h>
#include <QDebug>

namespace MacBookmarks {
    QByteArray getBookmarkForUrl(const QString &urlPath)
    {
        @autoreleasepool {
            NSString *nsPath = urlPath.toNSString();
            NSURL *url = [NSURL fileURLWithPath:nsPath];
            if (!url) {
                return QByteArray();
            }
            
            NSError *error = nil;
            NSData *bookmarkData = [url bookmarkDataWithOptions:NSURLBookmarkCreationWithSecurityScope
                                 includingResourceValuesForKeys:nil
                                                  relativeToURL:nil
                                                          error:&error];
            if (error || !bookmarkData) {
                qWarning() << "Failed to create security scoped bookmark:" 
                           << (error ? QString::fromNSString(error.localizedDescription) : "unknown error");
                return QByteArray();
            }
            
            return QByteArray(reinterpret_cast<const char*>(bookmarkData.bytes), bookmarkData.length);
        }
    }

    bool resolveBookmark(const QByteArray &bookmarkData, QString &resolvedPath)
    {
        @autoreleasepool {
            NSData *nsData = [NSData dataWithBytes:bookmarkData.constData() length:bookmarkData.length()];
            BOOL isStale = NO;
            NSError *error = nil;
            NSURL *resolvedUrl = [NSURL URLByResolvingBookmarkData:nsData
                                                           options:NSURLBookmarkResolutionWithSecurityScope
                                                     relativeToURL:nil
                                               bookmarkDataIsStale:&isStale
                                                             error:&error];
            if (error || !resolvedUrl) {
                qWarning() << "Failed to resolve security scoped bookmark:"
                           << (error ? QString::fromNSString(error.localizedDescription) : "unknown error");
                return false;
            }
            
            BOOL ok = [resolvedUrl startAccessingSecurityScopedResource];
            if (!ok) {
                qWarning() << "Failed to start accessing security scoped resource for path:" 
                           << QString::fromNSString(resolvedUrl.path);
            }
            
            resolvedPath = QString::fromNSString(resolvedUrl.path);
            return true;
        }
    }
}
