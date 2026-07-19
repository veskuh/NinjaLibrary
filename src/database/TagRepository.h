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

#ifndef TAGREPOSITORY_H
#define TAGREPOSITORY_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>

class TagRepository
{
public:
    static bool ensureTagLinked(QSqlDatabase &db, int docId, const QString &tagName)
    {
        QString trimmed = tagName.trimmed();
        if (trimmed.isEmpty()) {
            return true;
        }

        QSqlQuery insertTag(db);
        insertTag.prepare("INSERT OR IGNORE INTO tags (name) VALUES (:name);");
        insertTag.bindValue(":name", trimmed);
        if (!insertTag.exec()) {
            qWarning() << "TagRepository: Failed to insert tag:" << insertTag.lastError().text();
            return false;
        }

        QSqlQuery getTagId(db);
        getTagId.prepare("SELECT id FROM tags WHERE name = :name;");
        getTagId.bindValue(":name", trimmed);
        if (!getTagId.exec() || !getTagId.next()) {
            qWarning() << "TagRepository: Failed to retrieve tag ID:" << getTagId.lastError().text();
            return false;
        }
        int tagId = getTagId.value(0).toInt();

        QSqlQuery linkTag(db);
        linkTag.prepare(
            "INSERT OR IGNORE INTO document_tags (document_id, tag_id) VALUES (:docId, :tagId);");
        linkTag.bindValue(":docId", docId);
        linkTag.bindValue(":tagId", tagId);
        if (!linkTag.exec()) {
            qWarning() << "TagRepository: Failed to link tag to document:" << linkTag.lastError().text();
            return false;
        }

        return true;
    }
};

#endif // TAGREPOSITORY_H
