/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "NonUniformTilePartIterator.h"

#include <common_math_types.h>
#include <pmi_core_defs.h>

#include <QtTest>

_PMI_BEGIN

class NonUniformTilePartIteratorTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testNonUniformTilePartIterator_data();
    void testNonUniformTilePartIterator();
};

void NonUniformTilePartIteratorTest::testNonUniformTilePartIterator_data()
{
    QTest::addColumn<point2dList>("scanPart");
    QTest::addColumn<double>("mzStart");
    QTest::addColumn<double>("mzEnd");
    QTest::addColumn<point2dList>("expected");

    const point2dList scanPart = {
        point2d(0, 0),
        point2d(1, 10),
        point2d(2, 20),
        point2d(3, 30),
        point2d(4, 40),
        point2d(5, 50),
        point2d(6, 60),
        point2d(7, 70),
        point2d(8, 80),
        point2d(9, 90),
    };

    QTest::newRow("inMiddle") << scanPart << 3.0 << 6.1
        << point2dList({
        point2d(3, 30),
        point2d(4, 40),
        point2d(5, 50),
        point2d(6, 60),
    });

    QTest::newRow("firstItem") << scanPart << 0.0 << 0.1
        << point2dList({
        point2d(0, 0),
    });

    QTest::newRow("lastItem") << scanPart << 9.0 << 9.1
        << point2dList({
        point2d(9, 90),
    });

    QTest::newRow("outOfRange") << scanPart << 10.0 << 20.0
        << point2dList({
    });


    QTest::newRow("emptyInput") << point2dList() << 10.0 << 20.0
        << point2dList({
    });


}

void NonUniformTilePartIteratorTest::testNonUniformTilePartIterator()
{
    QFETCH(point2dList, scanPart);
    QFETCH(double, mzStart);
    QFETCH(double, mzEnd);

    NonUniformTilePartIterator iterator(scanPart, mzStart, mzEnd);
    point2dList result;
    QFETCH(point2dList, expected);
    while (iterator.hasNext()) {
        result.push_back(iterator.next());
    }
}

_PMI_END

QTEST_MAIN(pmi::NonUniformTilePartIteratorTest)

#include "NonUniformTilePartIteratorTest.moc"
