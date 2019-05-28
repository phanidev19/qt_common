/*
 * Copyright (C) 2019 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#include <MSReader.h>

#include "ComInitializer.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QScopedPointer>

using namespace pmi;
using namespace pmi::msreader;

#define PMI_MANUAL_VERIFY(Expr) if (!Expr) { qDebug() << "#Expr is false"; return; }


QStringList testGetRandomXICData_data()
{
    return QStringList()
        << QStringLiteral("P:\\PMI-Dev\\J\\LT\\4200\\LT-4234\\2019-01-31 Weijia Tang SlowIntact\\WT_20190110_SHP643_GMP5_01.raw")
        << QStringLiteral("P:\\PMI-Dev\\J\\LT\\4200\\LT-4234\\2019-01-31 Weijia Tang SlowIntact\\WT_20190110_SHP643_GMP5_02.raw")
        << QStringLiteral("P:\\PMI-Dev\\Managed\\Data\\VendorData\\Thermo\\TestMab.CID_Trypsin_1.raw")
        << QStringLiteral("P:\\PMI-Dev\\Managed\\Data\\MAM.NIST\\NISTmAb_Control_MS1.raw")
        << QStringLiteral("P:\\PMI-Dev\\Managed\\Data\\MAM.NIST\\NISTmAb_Control_MS2.raw");
}

double boundedRand(double lower, double higher)
{
    return (static_cast<double>(qrand()) / RAND_MAX) * (higher - lower) + lower;
}

void testGetRandomXICData(const QString& msFilePath)
{
    PMI_MANUAL_VERIFY(QFileInfo::exists(msFilePath));

    MSReader *reader = MSReader::Instance();
    Err e = reader->openFile(msFilePath);
    PMI_MANUAL_VERIFY(e == kNoErr);

    msreader::XICWindow xic_win;

    const int LOOP_COUNT = 100;

    long firstScanNumber = 0;
    long lastScanNumber = 0;
    long totalNumber = 0;
    reader->getNumberOfSpectra(&totalNumber, &firstScanNumber, &lastScanNumber);

    double minTime = 0;
    double maxTime = 0;
    reader->getTimeDomain(&minTime, &maxTime);
    double minMass = 200;
    double maxMass = 2000;


    qDebug() << "File: " << msFilePath;
    qDebug() << "   Scan range: " << firstScanNumber << " - " << lastScanNumber;
    qDebug() << "   Time range: " << minTime << " - " << maxTime;
    qDebug();
    qDebug() << "   " << LOOP_COUNT << " calls to GetChroData";
    qDebug();

    QElapsedTimer et;
    et.start();

    for (int i = 0; i < LOOP_COUNT; ++i)
    {
        xic_win.time_start = boundedRand(minTime, maxTime);
        xic_win.time_end = boundedRand(xic_win.time_start, maxTime);
        xic_win.mz_start = boundedRand(minMass, maxMass);
        xic_win.mz_end = boundedRand(xic_win.mz_start, maxMass);

        point2dList data;
        Err e = reader->getXICData(xic_win, &data);
        PMI_MANUAL_VERIFY(e == kNoErr);
    }

    qDebug() << "Random windows:" << et.elapsed() << "ms";

    const int fractions[] = { 10, 25, 50 };
    for(int fraction : fractions)
    {
        et.restart();

        double minTimeRange = 1;
        double maxTimeRange = std::max(minTimeRange, (maxTime - minTime) / fraction);
        for (int i = 0; i < LOOP_COUNT; ++i)
        {
            xic_win.time_start = boundedRand(minTime, maxTime);
            xic_win.time_end = std::min(maxTime, xic_win.time_start + std::max(minTimeRange, boundedRand(0, maxTimeRange)));
            xic_win.mz_start = boundedRand(minMass, maxMass);
            xic_win.mz_end = boundedRand(xic_win.mz_start, maxMass);

            point2dList data;
            Err e = reader->getXICData(xic_win, &data);
            PMI_MANUAL_VERIFY(e == kNoErr);
        }

        qDebug() << "Random windows of < 1/" << fraction << " of the time range: " << et.elapsed() << "ms";
    }

    reader->releaseInstance();
}


int main(int argc, char* argv[])
{
    pmi::ComInitializer comInitializer;
        
    QCoreApplication app(argc, argv);

    const QStringList testCases = testGetRandomXICData_data();
    for (auto test : testCases) {
        testGetRandomXICData(test);
    }
}
