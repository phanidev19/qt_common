/*
 * Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author  : Michael Georgoulopoulos mgeorgoulopoulos@gmail.com
 */

#include "MSMultiSampleTimeWarp.h"

#include <PMiTestUtils.h>

#include <QDir>
#include <QtTest>

#include <ProgressBarInterface.h>

_PMI_BEGIN

class MSMultiSampleTimeWarpTest : public QObject
{
    Q_OBJECT

public:
    MSMultiSampleTimeWarpTest(const QStringList &args);

private Q_SLOTS:
    void testAPI();
    void testAPIOverloads();

private:
    const int m_numberOfPointsToWarp = 10;
    QVector<QString> m_msFilenames;
    QVector<MSMultiSampleTimeWarp::MSID> m_msids;
    QDir m_testDataBasePath;
};

namespace {
    // Dummy progress bar, for testing
    class ProgressBar : public ProgressBarInterface
    {
    public:
        ProgressBar() 
            : pushCount(0)
            , popCount(0)
            , jobCount(0)
            , jobCompletedCount(0)
        {
        }

        void push(int max) override
        {
            pushCount++;
            jobCount += max;
        }

        void pop() override 
        {
            popCount++;
        }

        double incrementProgress(int inc) override 
        {
            jobCompletedCount += inc;
            return 0.0;
        }

        void setText(const QString &text) override
        {
        
        }

        void refreshUI() override 
        {
        
        }

        bool userCanceled() const override 
        {
            return false;
        }

    public:
        int pushCount;
        int popCount;
        int jobCount;
        int jobCompletedCount;
    };
}

MSMultiSampleTimeWarpTest::MSMultiSampleTimeWarpTest(const QStringList &args)
    : m_testDataBasePath(args[0])
{
    // Gather filenames from test data
    m_msFilenames.push_back(m_testDataBasePath.filePath("JH121614_ProtMet1_T0_Tryp_01.RAW"));
    m_msFilenames.push_back(m_testDataBasePath.filePath("JH121614_ProtMet1_pH8p5_7Day_Tryp_01.RAW"));

    // Add random IDs to test that it works even when indices are not 0, 1, ...
    m_msids.push_back(12345);
    m_msids.push_back(42);

    Q_ASSERT(m_msFilenames.size() == m_msids.size());
}

/*! \brief Utility to verify monotony of a sequence. */
static bool monotonic(const QVector<double> &values)
{
    if (values.size() < 2) {
        return true;
    }

    for (int i = 1; i < static_cast<int>(values.size()); i++) {
        if (values[i] < values[i - 1]) {
            return false;
        }
    }

    return true;
}

/*! \brief Gets n uniformly spaced time values from plot. */
static QVector<double> getNTimes(const PlotBase &plot, int n)
{
    QVector<double> ret;

    const int stride = static_cast<int>(plot.getPointListSize()) / n;
    for (int i = 0; i < static_cast<int>(plot.getPointListSize()); i += stride) {
        ret.push_back(plot.getPointList().at(i).x());
    }

    return ret;
}

/*! \brief Exercise all integer-addressed APIs */
void MSMultiSampleTimeWarpTest::testAPI()
{
    QSharedPointer<ProgressBar> progress(new ProgressBar());
    MSMultiSampleTimeWarp w;

    // First, make sure that we can actually create the warp.
    QVERIFY(w.constructTimeWarp(m_msFilenames, m_msids, progress) == kNoErr);

    // Test that progress reporting works
    QVERIFY(progress->jobCount != 0);
    QVERIFY(progress->jobCount == progress->jobCompletedCount);
    QVERIFY(progress->pushCount != 0);
    QVERIFY(progress->pushCount == progress->popCount);
    
    // Test individual samples
    for (const MSMultiSampleTimeWarp::MSID &msid : m_msids) {
        // Check base peak
        PlotBase plot;
        QVERIFY(w.getBasePeakPlot(msid, &plot) == kNoErr);
        QVERIFY(static_cast<int>(plot.getPointList().size()) >= m_numberOfPointsToWarp);

        // produce list of points to warp based on plot
        QVector<double> timesOriginal = getNTimes(plot, m_numberOfPointsToWarp);
        QVERIFY(monotonic(timesOriginal));

        // check time warp fetch
        TimeWarp timeWarp;
        QVERIFY(w.getTimeWarp(msid, &timeWarp) == kNoErr);

        // warp times using the fetched time warp
        QVector<double> timesWarpedWithFetched;
        for (const double t : timesOriginal) {
            double tWarped = timeWarp.warp(t);
            timesWarpedWithFetched.push_back(tWarped);
        }
        QVERIFY(monotonic(timesWarpedWithFetched));

        // warp times using the API
        QVector<double> timesWarpedAPI;
        for (const double t : timesOriginal) {
            double tWarped = 0.0;
            QVERIFY(w.warp(msid, t, &tWarped) == kNoErr);
            timesWarpedAPI.push_back(tWarped);
        }
        QVERIFY(monotonic(timesWarpedAPI));

        // Safe to compare reals as we test the assumption that either way they undergo the same
        // series of operations
        QVERIFY(timesWarpedWithFetched == timesWarpedAPI);
    } // end per-sample loop
}

/*! \brief Exercise all filename-addressed APIs */
void MSMultiSampleTimeWarpTest::testAPIOverloads()
{
    QSharedPointer<ProgressBarInterface> progress;
    MSMultiSampleTimeWarp w;

    // First, make sure that we can actually create the warp.
    QVERIFY(w.constructTimeWarp(m_msFilenames, progress) == kNoErr);

    // Test individual samples
    for (const QString &filename : m_msFilenames) {
        // Check base peak
        PlotBase plot;
        QVERIFY(w.getBasePeakPlot(filename, &plot) == kNoErr);
        QVERIFY(static_cast<int>(plot.getPointList().size()) >= m_numberOfPointsToWarp);

        // produce list of points to warp based on plot
        QVector<double> timesOriginal = getNTimes(plot, m_numberOfPointsToWarp);
        QVERIFY(monotonic(timesOriginal));

        // check time warp fetch
        TimeWarp timeWarp;
        QVERIFY(w.getTimeWarp(filename, &timeWarp) == kNoErr);

        // warp times using the fetched time warp
        QVector<double> timesWarpedWithFetched;
        for (const double t : timesOriginal) {
            double tWarped = timeWarp.warp(t);
            timesWarpedWithFetched.push_back(tWarped);
        }
        QVERIFY(monotonic(timesWarpedWithFetched));

        // warp times using the API
        QVector<double> timesWarpedAPI;
        for (const double t : timesOriginal) {
            double tWarped = 0.0;
            QVERIFY(w.warp(filename, t, &tWarped) == kNoErr);
            timesWarpedAPI.push_back(tWarped);
        }
        QVERIFY(monotonic(timesWarpedAPI));

        // Safe to compare reals as we test the assumption that either way they undergo the same
        // series of operations
        QVERIFY(timesWarpedWithFetched == timesWarpedAPI);
    } // end per-sample loop
}

_PMI_END

PMI_TEST_GUILESS_MAIN_WITH_ARGS(pmi::MSMultiSampleTimeWarpTest,
                                QStringList() << "Remote Data Folder")

#include "MSMultiSampleTimeWarpTest.moc"
