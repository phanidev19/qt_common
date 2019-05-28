/*
 * Copyright (C) 2019 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author  : Michael Georgoulopoulos mgeorgoulopoulos@gmail.com
 */

#include "pmi_core_defs.h"
#include <QFile>
#include <QtTest>

#include "PlotBase.h"
#include "TimeWarp.h"

_PMI_BEGIN

class TimeWarpTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testMapTime();

    /*! \brief Tests that we create a successful warp even when sampling frequencies vary. */
    void testDifferentSampling();
};

void TimeWarpTest::testMapTime()
{
    // sample data
    std::vector<double> timeSource = { 0.02, 0.04, 0.07, 0.09, 0.10, 0.14, 0.16, 0.18, 0.20, 0.24 };
    std::vector<double> scanIndexTarget = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

    QCOMPARE(TimeWarp::mapTime(timeSource, scanIndexTarget, 0.02), 0.0);
    QCOMPARE(TimeWarp::mapTime(timeSource, scanIndexTarget, 0.09), 3.0);
    QCOMPARE(TimeWarp::mapTime(timeSource, scanIndexTarget, 0.03), 0.5);
    QCOMPARE(TimeWarp::mapTime(timeSource, scanIndexTarget, 0.24), 9.0);

    QCOMPARE(TimeWarp::mapTime(scanIndexTarget, timeSource, 0), 0.02);
    QCOMPARE(TimeWarp::mapTime(scanIndexTarget, timeSource, 3), 0.09);
    QCOMPARE(TimeWarp::mapTime(scanIndexTarget, timeSource, 0.5), 0.03);
    QCOMPARE(TimeWarp::mapTime(scanIndexTarget, timeSource, 9), 0.24);
}

static double psin(double t)
{
    double scale = 1.0;
    return scale * std::max(0.0, sin(t));
}

static Err saveCSV(const PlotBase &plot, const QString &filename)
{
    Err e = kNoErr;

    QFile file(filename);
    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        rrr(kFileOpenError);
    }
    QTextStream s(&file);
    s << "x\ty\n";
    for (const point2d &p : plot.getPointList()) {
        s << p.x() << "\t" << p.y() << "\n";
    }
    file.close();

    return e;
}

static Err saveCSV(const std::vector<double> &x, const std::vector<double> &y, const QString &filename)
{
    Err e = kNoErr;

    if (x.size() != y.size()) {
        rrr(kBadParameterError);
    }

    point2dList points;
    for (int i = 0; i < static_cast<int>(x.size()); i++) {
        points.push_back(point2d(x[i], y[i]));
    }

    //e = saveCSV(PlotBase(points), filename); ree;


    return e;
}

void TimeWarpTest::testDifferentSampling()
{
    const double duration = 10.0;
    const double samplingRate = 10.0;
    const double samplingPeriod = 1.0 / samplingRate;

    // Multiplier to vary sampling speeds of the two plots
    const int samplingFactor = 4;

    // Produce two plots with the same signal but different sampling rates
    point2dList pointsA;
    double xa = 0.0;
    double samplingPeriodA = samplingPeriod / static_cast<double>(samplingFactor);
    point2dList pointsB;
    double xb = 0.0;
    double samplingPeriodB = samplingPeriod * static_cast<double>(samplingFactor);
    const int pointCount = static_cast<int>(duration * samplingRate);
    for (int i = 0; i < pointCount / (2 * samplingFactor); i++) {
        pointsA.push_back(point2d(xa, psin(xa)));
        xa += samplingPeriodA;
        pointsB.push_back(point2d(xb, psin(xb)));
        xb += samplingPeriodB;
    }

    // Second half
    const int remainingSamples = pointCount - static_cast<int>(pointsA.size());
    const double startA = pointsA.back().x();
    samplingPeriodA = (duration - startA) / remainingSamples;
    const double startB = pointsB.back().x();
    samplingPeriodB = (duration - startB) / remainingSamples;
    for (int i = 0; i < remainingSamples; i++) {
        pointsA.push_back(point2d(xa, psin(xa)));
        xa += samplingPeriodA;
        pointsB.push_back(point2d(xb, psin(xb)));
        xb += samplingPeriodB;
    }

    // double-check that both plots contain the same signal
    QCOMPARE(pointsA.size(), pointsB.size());
    PlotBase plotBaseA(pointsA);
    PlotBase plotBaseB(pointsB);
    const double signalTolerance = 0.05;
    for (double i = 0.0; i <= duration; i += samplingPeriod) {
        double diff = std::abs(plotBaseB.evaluate(i) - plotBaseA.evaluate(i));
        QVERIFY(diff < signalTolerance);
    }

    // Export to CSV
    //QCOMPARE(saveCSV(pointsA, "DifferentSamplingSignalA.csv"), kNoErr);
    //QCOMPARE(saveCSV(pointsB, "DifferentSamplingSignalB.csv"), kNoErr);

    // Create time warp
    TimeWarp::TimeWarpOptions options;
    TimeWarp timeWarp;
    QCOMPARE(timeWarp.constructWarp(pointsB, pointsA, options), kNoErr);

    // Get anchors
    std::vector<double> anchorsA;
    std::vector<double> anchorsB;
    QCOMPARE(timeWarp.getAnchors(&anchorsA, &anchorsB), kNoErr);

    // Export anchors to CSV
    //QCOMPARE(saveCSV(anchorsA, anchorsB, "DifferentSamplingWarpAnchors.csv"), kNoErr);

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

QTEST_MAIN(pmi::TimeWarpTest)

#include "TimeWarpTest.moc"
