/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include <QtTest>
#include "pmi_core_defs.h"

#include "NonUniformTileStoreSqlite.h"

#include "MSDataNonUniformAdapter.h"
#include "MSReader.h"
#include "MSReaderInfo.h"
#include "NonUniformTileFeatureFinder.h"
#include "NonUniformTilePartIterator.h"
#include "RandomNonUniformTileIterator.h"
#include "SequentialNonUniformTileIterator.h"

_PMI_BEGIN

class NonUniformTileFeatureFinderTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testFindLocalMaxima();


};


void NonUniformTileFeatureFinderTest::testFindLocalMaxima()
{
    QString documentFilePath = R"(c:\data\TestMab.CID_Trypsin_1.raw.NonUniform.cache)";
    if (!QFileInfo(documentFilePath).isReadable()) {
        QSKIP("WIP TEST-Needs small testing heatmap file,TODO: remote data ");
    }
    
    QDir outputDir = QFile::decodeName(PMI_TEST_FILES_OUTPUT_DIR) + QString("/NonUniformTileFeatureFinderTest/");
    if (!outputDir.exists()) {
        outputDir.mkpath(outputDir.absolutePath());
    }

    QString csvOutput = QString("%1.csv").arg(QFileInfo(documentFilePath).completeBaseName());

    qDebug() << "Start...!";
    QElapsedTimer et;
    et.start();

    int topk = 50000;
    
    MSReader *reader = MSReader::Instance();
    Err e = reader->openFile(R"(c:\data\TestMab.CID_Trypsin_1.raw)");
    QCOMPARE(e, kNoErr);

    QList<msreader::ScanInfoWrapper> scanInfo;
    e = reader->getScanInfoListAtLevel(1, &scanInfo);
    QCOMPARE(e, kNoErr);

    MSDataNonUniformAdapter document(documentFilePath);
    e = document.load(scanInfo);
    QCOMPARE(e, kNoErr);

    NonUniformTileFeatureFinder finder(&document);
    finder.setTopKIntensities(topk);
    finder.findLocalMaxima();
    finder.saveToCsv(outputDir.filePath(csvOutput));

    qDebug() << "Done in" << et.elapsed() << "ms";
}


_PMI_END

QTEST_MAIN(pmi::NonUniformTileFeatureFinderTest)



#include "NonUniformTileFeatureFinderTest.moc"
