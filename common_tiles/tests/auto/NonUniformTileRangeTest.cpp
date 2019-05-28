#include <QtTest>
#include "pmi_core_defs.h"
#include "NonUniformTileRange.h"

#include "MzScanIndexRect.h"

_PMI_BEGIN

class NonUniformTileRangeTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testTileCount();
    void testConversions();
    void testTileXConfigurations();
    void testTileX();
    void testNull();
    void testContains();

    void testTileRect();
};

struct RangeConfig {
    double mzMin;
    double mzMax;
    double mzTileWidth;

    int scanIndexMin;
    int scanIndexMax;
    int scanIndexTileHeight;
};

void NonUniformTileRangeTest::testTileCount()
{
    NonUniformTileRange range;
    range.setMzTileLength(5.0);

    range.setMz(0.0, 20.0);
    QCOMPARE(range.tileCountX(), 5);

    range.setMz(0.0, 19.99);
    QCOMPARE(range.tileCountX(), 5);

    range.setMz(0.0, 21.0);
    QCOMPARE(range.tileCountX(), 5);

    range.setMz(0.0, 25.0);
    QCOMPARE(range.tileCountX(), 6);

    range.setScanIndexLength(5);

    range.setScanIndex(0, 20); 
    QCOMPARE(range.tileCountY(), 5);

    range.setScanIndex(0, 19);
    QCOMPARE(range.tileCountY(), 4);

    range.setScanIndex(0, 21);
    QCOMPARE(range.tileCountY(), 5);

    range.setScanIndex(0, 25);
    QCOMPARE(range.tileCountY(), 6);
}

void NonUniformTileRangeTest::testConversions()
{
    NonUniformTileRange range;
    range.setMz(195.11802005248208, 1515.1351516368195);
    range.setMzTileLength(30.0);

    range.setScanIndex(0, 63);
    range.setScanIndexLength(64);

    int tiles = range.tileCountX();
    
    int failed = 0;
    for (int i = 0; i < tiles; ++i) {
        double mz = range.mzAt(i);

        int tileX = range.tileX(mz);
        
        if (tileX != i){
            qDebug() << "Failed at" << i << "with mz" << mz << "wrong index:" << tileX;
            failed++;
        }
    } 

    QVERIFY(failed == 0);
}

void NonUniformTileRangeTest::testTileXConfigurations()
{
    QVector<RangeConfig> configs = {
        // mzMin, mzMax, mzTileWidth, scanIndexMin, scanIndexMax, scanIndexTileHeight
        { 195.0, 1516.0, 1007.0, 0, 5682, 301 },
        { 195.0, 1516.0, 10000.0, 0, 5682, 1 },
        { 195.0, 1516.0, 0.0123, 0, 5682, 2007 },
        { 195.0, 1516.0, 0.05, 0, 5682, 1003 },
        { 195.0, 1516.0, 500.05, 0, 5682, 150 },
        { 195.0, 1516.0, 30, 0, 5682, 60 },
        { 195.0, 1516.0, 60, 0, 5682, 64 }
    };

    int failedConfigurations = 0;
    int configIndex = 0;
    for (const RangeConfig &config : configs) {
        configIndex++;
        
        NonUniformTileRange range;

        range.setMz(config.mzMin,config.mzMax);
        range.setMzTileLength(config.mzTileWidth);

        range.setScanIndex(config.scanIndexMin, config.scanIndexMax);
        range.setScanIndexLength(config.scanIndexTileHeight);

        int tileCountX = range.tileCountX();
        int failed = 0;
        for (int tileX = 0; tileX < tileCountX; tileX++) {
            double mz = range.mzAt(tileX);
            int actualTileX = range.tileX(mz);

            if (actualTileX != tileX) {
                failed++;
                qDebug() << "FAIL" << configIndex<< ":diff" << tileX - actualTileX << "TileX" << tileX << "returns mz" << mz << "converted to tileX" << actualTileX;
            }
        }

        if (failed > 0) {
            qDebug() << "Config" << configIndex << "failed" << failed << "times";
            failedConfigurations++;
        }
    }

    QCOMPARE(failedConfigurations, 0);
}

