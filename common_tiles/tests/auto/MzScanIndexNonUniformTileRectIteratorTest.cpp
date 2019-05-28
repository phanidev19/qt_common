/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

// common_tiles
#include "MzScanIndexNonUniformTileRectIterator.h"
#include "MzScanIndexRect.h"
#include "NonUniformTilePartIterator.h"
#include "NonUniformTileStore.h"
#include "NonUniformTileStoreSqlite.h"
#include "SequentialNonUniformTileIterator.h"

// common_ms
#include "MSDataNonUniform.h"
#include <MSDataNonUniformAdapter.h>
#include <MSReader.h>
#include <MSReaderInfo.h>
#include <ScanIndexNumberConverter.h>

// common_core_mini
#include <CacheFileManagerInterface.h>
#include <PMiTestUtils.h>
#include <Point2dListUtils.h>

#include <pmi_core_defs.h>

#include <QtTest>


_PMI_BEGIN

static const QString MSFILE = "cona_tmt0saxpdetd.raw";
static const QString MSFILE_NON_UNIFORM_CACHE = "cona_tmt0saxpdetd.raw.NonUniform.cache";

class MzScanIndexNonUniformTileRectIteratorTest : public QObject
{
    Q_OBJECT

public:
    MzScanIndexNonUniformTileRectIteratorTest(const QStringList &args);

private Q_SLOTS:
    void testSequentialNonUniformTileIteratorFindMinMax();
    void testSequentialNonUniformTileIteratorXIC();
    void testIterateOverSingleScan();

private:
    QDir m_testDataBasePath;
};

MzScanIndexNonUniformTileRectIteratorTest::MzScanIndexNonUniformTileRectIteratorTest(const QStringList &args)
    : m_testDataBasePath(args[0])
{
}

void MzScanIndexNonUniformTileRectIteratorTest::testSequentialNonUniformTileIteratorFindMinMax()
{
    const QString msFilePath = m_testDataBasePath.filePath(MSFILE);

    MSReader *reader = MSReader::Instance();
    QCOMPARE(reader->openFile(msFilePath), kNoErr);

    QString nonUniformFilePath;
    const CacheFileManagerInterface* cacheManager = reader->cacheFileManager();
    QCOMPARE(cacheManager->findOrCreateCachePath(MSDataNonUniformAdapter::formatSuffix(), &nonUniformFilePath), kNoErr);

    MSDataNonUniformAdapter document(nonUniformFilePath);

    QVERIFY(QFileInfo(document.cacheFilePath()).exists());

    QList<msreader::ScanInfoWrapper> scanInfo;
    QCOMPARE(reader->getScanInfoListAtLevel(1, &scanInfo), kNoErr);

    QCOMPARE(document.load(scanInfo), kNoErr);

    bool doCentroiding = document.hasData(NonUniformTileStore::ContentMS1Centroided);

    const NonUniformTileRange &range = document.range();

    int firstTile = range.tileX(range.mzMin());
    int lastTile = range.tileX(range.mzMax());
    int pixelWidth = lastTile - firstTile + 1;

    int firstScan = range.area().top();
    int lastScan = range.area().bottom();
    int pixelHeight = lastScan - firstScan + 1;

    MzScanIndexRect area = MzScanIndexRect::fromQRectF(range.area());

    double min = 0.0;
    double max = 0.0;

    // find minimum and maximum in NonUniform tiles
    QElapsedTimer et1;
    et1.start();
    
    NonUniformTileManager manager(document.store());
    MzScanIndexNonUniformTileRectIterator iterator(&manager, range, doCentroiding, area);
    bool minMaxInitialized = false;
    quint32 visitedPoints = 0;
    while (iterator.hasNext()) {
        point2dList data = iterator.next();

        NonUniformTilePartIterator tilePartIterator(data, iterator.mzStart(), iterator.mzEnd());
        while (tilePartIterator.hasNext()) {
            point2d pt = tilePartIterator.next();
            if (!minMaxInitialized) {
                min = pt.y();
                max = pt.y();
                minMaxInitialized = true;
            }

            min = qMin(pt.y(), min);
            max = qMax(pt.y(), max);

            visitedPoints++;
        }
    }
    qDebug() << "min, max found in tiles in" << et1.elapsed() << "ms";

    // expected
    et1.restart();
    MSReaderInfo msInfo(reader);
    msInfo.enableCentroiding(doCentroiding);
    double expectedMin = msInfo.fetchMinIntensityStats().min;
    double expectedMax = msInfo.fetchMaxIntensityStats().max;
    qDebug() << "min, max found in vendor in" << et1.elapsed() << "ms";

    quint32 expectedPointCount = msInfo.pointCount(1, doCentroiding);
    QCOMPARE(visitedPoints, expectedPointCount);

    QCOMPARE(min, expectedMin);
    QCOMPARE(max, expectedMax);
}

