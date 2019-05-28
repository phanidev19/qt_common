/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "ChargeDeterminator.h"

#include <pmi_core_defs.h>

#include <QtTest>

#include <Eigen/Dense>

using Eigen::MatrixXd;

_PMI_BEGIN

class ChargeDeterminatorTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testNextHigherLowerCharge_data();
    void testNextHigherLowerCharge();
    void testEigen3();
};

void ChargeDeterminatorTest::testNextHigherLowerCharge_data()
{
    QTest::addColumn<int>("charge");
    QTest::addColumn<int>("nextCharge");
    QTest::addColumn<double>("mostAbundantIonMz");
    QTest::addColumn<double>("expectedResult");

    // values taken from Feature finder algorithm.pptx / slide 3 / Calculation 1 and 2
    QTest::newRow("nextHigherCharge") << 3 << 4 << 949.16 << 712.12195625000004;
    QTest::newRow("nextLowerCharge") << 3 << 2 << 949.16 << 1423.2360874999999;
}

void ChargeDeterminatorTest::testNextHigherLowerCharge()
{
    QFETCH(int, charge);
    QFETCH(int, nextCharge);
    QFETCH(double, mostAbundantIonMz);
    QFETCH(double, expectedResult);
    
    ChargeDeterminator determinator;
    double result = determinator.nextChargeState(charge, nextCharge, mostAbundantIonMz);
    QCOMPARE(result, expectedResult);

}

void ChargeDeterminatorTest::testEigen3()
{
    MatrixXd m(2, 2);
    m(0, 0) = 3 * 3;
    m(1, 0) = 2.5;
    m(0, 1) = -1;
    m(1, 1) = m(1, 0) + m(0, 1);
    QCOMPARE(m(0, 0), 9.0);
}

_PMI_END

QTEST_MAIN(pmi::ChargeDeterminatorTest)

#include "ChargeDeterminatorTest.moc"
