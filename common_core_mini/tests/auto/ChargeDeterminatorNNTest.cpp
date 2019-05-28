/*
 * Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Lukas Tvrdy ltvrdy@milosolutions.com
 * Author: Andrew Nichols anichols@proteinmetrics.com
 */

#include "AveragineGenerator.h"
#include "ChargeDeterminatorNN.h"

#include <pmi_core_defs.h>

#include <QtTest>

_PMI_BEGIN

class ChargeDeterminatorNNTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:

    // tests one specific charge vector, good for quick debugging of certain values
    void testDetermineChargeStatic();

    // sophisticated generator generates scans with pre-determined/known charge and charge
    // determination is verified
    void testDetermineChargeGenerated();
};

void pmi::ChargeDeterminatorNNTest::testDetermineChargeStatic()
{
    QString neuralNetworkDbFilePath = QDir(qApp->applicationDirPath()).filePath("nn_weights.db");
    QVERIFY(QFileInfo::exists(neuralNetworkDbFilePath));
    ChargeDeterminatorNN chargeDeterminator;
    Err e = chargeDeterminator.init(neuralNetworkDbFilePath);
    QCOMPARE(e, kNoErr);

    point2dList data = {
        point2d(751.88, 158300000),
        point2d(752.38, 119600000),
        point2d(752.88, 55250000),
        point2d(753.38, 17200000),
        point2d(753.88, 1080000)
    };

    int charge = chargeDeterminator.determineCharge(data, 752.88);
    QCOMPARE(charge, 2);
}

void ChargeDeterminatorNNTest::testDetermineChargeGenerated()
{
    QString neuralNetworkDbFilePath = QDir(qApp->applicationDirPath()).filePath("nn_weights.db");
    QVERIFY(QFileInfo::exists(neuralNetworkDbFilePath));
    
    ChargeDeterminatorNN chargeDeterminator;
    Err e = chargeDeterminator.init(neuralNetworkDbFilePath);
    QCOMPARE(e, kNoErr);

    int mzStart = 200;
    int mzEnd = 2000;

    const int massStart = 600;
    const int massEnd = 10000;
    const int massStep = 100;

    AveragineGenerator generator(massStart);

    for (int mass = massStart; mass < massEnd; mass += massStep) {
        generator.setMass(mass);
        
        for (int expectedCharge = 1; expectedCharge <= 10; ++expectedCharge) {
            double monoIsotopeMz = generator.computeMonoisotopeMz(mass, expectedCharge);
            if ((monoIsotopeMz> mzStart) && (monoIsotopeMz < mzEnd)) {
                const point2dList data = generator.generateSignal(expectedCharge);
                int charge = chargeDeterminator.determineCharge(data, monoIsotopeMz);
                if (charge != expectedCharge) {
                    qDebug() << "Actual charge:" << charge << "Expected:" << expectedCharge;
                }
                
                QCOMPARE(charge, expectedCharge);
            } 
        }
    }
}

_PMI_END

QTEST_MAIN(pmi::ChargeDeterminatorNNTest)

#include "ChargeDeterminatorNNTest.moc"
