/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include <QtTest>
#include "pmi_core_defs.h"
#include "NonUniformTileRange.h"
#include "common_math_types.h"
#include "ScanDataTiledIterator.h"

_PMI_BEGIN

class ScanDataTiledIteratorTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testNext();


};

void ScanDataTiledIteratorTest::testNext()
{
    NonUniformTileRange range;

    range.setMz(0.0, 5.0);
    range.setMzTileLength(2.0);
    
    range.setScanIndex(0, 1);
    range.setScanIndexLength(2);

    const double INTENSITY = 0.0;
    point2dList scanData = {
        QPointF(0.012, INTENSITY),
        QPointF(0.9, INTENSITY),
        QPointF(2.00014, INTENSITY),
        QPointF(2.99999, INTENSITY),
        QPointF(4.9999, INTENSITY)
    };

    int tileXIndex = 0;
    int tileCountX = range.tileCountX();

    ScanDataTiledIterator it(&scanData, range, tileXIndex, tileCountX);

    point2dList tile1 = {
        scanData[0],
        scanData[1]
    };
    

    point2dList tile2 = {
        scanData[2],
        scanData[3],
    };

    point2dList tile3 = {
        scanData[4],
    };

        
    QVERIFY(it.hasNext());
    QCOMPARE(it.next(), tile1);
    QVERIFY(it.hasNext());
    QCOMPARE(it.next(), tile2);
    QVERIFY(it.hasNext());
    QCOMPARE(it.next(), tile3);
    QVERIFY(!it.hasNext());

}

_PMI_END

QTEST_MAIN(pmi::ScanDataTiledIteratorTest)

#include "ScanDataTiledIteratorTest.moc"
