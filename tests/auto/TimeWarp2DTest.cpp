/*
 * Copyright (C) 2019 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author  : Michael Georgoulopoulos mgeorgoulopoulos@gmail.com
 */

#include <QtTest>
#include "pmi_core_defs.h"

#include "PlotBase.h"
#include "TimeWarp2D.h"

_PMI_BEGIN

class TimeWarp2DTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testMapTime();

    /*! \brief Tests that we create a successful warp even when sampling frequencies vary. */
    void testDifferentSampling();


};

void TimeWarp2DTest::testMapTime()
{
    // sample data 
    std::vector<double> timeSource = { 0.02, 0.04, 0.07, 0.09, 0.10, 0.14, 0.16, 0.18, 0.20, 0.24 };
    std::vector<double> scanIndexTarget = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

    QCOMPARE(TimeWarp2D::mapTime(timeSource, scanIndexTarget, 0.02), 0.0);
    QCOMPARE(TimeWarp2D::mapTime(timeSource, scanIndexTarget, 0.09), 3.0);
    QCOMPARE(TimeWarp2D::mapTime(timeSource, scanIndexTarget, 0.03), 0.5);
    QCOMPARE(TimeWarp2D::mapTime(timeSource, scanIndexTarget, 0.24), 9.0);

    QCOMPARE(TimeWarp2D::mapTime(scanIndexTarget, timeSource, 0), 0.02);
    QCOMPARE(TimeWarp2D::mapTime(scanIndexTarget, timeSource, 3), 0.09);
    QCOMPARE(TimeWarp2D::mapTime(scanIndexTarget, timeSource, 0.5), 0.03);
    QCOMPARE(TimeWarp2D::mapTime(scanIndexTarget, timeSource, 9), 0.24);
}

static double psin(double t)
{
    double scale = 1.0;
    return scale * std::max(0.0, sin(t));
}

void TimeWarp2DTest::testDifferentSampling()
{
    const double duration = 10.0;
    const double samplingRate = 10.0;
    const double samplingPeriod = 1.0 / samplingRate;

    // Multiplier to vary sampling speeds of the two plots
    const int samplingFactor = 4;

    // Produce two plots with the same signal but different sampling rates
    TimedWarpElementList pointsA;
    double xa = 0.0;
    double samplingPeriodA = samplingPeriod / static_cast<double>(samplingFactor);
    TimedWarpElementList pointsB;
    double xb = 0.0;
    double samplingPeriodB = samplingPeriod * static_cast<double>(samplingFactor);
    const int pointCount = static_cast<int>(duration * samplingRate);

    // Create the first segment with varied sampling rates
    for (int i = 0; i < pointCount / (2 * samplingFactor); i++) {
        pointsA.push_back(TimedWarpElement(xa, WarpElement::legacyWarpElement(psin(xa))));
        xa += samplingPeriodA;
        pointsB.push_back(TimedWarpElement(xb, WarpElement::legacyWarpElement(psin(xb))));
        xb += samplingPeriodB;
    }

    // Second half
    const int remainingSamples = pointCount - static_cast<int>(pointsA.size());
    const double startA = pointsA.back().timeInMinutes;
    samplingPeriodA = (duration - startA) / remainingSamples;
    const double startB = pointsB.back().timeInMinutes;
    samplingPeriodB = (duration - startB) / remainingSamples;
    for (int i = 0; i < remainingSamples; i++) {
        pointsA.push_back(TimedWarpElement(xa, WarpElement::legacyWarpElement(psin(xa))));
        xa += samplingPeriodA;
        pointsB.push_back(TimedWarpElement(xb, WarpElement::legacyWarpElement(psin(xb))));
        xb += samplingPeriodB;
    }

    // double-check that both plots contain the same signal
    QCOMPARE(pointsA.size(), pointsB.size());
    const double signalTolerance = 0.05;
    for (double i = 0.0; i <= duration; i += samplingPeriod) {
            double diff = std::abs(pointsB.evaluate(100.0, i).intensity()
                                   - pointsA.evaluate(100.0, i).intensity());
            QVERIFY(diff < signalTolerance);
    }

    // Create time warp
    TimeWarp2D::TimeWarpOptions options;
    TimeWarp2D timeWarp; // legacy mode
    QCOMPARE(timeWarp.constructWarp(pointsB, pointsA, options), kNoErr);

    // Get anchors
    std::vector<double> anchorsA;
    std::vector<double> anchorsB;
    QCOMPARE(timeWarp.getAnchors(&anchorsA, &anchorsB), kNoErr);

    // Compare anchors
    QCOMPARE(anchorsA.size(), anchorsB.size());

    // This is the maximum distance between samples in both plots.
    const double maximumSamplingPeriod = samplingPeriod * samplingFactor;

    // Allow a slack of 3 samples. The faulty warp should be off by more than 8.
    const double tolerance = 3.0 * maximumSamplingPeriod;

    for (int i = 0; i < static_cast<int>(anchorsA.size()) && i < static_cast<int>(anchorsB.size());
        i++) {
        double diff = std::abs(anchorsB[i] - anchorsA[i]);
        QVERIFY(diff < tolerance);
    }
}

_PMI_END

QTEST_MAIN(pmi::TimeWarp2DTest)

#include "TimeWarp2DTest.moc"
