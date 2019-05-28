/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
* Author: Andrew Nichols anichols@proteinmetrics.com
*/

#include "FeatureFinderParameters.h"
#include "MonoisotopeDeterminator.h"
#include "MonoisotopeDeterminatorNN.h"
#include "PMiTestUtils.h"
#include "PlotBase.h"
#include "Point2dListUtils.h"

// stable
#include <pmi_core_defs.h>

#include <QtTest>
#include "ChargeDeterminatorNN.h"
#include "AveragineGenerator.h"

_PMI_BEGIN

static const QLatin1String TEST_ROOT_FOLDER_NAME("MonoisotopeDeterminatorNNTest");

class MonoisotopeDeterminatorNNTest : public QObject
{
    Q_OBJECT
public:
    explicit MonoisotopeDeterminatorNNTest(const QStringList &args)
        : m_testDataBasePath(args[0])
    {
    }

private Q_SLOTS :
    void testDetermineMonoisotope();
    void testDetermineMonoisotope_data();

    void testDetermineMonoisotopeFromAveragine();

private:
    QDir m_testDataBasePath;
};

void MonoisotopeDeterminatorNNTest::testDetermineMonoisotope_data()
{
    QTest::addColumn<QString>("scanDataCsvFileName");
    QTest::addColumn<QVector<QPointF>>("selectedIons");
    QTest::addColumn<int>("expectedCharge");
    QTest::addColumn<QVector<int>>("expectedOffset");

    // one cluster with charge 4
    QTest::newRow("cona-charge4-3points")
        << QString("cona_tmt0saxpdetd_centroided_scan_3471.csv")
        << QVector<point2d>({ point2d(611.858093261719, 8331401),
            point2d(612.108032226563, 6248811.5),
            point2d(612.358459472656, 3552560.5) })
        << 4 << QVector<int>({ 1, 2, 3 });

    QTest::newRow("cona-charge5-1points")
        << QString("cona_tmt0saxpdetd_centroided_scan_3471.csv")
        << QVector<point2d>({ point2d(760.375305175781, 703492.375),
            point2d(760.575622558594, 1559419.625),
            point2d(760.775451660156, 1822599.5) })
        << 5 << QVector<int>({ 0, 1, 2 });

    QTest::newRow("cona-charge3-1point")
        << QString("cona_tmt0saxpdetd_centroided_scan_2336.csv")
        << QVector<point2d>({ point2d(771.335021972656, 30009094) }) << 3 << QVector<int>({ 1 });
}

void MonoisotopeDeterminatorNNTest::testDetermineMonoisotope()
{
    ImmutableFeatureFinderParameters ffParams;

    QFETCH(QString, scanDataCsvFileName);
    QString scanDataFilepath = m_testDataBasePath.filePath(scanDataCsvFileName);

    QVERIFY(QFileInfo(scanDataFilepath).isReadable());

    PlotBase pb;
    pb.getDataFromFile(scanDataFilepath);

    point2dList scanData = pb.getPointList();
    QVERIFY(!scanData.empty());

    QFETCH(QVector<QPointF>, selectedIons);
    QFETCH(QVector<int>, expectedOffset);

    QString neuralNetworkDbFilePath = QDir(qApp->applicationDirPath()).filePath("nn_weights.db");
    QVERIFY(QFileInfo(neuralNetworkDbFilePath).isReadable());

    MonoisotopeDeterminatorNN monoDeterminator;
    Err e = monoDeterminator.init(neuralNetworkDbFilePath);
    QCOMPARE(e, kNoErr);

    ChargeDeterminatorNN chargeDeterm;
    e = chargeDeterm.init(neuralNetworkDbFilePath);
    QCOMPARE(e, kNoErr);

    for (int i = 0; i < selectedIons.size(); ++i) {
        const QPointF &selectedIon = selectedIons.at(i);

        auto item = std::find(scanData.begin(), scanData.end(), selectedIon);
        QVERIFY(item != scanData.end());

        int index = std::distance(scanData.begin(), item);

        double mzStart = selectedIon.x() - ffParams.apexChargeClustering;
        double mzEnd = selectedIon.x() + ffParams.apexChargeClustering;

        point2dList relevantData
            = Point2dListUtils::extractPointsIncluding(scanData, mzStart, mzEnd);

        // uncomment to observe the scan part that was used to determine monoisotope
        //#define DUMP_SCAN_PART
#ifdef DUMP_SCAN_PART
        PlotBase(relevantData).saveDataToFile(m_testDataBasePath.filePath("scanPart.csv"));
#endif

        int charge = chargeDeterm.determineCharge(relevantData, selectedIon.x());
        QFETCH(int, expectedCharge);
        QCOMPARE(charge, expectedCharge);

        double score = 0.0;
        int offset = monoDeterminator.determineMonoisotopeOffset(scanData, selectedIon.x(),
            charge, &score);

        if (offset != expectedOffset.at(i)) {
            qDebug() << "mz"
                << MonoisotopeDeterminator::fromOffsetToMz(offset, selectedIon.x(), charge);
            qDebug() << "score" << score;
        }

        QCOMPARE(offset, expectedOffset.at(i));
    }
}

void MonoisotopeDeterminatorNNTest::testDetermineMonoisotopeFromAveragine()
{
    QString neuralNetworkDbFilePath = QDir(qApp->applicationDirPath()).filePath("nn_weights.db");
    QVERIFY(QFileInfo::exists(neuralNetworkDbFilePath));

    MonoisotopeDeterminatorNN monoDeterminator;
    Err e = monoDeterminator.init(neuralNetworkDbFilePath);
    QCOMPARE(e, kNoErr);

    int mzStart = 200;
    int mzEnd = 2000;

    const int massStart = 400;
    const int massEnd = 9000;
    const int massStep = 100;

    AveragineGenerator generator(massStart);

    int testCombinations = 0;

    for (int mass = massStart; mass < massEnd; mass += massStep) {

        generator.setMass(mass);

        for (int charge = 1; charge <= 10; ++charge) {
            const double monoIsotopeMz = generator.computeMonoisotopeMz(mass, charge);
            
            if ((monoIsotopeMz > mzStart) && (monoIsotopeMz < mzEnd)) {
                double maxMz;
                int expectedMonoisotopeOffset;
                generator.getMaxMzAndMonoisotopeOffset(charge, &maxMz, &expectedMonoisotopeOffset);

                const point2dList data = generator.generateSignal(charge);

                double score = 0.0;
                int monoOffset = monoDeterminator.determineMonoisotopeOffset(
                    data, maxMz, static_cast<int>(charge), &score);

                if (monoOffset != expectedMonoisotopeOffset) {
                    qDebug() << "Actual monoisotope" << monoOffset
                             << "Expected monoisotope:" << expectedMonoisotopeOffset;
                }

                QCOMPARE(monoOffset, expectedMonoisotopeOffset);
                testCombinations++;
            }
        }
    }

    qDebug() << testCombinations << "mass/charge combinations verified!";
}

_PMI_END

PMI_TEST_GUILESS_MAIN_WITH_ARGS(pmi::MonoisotopeDeterminatorNNTest,
                                QStringList() << "Remote Data Folder")

#include "MonoisotopeDeterminatorNNTest.moc"