void MzScanIndexNonUniformTileRectIteratorTest::testSequentialNonUniformTileIteratorXIC()
{
    // this test fetches XIC over the whole spectrum of the sample in MS1 space
    // using MSReader::getXICData, MSDataNonUniformAdapter::getXICData and
    // MSDataNonUniform::getXICDataMzScanIndexIterator to verify correct functionality of
    // MzScanIndexNonUniformTileRectIterator.

    // Note: test provides meta data about auto test: running times, but they are heavily biased by
    // the fact that sqlite database gets accessed consequentially and thus order of execution has
    // impact on the resulting running times due caching, i.e. this is not proper performance test 

    // if the CSV files of all results should be dumped
    bool dumpThem = false;
    
    // init MSDataNonUniformAdapter
    const QString msFilePath = m_testDataBasePath.filePath(MSFILE);

    MSReader *reader = MSReader::Instance();
    QCOMPARE(reader->openFile(msFilePath), kNoErr);

    QString nonUniformFilePath;
    const CacheFileManagerInterface *cacheManager = reader->cacheFileManager();
    QCOMPARE(cacheManager->findOrCreateCachePath(MSDataNonUniformAdapter::formatSuffix(), &nonUniformFilePath), kNoErr);

    MSDataNonUniformAdapter document(nonUniformFilePath);

    bool ok = false;
    NonUniformTileRange range = document.loadRange(&ok);
    QVERIFY(ok);
    
    // build testing XIC window
    QList<msreader::ScanInfoWrapper> scanInfo;
    QCOMPARE(reader->getScanInfoListAtLevel(1, &scanInfo), kNoErr);

    msreader::XICWindow win;
    win.mz_start = range.mzMin();
    win.mz_end = range.mzMax();
    win.time_start = scanInfo.first().scanInfo.retTimeMinutes;
    win.time_end = scanInfo.last().scanInfo.retTimeMinutes;

    qDebug() << "XIC window:";
    qDebug() << "mz:" << win.mz_start << "," << win.mz_end;
    qDebug() << "time:" << win.time_start << "," << win.time_end;

    QElapsedTimer et;

    // get XIC from Thermo reader
    et.start();
    point2dList readerExpected;
    QCOMPARE(reader->getXICData(win, &readerExpected, 1), kNoErr);
    qDebug() << "MSReader::getXICData done in" << et.elapsed() << "ms";

    // get XIC from NonUniform
    QCOMPARE(document.load(scanInfo), kNoErr);

    // get XIC for whole file
    // fine-tuned version
    et.restart();
    point2dList expectedUniform;
    document.getXICData(win, &expectedUniform, 1);
    qDebug() << "MSDataNonUniformAdapter::getXICData done in" << et.elapsed() << "ms";

    // New SequentialIterator version
    et.restart();
    point2dList sequentialXICData;
    document.nonUniformData()->getXICDataMzScanIndexIterator(win, &sequentialXICData);
    qDebug() << "MSDataNonUniform::getXICDataMzScanIndexIterator done in" << et.elapsed() << "ms";
    
    bool dataDifferent = (sequentialXICData != expectedUniform);
    if (dataDifferent || dumpThem) {
        if (dataDifferent) {
            qWarning() << "Sequential data are different from expected uniform XIC";
        }

        PlotBase(sequentialXICData).saveDataToFile(m_testDataBasePath.filePath("SequentialIterator_getXICData.csv"));
        PlotBase(expectedUniform).saveDataToFile(m_testDataBasePath.filePath("MSDataNonUniformAdapter_getXICData.csv"));
        PlotBase(readerExpected).saveDataToFile(m_testDataBasePath.filePath("MSReader_getXICData.csv"));
    }
    QCOMPARE(sequentialXICData, expectedUniform);

    dataDifferent = sequentialXICData != readerExpected;
    if (dataDifferent || dumpThem) {
        if (dataDifferent) {
            qWarning() << "Sequential data are different from Thermo reader XIC";
        }

        PlotBase(sequentialXICData).saveDataToFile(m_testDataBasePath.filePath("SequentialIterator_getXICData.csv"));
        PlotBase(expectedUniform).saveDataToFile(m_testDataBasePath.filePath("MSDataNonUniformAdapter_getXICData.csv"));
        PlotBase(readerExpected).saveDataToFile(m_testDataBasePath.filePath("MSReader_getXICData.csv"));
    }
    QCOMPARE(sequentialXICData, readerExpected);
}

