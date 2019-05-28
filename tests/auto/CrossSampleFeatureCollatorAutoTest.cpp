/*
 * Copyright (C) 2019 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Andrew Nichols anichols@proteinmetrics.com
 */

#include "CrossSampleFeatureCollatorTurbo.h"

#include <PMiTestUtils.h>
#include <pmi_core_defs.h>

#include <QtTest>

_PMI_BEGIN

class CrossSampleFeatureCollatorAutoTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testCollationParameterTimeThresholdValidity();
    void testCollationParameterMWThresholdValidity();
    void testCollationAmbiguity();
    void testDuplicateRemoval();

private:
    SettableFeatureFinderParameters m_ffUserParams;
    ImmutableFeatureFinderParameters m_ffImmutables;
};

void CrossSampleFeatureCollatorAutoTest::testCollationParameterTimeThresholdValidity()
{

    CrossSampleFeatureTurbo feature1;
    feature1.sampleId = 1;
    feature1.mwMonoisotopic = 1000;
    feature1.rt = 10;
    feature1.rtWarped = feature1.rt;
    feature1.feature = 1;

    CrossSampleFeatureTurbo feature2;
    feature2.sampleId = 2;
    feature2.mwMonoisotopic = 1000;
    feature2.rt = 10.0799999;
    feature2.rtWarped = feature2.rt;
    feature2.feature = 2;

    CrossSampleFeatureTurbo feature3;
    feature3.sampleId = 1;
    feature3.mwMonoisotopic = 2000;
    feature3.rt = 10;
    feature3.rtWarped = feature3.rt;
    feature3.feature = 3;

    CrossSampleFeatureTurbo feature4;
    feature4.sampleId = 2;
    feature4.mwMonoisotopic = 2000;
    feature4.rt = 10.0801;
    feature4.rtWarped = feature4.rt;
    feature4.feature = 4;

    QVector<CrossSampleFeatureTurbo> consolidatedFeatures
        = { feature1, feature2, feature3, feature4 };

    CrossSampleFeatureCollatorTurbo crossSampleCollatorTurbo(QVector<SampleFeaturesTurbo>(),
                                                             m_ffUserParams);
    crossSampleCollatorTurbo.testCollation(consolidatedFeatures);
    QVector<CrossSampleFeatureTurbo> results
        = crossSampleCollatorTurbo.consolidatedCrossSampleFeatures();

    QCOMPARE(0.08, m_ffImmutables.maxTimeToleranceWarped);
    QCOMPARE(results[0].masterFeature, 0);
    QCOMPARE(results[0].feature, 1);
    QCOMPARE(results[1].feature, 2);
    QCOMPARE(results[2].feature, 3);
    QCOMPARE(results[3].feature, 4);
    QCOMPARE(results[0].masterFeature, results[1].masterFeature);
    QCOMPARE(results[2].masterFeature, 1);
    QCOMPARE(results[3].masterFeature, 2);
}

void CrossSampleFeatureCollatorAutoTest::testCollationParameterMWThresholdValidity()
{

    CrossSampleFeatureTurbo feature1;
    feature1.sampleId = 1;
    feature1.mwMonoisotopic = 1000;
    feature1.rt = 10;
    feature1.rtWarped = feature1.rt;
    feature1.feature = 1;

    CrossSampleFeatureTurbo feature2;
    feature2.sampleId = 2;
    feature2.mwMonoisotopic = 1000.051;
    feature2.rt = 10;
    feature2.rtWarped = feature2.rt;
    feature2.feature = 2;

    CrossSampleFeatureTurbo feature3;
    feature3.sampleId = 2;
    feature3.mwMonoisotopic = 1000.015;
    feature3.rt = 10;
    feature3.rtWarped = feature3.rt;
    feature3.feature = 3;

    QVector<CrossSampleFeatureTurbo> consolidatedFeatures = { feature1, feature2, feature3 };

    CrossSampleFeatureCollatorTurbo crossSampleCollatorTurbo(QVector<SampleFeaturesTurbo>(),
                                                             m_ffUserParams);
    crossSampleCollatorTurbo.testCollation(consolidatedFeatures);
    QVector<CrossSampleFeatureTurbo> results
        = crossSampleCollatorTurbo.consolidatedCrossSampleFeatures();

    QCOMPARE(15, m_ffUserParams.ppm);

    QCOMPARE(results[0].masterFeature, 0);
    QCOMPARE(results[0].feature, 1);
    QCOMPARE(results[1].masterFeature, 0);
    QCOMPARE(results[1].feature, 3);
    QCOMPARE(results[2].masterFeature, 1);
    QCOMPARE(results[2].feature, 2);
}

