/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "TileDocument.h"
#include "TileStoreSqlite.h"

#include <pmi_core_defs.h>

#include <QDateTime>
#include <QtTest>

_PMI_BEGIN

class TileDocumentTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testSaveMetaInfo();
};

void TileDocumentTest::testSaveMetaInfo()
{
    const QString heatMapFilePath = QDir(PMI_TEST_FILES_OUTPUT_DIR).filePath("test.db3");

    const QLatin1String customTag("DateTime");
    const MetaInfoEntry msEntry = { TileDocument::TAG_MS_FILE_PATH, "C:\\Test1.raw" };
    const MetaInfoEntry dateEntry = { customTag, QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") };

    {
        TileStoreSqlite store(heatMapFilePath);
        
        TileDocument document;
        QCOMPARE(document.saveMetaInfo(&store.db(), { msEntry, dateEntry }), kNoErr);

        MetaInfoEntry actualMsEntry = { TileDocument::TAG_MS_FILE_PATH, QString() };
        QCOMPARE(document.loadMetaInfo(&store.db(), &actualMsEntry), kNoErr);
        QCOMPARE(actualMsEntry.value, msEntry.value);

        MetaInfoEntry actualDateEntry = { customTag, QString() };
        QCOMPARE(document.loadMetaInfo(&store.db(), &actualDateEntry), kNoErr);
        QCOMPARE(actualDateEntry.value, dateEntry.value);
    }

    QVERIFY(QFile::remove(heatMapFilePath));
}

_PMI_END

QTEST_MAIN(pmi::TileDocumentTest)

#include "TileDocumentTest.moc"
