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

#ifndef DOCUTILS_H
#define DOCUTILS_H

#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QMap>

namespace DocUtils {
// Returns a human-friendly description of the file type based on extension
inline QString fileTypeDescription(const QString &fileName)
{
    int lastDot = fileName.lastIndexOf('.');
    if (lastDot < 0) {
        return "Unknown File";
    }
    int lastSlash = fileName.lastIndexOf('/');
    int lastBack = fileName.lastIndexOf('\\');
    int lastSeparator = (lastSlash > lastBack) ? lastSlash : lastBack;
    if (lastDot <= lastSeparator) {
        return "Unknown File";
    }
    QString ext = fileName.mid(lastDot + 1).toLower();

    static const QMap<QString, QString> types = {
        {"pdf", "PDF Document"},
        {"doc", "Word Document"},
        {"docx", "Word Document"},
        {"xls", "Excel Spreadsheet"},
        {"xlsx", "Excel Spreadsheet"},
        {"ppt", "PowerPoint Presentation"},
        {"pptx", "PowerPoint Presentation"},
        {"txt", "Plain Text File"},
        {"md", "Markdown Document"},
        {"png", "PNG Image"},
        {"jpg", "JPEG Image"},
        {"jpeg", "JPEG Image"},
        {"gif", "GIF Image"},
        {"bmp", "BMP Image"},
        {"tiff", "TIFF Image"},
        {"tif", "TIFF Image"},
        {"rtf", "Rich Text Format"},
        {"html", "HTML Document"},
        {"htm", "HTML Document"},
        {"json", "JSON Document"},
        {"csv", "Comma-Separated Values File"},
        {"xml", "XML Document"},
        {"zip", "ZIP Archive"},
        {"tar", "TAR Archive"},
        {"gz", "GZIP Archive"}
    };

    auto it = types.find(ext);
    if (it != types.end()) {
        return it.value();
    }
    return ext.toUpper() + " File";
}
// Returns true if the directory path (or any of its parents) matches macOS package/VCS/Trash structures
inline bool isInsideIgnoredDir(const QString &dirPath)
{
    QStringList segments = dirPath.split(QRegularExpression("[/\\\\]"), Qt::SkipEmptyParts);
    for (const QString &segment : segments) {
        if (segment.endsWith(".app", Qt::CaseInsensitive) ||
            segment.endsWith(".photoslibrary", Qt::CaseInsensitive) ||
            segment.endsWith(".photolibrary", Qt::CaseInsensitive) ||
            segment.endsWith(".migratedphotolibrary", Qt::CaseInsensitive) ||
            segment.endsWith(".framework", Qt::CaseInsensitive) ||
            segment.endsWith(".bundle", Qt::CaseInsensitive) ||
            segment.endsWith(".xcodeproj", Qt::CaseInsensitive) ||
            segment.endsWith(".pages", Qt::CaseInsensitive) ||
            segment.endsWith(".numbers", Qt::CaseInsensitive) ||
            segment.endsWith(".key", Qt::CaseInsensitive) ||
            segment.endsWith(".wdgt", Qt::CaseInsensitive) ||
            segment.endsWith(".plugin", Qt::CaseInsensitive) ||
            segment.endsWith(".appex", Qt::CaseInsensitive) ||
            segment.endsWith(".scnassets", Qt::CaseInsensitive) ||
            segment.endsWith(".xcassets", Qt::CaseInsensitive) ||
            segment.toLower() == ".git" ||
            segment.toLower() == ".svn" || segment.toLower() == ".trash") {
            return true;
        }
    }
    return false;
}
// Returns true if the file extension is supported for text extraction
bool isSupportedTextDocument(const QString &filePath);

// Extracts plain text from the document at filePath
QString extractText(const QString &filePath);

// Copies plain text to the system clipboard
void copyToClipboard(const QString &text);
}  // namespace DocUtils

#endif  // DOCUTILS_H