void CrossSampleFeatureCollatorAutoTest::testCollationAmbiguity()
{

    CrossSampleFeatureTurbo feature1;
    feature1.sampleId = 1;
    feature1.mwMonoisotopic = 1000;
    feature1.rt = 10;
    feature1.rtWarped = feature1.rt;
    feature1.feature = 1;

    CrossSampleFeatureTurbo feature2;
    feature2.sampleId = 2;
    feature2.mwMonoisotopic = 1000;
    feature2.rt = 9.95;
    feature2.rtWarped = feature2.rt;
    feature2.feature = 1;

    CrossSampleFeatureTurbo feature3;
    feature3.sampleId = 3;
    feature3.mwMonoisotopic = 1000;
    feature3.rt = 10.01;
    feature3.rtWarped = feature3.rt;
    feature3.feature = 1;

    CrossSampleFeatureTurbo feature4;
    feature4.sampleId = 4;
    feature4.mwMonoisotopic = 1000;
    feature4.rt = 10.0301;
    feature4.rtWarped = feature4.rt;
    feature4.feature = 1;

    QVector<CrossSampleFeatureTurbo> consolidatedFeatures
        = { feature1, feature2, feature3, feature4 };

    CrossSampleFeatureCollatorTurbo crossSampleCollatorTurbo(QVector<SampleFeaturesTurbo>(),
                                                             m_ffUserParams);
    crossSampleCollatorTurbo.testCollation(consolidatedFeatures);
    QVector<CrossSampleFeatureTurbo> results
        = crossSampleCollatorTurbo.consolidatedCrossSampleFeatures();

    QCOMPARE(15, m_ffUserParams.ppm);
    QCOMPARE(results[0].masterFeature, 0);
    QCOMPARE(results[1].masterFeature, 0);
    QCOMPARE(results[2].masterFeature, 0);
    QCOMPARE(results[3].masterFeature, 1);
    QCOMPARE(results[0].sampleId, 2);
    QCOMPARE(results[1].sampleId, 1);
    QCOMPARE(results[2].sampleId, 3);
    QCOMPARE(results[3].sampleId, 4);
}

void CrossSampleFeatureCollatorAutoTest::testDuplicateRemoval()
{

    CrossSampleFeatureTurbo feature1;
    feature1.sampleId = 1;
    feature1.mwMonoisotopic = 1000;
    feature1.rt = 10;
    feature1.rtWarped = feature1.rt;
    feature1.maxIntensity = 1000;
    feature1.feature = 1;

    CrossSampleFeatureTurbo feature2;
    feature2.sampleId = 2;
    feature2.mwMonoisotopic = 1000;
    feature2.rt = 10.0799999;
    feature2.rtWarped = feature2.rt;
    feature2.maxIntensity = 10000;
    feature2.feature = 2;

    CrossSampleFeatureTurbo feature3;
    feature3.sampleId = 1;
    feature3.mwMonoisotopic = 1000;
    feature3.rt = 10.05;
    feature3.rtWarped = feature3.rt;
    feature3.maxIntensity = 10000;
    feature3.feature = 3;

    CrossSampleFeatureTurbo feature4;
    feature4.sampleId = 2;
    feature4.mwMonoisotopic = 2000;
    feature4.rt = 10.0799999;
    feature4.rtWarped = feature4.rt;
    feature4.maxIntensity = 10000;
    feature4.feature = 4;

    CrossSampleFeatureTurbo feature5;
    feature5.sampleId = 1;
    feature5.mwMonoisotopic = 2000;
    feature5.rt = 10.05;
    feature5.rtWarped = feature5.rt;
    feature5.maxIntensity = 10000;
    feature5.feature = 5;



    QVector<CrossSampleFeatureTurbo> consolidatedFeatures
        = { feature1, feature2, feature3, feature4,  feature5 };

    CrossSampleFeatureCollatorTurbo crossSampleCollatorTurbo(QVector<SampleFeaturesTurbo>(),
        m_ffUserParams);

    bool removeDuplicates = true;
    crossSampleCollatorTurbo.testCollation(consolidatedFeatures, removeDuplicates);

    QVector<CrossSampleFeatureTurbo> results
        = crossSampleCollatorTurbo.consolidatedCrossSampleFeatures();

    std::sort(results.begin(), results.end(),
              [](const CrossSampleFeatureTurbo &a, const CrossSampleFeatureTurbo &b) {
                  return a.feature < b.feature;
              });

    QCOMPARE(0.08, m_ffImmutables.maxTimeToleranceWarped);
    QCOMPARE(results[0].masterFeature, 0);
    QCOMPARE(results[0].feature, 2);
    QCOMPARE(results[1].feature, 3);
    QCOMPARE(results[2].feature, 4);
    QCOMPARE(results[3].feature, 5);
    QCOMPARE(results[0].masterFeature, results[1].masterFeature);
    QCOMPARE(results[2].masterFeature, 1);
    QCOMPARE(results[3].masterFeature, 1);
}

_PMI_END

QTEST_MAIN(pmi::CrossSampleFeatureCollatorAutoTest)

#include "CrossSampleFeatureCollatorAutoTest.moc"
