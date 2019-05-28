/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

// common_core_mini
#include <CsvWriter.h>
#include <PMiTestUtils.h>
#include <PlotBase.h>
#include <PlotBaseUtils.h>
#include <RowNode.h>
#include <RowNodeOptionsCore.h>
#include <PmiQtCommonConstants.h>

// common_stable
#include <pmi_core_defs.h>

#include "ZScorePeakDetector.h"

#include <QFileInfo>
#include <QtTest>

_PMI_BEGIN

class ZScorePeakDetectorTest : public QObject
{
    Q_OBJECT

public:
    explicit ZScorePeakDetectorTest(const QStringList &args)
        : m_testDataBasePath(args[0]) {
    }

private Q_SLOTS:
    // TODO: improve test to verify the peaks , right now it is tool for manual testing of the peak
    // finding
    void testFindPeaks();

private:
    QDir m_testDataBasePath;
};

void ZScorePeakDetectorTest::testFindPeaks()
{
    QSKIP("Not production ready: zScore needs to be tweaked better; used as manual test");

    // XIC window dumped using PlotBase::saveDataToFile
    QString fileName = QString("xic_01.csv");
    // time where we are looking for integration bounds
    double time = 110.814;

    PlotBase pb;
    QString testFile = m_testDataBasePath.filePath(fileName);
    QCOMPARE(pb.getDataFromFile(testFile), kNoErr);
    point2dList xicData = pb.getPointList();

    // values taken from Byomap's defaults
    RowNode options;
    rn::setOption(options, kpeak_interval_time_min, 0.5);
    rn::setOption(options, kpeak_tol_max, 0.005);
    rn::setOption(options, kpeak_tol_min, 0.0005);

    point2dIdxList xstart;
    point2dIdxList xend;
    point2dIdxList xapex;
    Err e = makePeaks2(pb, options, xstart, xend, xapex);
    QCOMPARE(e, kNoErr);

    auto lower
        = std::lower_bound(xicData.begin(), xicData.end(), point2d(time, 0.0), point2d_less_x);
    int index = std::distance(xicData.begin(), lower);
    QVERIFY(index >= 0);
    QVERIFY(index < static_cast<int>(xicData.size()));

    Interval<int> makePeaksResult;
    QVector<Interval<int>> intervalsMakePeaks;
    for (int i = 0; i < static_cast<int>(xstart.size()); ++i) {
        Interval<int> item;
        item.rstart() = xstart.at(i);
        item.rend() = xend.at(i);
        intervalsMakePeaks.push_back(item);

        if (item.contains(index)) {
            makePeaksResult = item;
        }
    }

    std::vector<double> makePeaksResultScore = ZScorePeakDetector::toZScoreSignal(
        QVector<Interval<int>>{ makePeaksResult }, static_cast<int>(xicData.size()));
    std::vector<double> makePeaksScore
        = ZScorePeakDetector::toZScoreSignal(intervalsMakePeaks, static_cast<int>(xicData.size()));

    ZScoreSettings settings;
    settings.threshold = 4.0;
    settings.influence = 0.015;
    settings.lag = qCeil(0.25 * xicData.size());

    QVector<Interval<int>> peakIndexes = ZScorePeakDetector::findPeaks(xicData, settings);
    std::vector<double> signal = ZScorePeakDetector::zScore(xicData, settings);

    std::vector<double> intervalScore
        = ZScorePeakDetector::toZScoreSignal(peakIndexes, static_cast<int>(xicData.size()));
    QCOMPARE(signal, intervalScore);

    // export to CSV
    QString outputFileName
        = QString("%1%2").arg(QFileInfo(fileName).completeBaseName(), "_debugInfo.csv");

    CsvWriter writer(m_testDataBasePath.filePath(outputFileName));
    QVERIFY(writer.open());

    QStringList csvRow = { "time",     "intensity",  "zScoreOriginal", "zScoreScaled",
                           "interval", "makePeaks2", "makePeaksResult" };

    writer.writeRow(csvRow);

    auto max = std::max_element(xicData.begin(), xicData.end(), point2d_less_y);
    for (size_t i = 0; i < xicData.size(); ++i) {
        csvRow[0] = QString::number(xicData.at(i).x());
        csvRow[1] = QString::number(xicData.at(i).y());
        csvRow[2] = QString::number(signal.at(i));
        csvRow[3] = QString::number(signal.at(i) * max->ry());
        csvRow[4] = QString::number(intervalScore.at(i) * max->ry());
        csvRow[5] = QString::number(makePeaksScore.at(i) * max->ry());
        csvRow[6] = QString::number(makePeaksResultScore.at(i) * max->ry());
        writer.writeRow(csvRow);
    }
}

_PMI_END

PMI_TEST_GUILESS_MAIN_WITH_ARGS(pmi::ZScorePeakDetectorTest, QStringList() << "Remote Data Folder")

#include "ZScorePeakDetectorTest.moc"