void MzScanIndexNonUniformTileRectIteratorTest::testIterateOverSingleScan()
{
    // boiler-plate code to load document
    const QString msFilePath = m_testDataBasePath.filePath(MSFILE);

    MSReader *reader = MSReader::Instance();
    QCOMPARE(reader->openFile(msFilePath), kNoErr);

    QString nonUniformFilePath;
    const CacheFileManagerInterface* cacheManager = reader->cacheFileManager();
    QCOMPARE(cacheManager->findOrCreateCachePath(MSDataNonUniformAdapter::formatSuffix(), &nonUniformFilePath), kNoErr);

    MSDataNonUniformAdapter document(nonUniformFilePath);

    QVERIFY(QFileInfo(document.cacheFilePath()).exists());

    QList<msreader::ScanInfoWrapper> scanInfo;
    QCOMPARE(reader->getScanInfoListAtLevel(1, &scanInfo), kNoErr);

    QCOMPARE(document.load(scanInfo), kNoErr);

    NonUniformTileManager manager(document.store());

    bool doCentroiding = document.hasData(NonUniformTileStore::ContentMS1Centroided);
    
    // test first, last scan and some scan in the middle
    QVector<int> scanIndexes = { 0, 2751, 1000 };
    for (int scanIndex : scanIndexes) {

        MzScanIndexRect area;
        area.mz.rstart() = 380.0;
        area.mz.rend() = 1700.0;

        area.scanIndex.rstart() = scanIndex;
        area.scanIndex.rend() = scanIndex;

        MzScanIndexNonUniformTileRectIterator iterator(&manager, document.range(), doCentroiding, area);

        point2dList scan;
        while (iterator.hasNext()) {
            point2dList data = iterator.next();
            scan.insert(scan.end(), data.begin(), data.end());
        }

        point2dList expectedScan;
        long scanNumber = document.nonUniformData()->converter().toScanNumber(scanIndex);
        reader->getScanData(scanNumber, &expectedScan, doCentroiding);
        expectedScan = Point2dListUtils::extractPointsIncluding(expectedScan, area.mz.start(), area.mz.end());

        QCOMPARE(scan, expectedScan);
    }
}

_PMI_END

PMI_TEST_GUILESS_MAIN_WITH_ARGS(pmi::MzScanIndexNonUniformTileRectIteratorTest,
                                QStringList() << "Remote Data Folder")

#include "MzScanIndexNonUniformTileRectIteratorTest.moc"
