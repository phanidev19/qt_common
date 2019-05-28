/*
 * Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author  : Michael Georgoulopoulos mgeorgoulopoulos@gmail.com
 */

#include "MSMultiSampleTimeWarp2D.h"

#include <PMiTestUtils.h>

#include <QDir>
#include <QtTest>

_PMI_BEGIN

class MSMultiSampleTimeWarp2DTest : public QObject
{
    Q_OBJECT

public:
    MSMultiSampleTimeWarp2DTest(const QStringList &args);

private Q_SLOTS:
    /*! \brief Exercise all integer-addressed APIs */
    void testMonotony();

    /*! \brief Exercise all filename-addressed APIs */
    void testMonotonyWithSimpleAPI();


private:
    const int m_numberOfPointsToWarp = 10;
    QVector<QString> m_msFilenamesFaux;
    QVector<QString> m_msFilenamesReal;
    QVector<MSMultiSampleTimeWarp2D::MSID> m_msids;
    QDir m_testDataBasePath;
};

MSMultiSampleTimeWarp2DTest::MSMultiSampleTimeWarp2DTest(const QStringList &args)
    : m_testDataBasePath(args[0])
{
    // Gather filenames from test data
    m_msFilenamesReal.push_back(m_testDataBasePath.filePath("JH121614_ProtMet1_T0_Tryp_01.RAW"));
    m_msFilenamesReal.push_back(m_testDataBasePath.filePath("JH121614_ProtMet1_pH8p5_7Day_Tryp_01.RAW"));

    // Add random IDs to test that it works even when indices are not 0, 1, ...
    m_msids.push_back(12345);
    m_msids.push_back(42);

    Q_ASSERT(m_msFilenamesReal.size() == m_msids.size());
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
static QVector<double> getNTimes(const TimedWarpElementList &plot, int n)
{
    QVector<double> ret;

    const int stride = static_cast<int>(plot.size()) / n;
    for (int i = 0; i < static_cast<int>(plot.size()); i += stride) {
        ret.push_back(plot.at(i).timeInMinutes);
    }

    return ret;
}

void MSMultiSampleTimeWarp2DTest::testMonotony()
{
    MSMultiSampleTimeWarp2D w;

    // First, make sure that we can actually create the warp.
    QVERIFY(w.constructTimeWarp(m_msFilenamesReal, m_msids) == kNoErr);

    // Test individual samples
    for (const MSMultiSampleTimeWarp2D::MSID &msid : m_msids) {
        // Check cached plot
        TimedWarpElementList plot;
        QVERIFY(w.getTimedWarpElementList(msid, &plot) == kNoErr);
        QVERIFY(static_cast<int>(plot.size()) >= m_numberOfPointsToWarp);

        // produce list of points to warp based on plot
        QVector<double> timesOriginal = getNTimes(plot, m_numberOfPointsToWarp);
        QVERIFY(monotonic(timesOriginal));

        // check time warp fetch
        TimeWarp2D timeWarp;
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

void MSMultiSampleTimeWarp2DTest::testMonotonyWithSimpleAPI()
{
    MSMultiSampleTimeWarp2D w;

    // First, make sure that we can actually create the warp.
    QVERIFY(w.constructTimeWarp(m_msFilenamesReal) == kNoErr);

    // Test individual samples
    for (const QString &filename : m_msFilenamesReal) {
        // Check cached plot
        TimedWarpElementList plot;
        QVERIFY(w.getTimedWarpElementList(filename, &plot) == kNoErr);
        QVERIFY(w.getTimedWarpElementList(filename, &plot) == kNoErr);
        QVERIFY(static_cast<int>(plot.size()) >= m_numberOfPointsToWarp);

        // produce list of points to warp based on plot
        QVector<double> timesOriginal = getNTimes(plot, m_numberOfPointsToWarp);
        QVERIFY(monotonic(timesOriginal));

        // check time warp fetch
        TimeWarp2D timeWarp;
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

PMI_TEST_GUILESS_MAIN_WITH_ARGS(pmi::MSMultiSampleTimeWarp2DTest,
                                QStringList() << "Remote Data Folder")

#include "MSMultiSampleTimeWarp2DTest.moc"
