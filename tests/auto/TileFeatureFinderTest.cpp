/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include <QtTest>
#include "pmi_core_defs.h"
#include "pmi_common_ms_debug.h"

#include "TileFeatureFinder.h"
#include "MSEquispacedData.h"
#include "TileManager.h"
#include "TilePositionConverter.h"
#include "TileRange.h"
#include "TileStoreSqlite.h"

#include "SequentialTileIterator.h"

_PMI_BEGIN

class TileFeatureFinderTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void testExpandLocalMaxima();

    void testFindLocalMaxima();
    void testFindMinMaxIntensity();
    void testFindMinMaxIntensity_data();

    void testSequentialTileIterator();
};

void TileFeatureFinderTest::testFindLocalMaxima()
{
    QString documentFilePath = R"(p:\PMI-Dev\Share\For_Lukas\From_Lukas\LT-375\2Dmap.db3)";
    if (!QFileInfo(documentFilePath).isReadable()) {
        QSKIP("WIP TEST-Needs small testing heatmap file, remote data ");
    }
    qDebug() << "Start...!";
    QElapsedTimer et;
    et.start();

    int level = 1; // rip-map level 1->(1,1), 2->(2,2)
    int topk = 50000;

    QDir outputDir = QFile::decodeName(PMI_TEST_FILES_OUTPUT_DIR) + QString("/TileFeatureFinderTest/");
    if (!outputDir.exists()) {
        outputDir.mkpath(outputDir.absolutePath());
    }

    QString csvOutput
        = QString("%1_level%2.csv").arg(QFileInfo(documentFilePath).completeBaseName()).arg(level);

    TileFeatureFinder finder(documentFilePath);
    finder.setTopKIntensities(topk);
    finder.setLevelOfInterest(level);
    bool neighborTileVersion = true;
    int foundMaximas = 0;
    if (neighborTileVersion) {
        foundMaximas = finder.findLocalMaximaNG();
    } else {
        foundMaximas = finder.findLocalMaxima();
    }
    
    qDebug() << "Found maximas:" << foundMaximas;

    double mzDistance = 165.5;
    double mzRadius = 0.5; 
    double timeRadius = 0.1;
    double intensityTolerance = 0.10; // 10% 
    
    int filtered = finder.filterLocalMaxima(mzDistance, mzRadius, timeRadius, intensityTolerance, TileFeatureFinder::SearchDirectionRight);
    qDebug() << "Filtered items:" << filtered;

    finder.saveToCsv(outputDir.filePath(csvOutput));

    qDebug() << "Done in" << et.elapsed();
}

void TileFeatureFinderTest::testExpandLocalMaxima()
{
    QString outDir = QString(R"(C:\testdct\)");
    QString documentFilePath = outDir + R"(Test_Trypsin_1.small.db3)";

    if (!QFileInfo(documentFilePath).isReadable()) {
        QString msg = R"(WIP TEST-Needs small testing heatmap file: )" + documentFilePath;
        QSKIP(msg.toUtf8()); // Does not accept a QString
    }
    qDebug() << "Starting...!";
    QElapsedTimer et;
    et.start();

    int level = 1; // rip-map level 1->(1,1), 2->(2,2)
    int topk = 50000;

    QDir outputDir
        = QFile::decodeName(PMI_TEST_FILES_OUTPUT_DIR) + QString("/TileFeatureFinderTest/");
    if (!outputDir.exists()) {
        outputDir.mkpath(outputDir.absolutePath());
    }

    QString csvOutput
        = QString("%1_level%2.csv").arg(QFileInfo(documentFilePath).completeBaseName()).arg(level);

    TileFeatureFinder finder(documentFilePath);

    finder.setTopKIntensities(topk);
    finder.setLevelOfInterest(level);

    int foundMaximas = finder.findLocalMaximaNG();

    qDebug() << "Found maximas:" << foundMaximas;

    finder.initializePeakArray();
    finder.updateTimeMzRanges();
    finder.associatePeaks();

    qDebug() << "Found maximas after additional analysis:" << foundMaximas;

    bool showOriginal = true;
    bool showAll = false;
    
    // Write out the annotations
    finder.saveToCsv(outputDir.filePath(csvOutput), showOriginal, showAll);

    // Organize and save the raw data to a csv file - if so desired
    // TODO:: could move this to a separate test
    QDir rawOutputDir(outDir);
    if (!rawOutputDir.exists()) {
        rawOutputDir.mkpath(rawOutputDir.absolutePath());
    }
    QString rawCsvOutput = QString("%1_level%2.rawdata.csv")
                               .arg(QFileInfo(documentFilePath).completeBaseName())
                               .arg(level);
    MSEquispacedData rawData;
    finder.allData(&rawData);
    rawData.saveToCsv(rawOutputDir.filePath(rawCsvOutput));

    qDebug() << "Done in" << et.elapsed();
}