void NonUniformTileRangeTest::testTileX()
{
    //TODO: have this initializer for NonUniformTileRange too!
    RangeConfig config = { 0.0, 200.0, 100.0, 
                             0, 1000, 50 };

    NonUniformTileRange range;
    range.setMz(config.mzMin, config.mzMax);
    range.setMzTileLength(config.mzTileWidth);
    range.setScanIndex(config.scanIndexMin, config.scanIndexMax);
    range.setScanIndexLength(config.scanIndexTileHeight);
    QVERIFY(!range.isNull());

    QCOMPARE(range.tileX(0.0), 0);
    QCOMPARE(range.tileX(25.0), 0);
    QCOMPARE(range.tileX(50.0), 0);
    QCOMPARE(range.tileX(50.00000001), 0);
    QCOMPARE(range.tileX(50.0), 0);
    QCOMPARE(range.tileX(60.00000001), 0);
    QCOMPARE(range.tileX(99.00000009), 0);
    QCOMPARE(range.tileX(99.99999999), 0);

    QCOMPARE(range.tileX(100), 1);
    QCOMPARE(range.tileX(100.0000001), 1);
}


void NonUniformTileRangeTest::testNull()
{
    NonUniformTileRange range;
    QVERIFY(range.isNull());
    
    range.setMz(100, 200);
    QVERIFY(!range.isNull());
}

void NonUniformTileRangeTest::testContains()
{
    NonUniformTileRange range;
    range.setMz(195.11802005248208, 1515.1351516368195);
    range.setMzTileLength(30.0);

    range.setScanIndex(0, 63);
    range.setScanIndexLength(64);


    QRectF wholeRange = range.area();
    // whole range fits
    QVERIFY(range.contains(wholeRange));

    // if we translate whole range, it should not fit
    QRectF translatedWholeRange = wholeRange.translated(QPointF(1,1));
    QVERIFY(!range.contains(translatedWholeRange));

    // scale down, it needs to fit
    QRectF scaledDown = wholeRange.adjusted(5, 5, -5, -5);
    QVERIFY(range.contains(scaledDown));
}

void NonUniformTileRangeTest::testTileRect()
{
    // this test tests and demonstrates API showing which tiles belong to given rect specified by
    // mz interval and scan index interval
    const double mzTileLenght = 30.0;
    const int scanIndexLength = 64;

    NonUniformTileRange range;
    // 45 tiles in row
    range.setMz(195.0, 1520.0);
    range.setMzTileLength(mzTileLenght);

    // 2 tiles in column
    range.setScanIndex(0, 127);
    range.setScanIndexLength(scanIndexLength);

    // randomly chosen epsilon so that we have prev value of tile boundary
    // for mz boundery we don't have granularity defined: bounds are mzStart +30.0mz, +60.0mz, ...
    // for scan index the granularity is 1 scan index 0, 1, 2,
    const double mzEpsilon = 0.0001;

    // test case to get first tile
    double mzStart = range.mzMin();
    double mzEnd = mzStart + mzTileLenght - mzEpsilon;
    int scanIndexStart = range.scanIndexMin();
    int scanIndexEnd = range.scanIndexMin() + (scanIndexLength / 2);

    QRect tileRect = range.tileRect(mzStart, mzEnd, scanIndexStart, scanIndexEnd);
    QCOMPARE(tileRect, QRect(0, 0, 1, 1));

    // test case to get first two tiles
    mzStart = range.mzMin();
    mzEnd = mzStart + mzTileLenght;
    scanIndexStart = range.scanIndexMin();
    scanIndexEnd = scanIndexStart + scanIndexLength;

    tileRect = range.tileRect(mzStart, mzEnd, scanIndexStart, scanIndexEnd);
    QCOMPARE(tileRect, QRect(0, 0, 2, 2));

    // test case to get 2nd tile (on second row and second column)
    // move by 1 tile to tile column 1 (counts from 0)
    mzStart = range.mzMin() + mzTileLenght;
    mzEnd = mzStart + mzTileLenght - mzEpsilon; // stay within this tile

    // move by 1 tile to tile row 1 (counts from 0)
    scanIndexStart = range.scanIndexMin() + scanIndexLength;
    scanIndexEnd = scanIndexStart + scanIndexLength - 1; // stay within this tile

    tileRect = range.tileRect(mzStart, mzEnd, scanIndexStart, scanIndexEnd);
    QCOMPARE(tileRect, QRect(1, 1, 1, 1));

    // all tiles
    tileRect
        = range.tileRect(range.mzMin(), range.mzMax(), range.scanIndexMin(), range.scanIndexMax());
    
    QRect fullAreaTileRect(0, 0, range.tileCountX(), range.tileCountY());
    QCOMPARE(tileRect, fullAreaTileRect);

    QCOMPARE(range.tileRect(range.areaRect()), fullAreaTileRect);
}

_PMI_END

QTEST_MAIN(pmi::NonUniformTileRangeTest)



#include "NonUniformTileRangeTest.moc"

