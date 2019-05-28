/*
 * Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Andrew Nichols anichols@proteinmetrics.com
 */

#include "BottomUpConvolution.h"
#include "pmi_core_defs.h"

#include <QtTest>

_PMI_BEGIN

class BottomUpConvolutionTest : public QObject
{
    Q_OBJECT
public:
    BottomUpConvolutionTest() {}
private Q_SLOTS:
    void testBottomUpConvolutionTypical();
    void testBottomUpConvolutionExtremes();
    void testBottomUpConvolutionDuplicates();
    void testNullData();
   

private:
    double m_pruningThreshold = 1e-5;

    void baseTest(const QList<BottomUpConvolution::DistributionInput> &distributionInputs,
                  const QList<int> &expectedKeys, const QVector<int> &expectedFractions,
                  const QMap<QStringList, double> &expectedOriginSubDistribution, int subTestKey)
    {

        BottomUpConvolution bottomUpConvolution;
        //// Build computed distributions to access w/ following functions below
        Err e = bottomUpConvolution.init(distributionInputs, m_pruningThreshold);
        QCOMPARE(e, kNoErr);
        int thresholdPercent = 0;
        const QVector<BottomUpConvolution::DeltaMassStick> sticks = bottomUpConvolution.deltaMassSticks();

        QList<int> computedDistributionKeys;
        QVector<int> computedFractions;
        QVector<BottomUpConvolution::DegradationConstituents> degradationConstituents;
        for (const BottomUpConvolution::DeltaMassStick & stick : sticks) {
            computedDistributionKeys.push_back( static_cast<int>(qRound(stick.deltaMass)) );
            computedFractions.push_back(static_cast<int>(qRound(100 * stick.intensityFraction)));
            degradationConstituents = static_cast<int>(qRound(stick.deltaMass)) == 16 ? stick.degradationConstituents : degradationConstituents;
        }

        std::sort(computedDistributionKeys.begin() , computedDistributionKeys.end());
        std::sort(computedFractions.begin(), computedFractions.end());

        QCOMPARE(computedDistributionKeys, expectedKeys);
        QCOMPARE(computedFractions, expectedFractions);

        QList<int> computedSubFractions;
        for(const BottomUpConvolution::DegradationConstituents & degradationConstituent : degradationConstituents){
            computedSubFractions.push_back(static_cast<int>((100 * degradationConstituent.percentContribution)));      
        }
        std::sort(computedSubFractions.begin(), computedSubFractions.end());

        QList<double> expectedSubFractions = expectedOriginSubDistribution.values();
        std::sort(expectedSubFractions.begin(), expectedSubFractions.end());

        QCOMPARE(expectedSubFractions.size(), computedSubFractions.size());

        for (int i = 0; i < static_cast<int>(expectedSubFractions.size()); ++i) {
            QCOMPARE(computedSubFractions[i] , static_cast<int>(expectedSubFractions[i]));
        }
    }
};

/*!
 * Test w./ data that algo will most likely see.
 */
void BottomUpConvolutionTest::testBottomUpConvolutionTypical()
{
    QList<BottomUpConvolution::DistributionInput> distributionInputs;
    BottomUpConvolution::DegradationID degradationID("0", 0, 0);
    BottomUpConvolution::DegradationID degradationID2("666", 0, 0);
    BottomUpConvolution::DistributionInput d1(degradationID, 16, 0.3);
    distributionInputs.push_back(d1);
    BottomUpConvolution::DistributionInput d2(degradationID2, 16, 0.1);
    distributionInputs.push_back(d2);

    QList<int> expectedKeys = { 0, 16, 32 };
    QVector<int> expectedFractions = { 3, 34, 63 };

    QMap<QStringList, double> expectedOriginSubDistribution;
    expectedOriginSubDistribution[QStringList() << "0:0:1/16"] = 79;
    expectedOriginSubDistribution[QStringList() << "1:0:666/16"] = 20;

    int subTestKey = 1;

    baseTest(distributionInputs, expectedKeys, expectedFractions, expectedOriginSubDistribution, subTestKey);
}

/*!
 * Purposly enters extreme y values.
 */
void BottomUpConvolutionTest::testBottomUpConvolutionExtremes()
{
    QList<BottomUpConvolution::DistributionInput> distributionInputs;
    BottomUpConvolution::DegradationID degradationID("0", 0, 0);
    BottomUpConvolution::DegradationID degradationID2("666", 0, 0);
    BottomUpConvolution::DistributionInput d1(degradationID, 16, 1);
    distributionInputs.push_back(d1);
    BottomUpConvolution::DistributionInput d2(degradationID2, 16, 1.1);
    distributionInputs.push_back(d2);
    

    BottomUpConvolution bottomUpConvolution;
    //// Build computed distributions to access w/ following functions below
    Err e = bottomUpConvolution.init(distributionInputs, m_pruningThreshold);
    QCOMPARE(e, kError);
}

/*!
 * Purposly enters duplicate x values.  Will get warnings from qWarning.
 */
void BottomUpConvolutionTest::testBottomUpConvolutionDuplicates()
{

    QList<BottomUpConvolution::DistributionInput> distributionInputs;
    BottomUpConvolution::DegradationID degradationID("666", 0, 0);
    BottomUpConvolution::DistributionInput d1(degradationID, 16, 0.505);
    distributionInputs.push_back(d1);
    distributionInputs.push_back(d1);

    BottomUpConvolution bottomUpConvolution;
    //// Build computed distributions to access w/ following functions below
    Err e = bottomUpConvolution.init(distributionInputs, m_pruningThreshold);
    QCOMPARE(e, kError);
}

void BottomUpConvolutionTest::testNullData() {
    QList<BottomUpConvolution::DistributionInput> distributionInputs;
    BottomUpConvolution bottomUpConvolution;
    //// Build computed distributions to access w/ following functions below
    Err e = bottomUpConvolution.init(distributionInputs, m_pruningThreshold);
    QCOMPARE(e, kError);
}


_PMI_END

QTEST_MAIN(pmi::BottomUpConvolutionTest)

#include "BottomUpConvolutionTest.moc"