void TileFeatureFinderTest::testFindMinMaxIntensity()
{
    QString documentFilePath = R"(P:\PMI-Dev\JIRA\LT-641\020215_DM_BioS3_trp_HCDETD.db3)";
    if (!QFileInfo(documentFilePath).isReadable()) {
        QSKIP("WIP TEST-Needs small testing heatmap file stored on Dropbox!");
    }
    
    qDebug() << "Start...!";
    QElapsedTimer et;
    et.start();

    TileFeatureFinder finder(documentFilePath);
    double min = 0;
    double max = 0;

    QFETCH(bool, useFastSequentialIterator);

    finder.findMinMaxIntensity(&min, &max, useFastSequentialIterator);

    QVERIFY(qFuzzyCompare(min, 0));
    QVERIFY(qFuzzyCompare(max, 193748255.108443945646));
    
    qDebug() << "Done in" << et.elapsed();
}

void TileFeatureFinderTest::testFindMinMaxIntensity_data()
{
    QTest::addColumn<bool>("useFastSequentialIterator");

    QTest::newRow("Threaded-RandomTileIterator") << false;
    QTest::newRow("Threaded-SequentialTileIterator") << true;
}

void TileFeatureFinderTest::testSequentialTileIterator()
{
    QString documentFilePath = R"(P:\PMI-Dev\JIRA\LT-641\020215_DM_BioS3_trp_HCDETD.db3)";
    qDebug() << "Document file path:" << documentFilePath;
    if (!QFileInfo(documentFilePath).isReadable()) {
        QSKIP("WIP TEST-Needs small testing heatmap file stored on Dropbox!");
    }

    QElapsedTimer et;
    et.start();

    TileStoreSqlite store(documentFilePath);
    TileRange range;
    TileRange::loadRange(&store.db(), &range);

    TilePositionConverter converter(range);
    converter.setCurrentLevel(QPoint(1, 1));

    // this allows to set some offset in tile coordinates
    int xOffset = 0;
    int yOffset = 0;

    int l = converter.globalTileX(range.minX() + range.stepX() * xOffset);
    int r = converter.globalTileX(range.maxX());
    int t = converter.scanIndexAt(range.minY() + range.stepY() * yOffset);
    int b = converter.scanIndexAt(range.maxY());

    QRect tileArea = QRect(QPoint(l, t), QPoint(r, b));
    qDebug() << "Tile Area" << tileArea;
    TileManager manager(&store);

    SequentialTileIterator iterator(&manager, tileArea, QPoint(1, 1));

//#define DEBUG_TILE_POS_ACCESS
#ifdef DEBUG_TILE_POS_ACCESS
    QRect rc = TileManager::normalizeRect(tileArea);
    QSize imageSize(rc.right(), rc.bottom());

    // image
    QImage visited = QImage(imageSize, QImage::Format_ARGB32);
    visited.fill(Qt::magenta);

    QColor baseColor(Qt::white);
    qreal h;
    qreal s;
    qreal v;

    baseColor.getHsvF(&h, &s, &v);
    int colorIndex = 0;
#endif

    double min = 0;
    double max = 0;
    while (iterator.hasNext()) {
        double intensity = iterator.next();
        if (intensity > max) {
            max = intensity;
        }

        if (intensity < min) {
            min = intensity;
        }

#ifdef DEBUG_TILE_POS_ACCESS
        const int x = iterator.x();
        const int y = iterator.y();

        qreal colorValue = (colorIndex % 11) / qreal(11);

        QColor pixelColor = baseColor;
        pixelColor.setHsvF(h, s, colorValue);

        visited.setPixel(x, y, pixelColor.rgb());

        colorIndex++;
#endif
    }

    qDebug() << "min" << min;
    qDebug() << "max" << max;

#ifdef DEBUG_TILE_POS_ACCESS
    qDebug() << "Saving";
    visited.save("D:/tileTest.bmp");
#endif

    qDebug() << "Done in" << et.elapsed() << "ms";
}

_PMI_END

QTEST_MAIN(pmi::TileFeatureFinderTest)


#include "TileFeatureFinderTest.moc"
