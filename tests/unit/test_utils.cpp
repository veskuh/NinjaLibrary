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

#include <QTest>
#include <QFileInfo>
#include "../../src/utils/DocUtils.h"

class TestUtils : public QObject
{
    Q_OBJECT
private slots:
    void testIsInsideIgnoredDir();
};

void TestUtils::testIsInsideIgnoredDir()
{
    // 1. Positive cases: directory paths that should be ignored
    QVERIFY(DocUtils::isInsideIgnoredDir("/Users/user/Projects/App.app"));
    QVERIFY(DocUtils::isInsideIgnoredDir("/Users/user/Projects/App.app/Contents"));
    QVERIFY(DocUtils::isInsideIgnoredDir("/Users/user/repo/.git"));
    QVERIFY(DocUtils::isInsideIgnoredDir("/Users/user/repo/.git/objects/pack"));
    QVERIFY(DocUtils::isInsideIgnoredDir("/Users/user/.Trash"));
    QVERIFY(DocUtils::isInsideIgnoredDir("/Users/user/.Trash/subfolder"));
    QVERIFY(DocUtils::isInsideIgnoredDir("/Users/user/Library.photoslibrary"));
    QVERIFY(DocUtils::isInsideIgnoredDir("/Users/user/Library.photoslibrary/resources"));

    // 2. Negative cases: directory paths that should NOT be ignored
    QVERIFY(!DocUtils::isInsideIgnoredDir("/Users/user/Documents"));
    QVERIFY(!DocUtils::isInsideIgnoredDir("/Users/user/Documents/git_project"));
    QVERIFY(!DocUtils::isInsideIgnoredDir("/Users/user/Documents/photos"));
    QVERIFY(!DocUtils::isInsideIgnoredDir("/Users/user/Documents/my.app.folder")); // Not ending in .app, it's .folder

    // 3. File path evaluation with QFileInfo::absolutePath()
    // A file ending in .app.pdf inside a clean directory should NOT be ignored
    QString cleanFile = "/Users/user/Documents/myreport.app.pdf";
    QVERIFY(!DocUtils::isInsideIgnoredDir(QFileInfo(cleanFile).absolutePath()));

    // A file inside an .app directory package SHOULD be ignored
    QString packageFile = "/Users/user/Projects/App.app/Contents/Resources/logo.png";
    QVERIFY(DocUtils::isInsideIgnoredDir(QFileInfo(packageFile).absolutePath()));

    // 4. Case insensitivity and mixed separators checks
    QVERIFY(DocUtils::isInsideIgnoredDir("C:\\Users\\user\\Projects\\App.APP\\Contents"));
    QVERIFY(DocUtils::isInsideIgnoredDir("/Users/user/repo/.GIT/refs"));
    QVERIFY(DocUtils::isInsideIgnoredDir("C:/Users/user/.trash/folder"));
}

QTEST_MAIN(TestUtils)
#include "test_utils.moc"
