/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "dbscan.h"

#include "HillClusterCsvFormatter.h"
#include "MSReader.h"
#include "MSReaderInfo.h"

#include "CsvWriter.h"
#include "MzInterval.h"

#include <ComInitializer.h>

#include <QColor>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>

#include <algorithm>

#define NOMINMAX


using namespace pmi;

static const double mzTolerance = 0.02;
static const double timeTolerance = 2.5;

static const double scanIndexTolerance = 2;

struct MzScanIndexPoint {
    double mz;
    double intensity;
    double *time;
    int scanIndex;

    static const uint dimensions = 2;

    double operator[](int idx) const { 
        if (idx == 0) {
            // return mz adjusted
            return (mz / mzTolerance);
        }

        if (idx == 1) {
            // return time adjusted
            return scanIndex / scanIndexTolerance;
        }

        qFatal("Failure: unexpected dimension requested!");
        return -1.0;
    }
};


Err runDbScan(const QString &filePath) {
    Err e = kNoErr;

    MSReader *ms = MSReader::Instance();
    e = ms->openFile(filePath); ree;

    qDebug() << "Point count" << MSReaderInfo(ms).pointCount(1, true);

    QList<msreader::ScanInfoWrapper> scanInfoList;
    e = ms->getScanInfoListAtLevel(1, &scanInfoList); ree;

    QVector<int> sizes;
    std::vector<MzScanIndexPoint> data = std::vector<MzScanIndexPoint>();

    int firstIndex = 0;
    int lastIndex = scanInfoList.size();

    for (int i = firstIndex; i <= lastIndex; ++i) {
        const msreader::ScanInfoWrapper &scanInfo = scanInfoList.at(i);
        point2dList scanData;
        e = ms->getScanData(scanInfo.scanNumber, &scanData, true); ree;

        MzScanIndexPoint item;
        item.time = const_cast<double *>(&scanInfo.scanInfo.retTimeMinutes);
        item.scanIndex = i;

        for (const point2d &pt : scanData) {
            MzScanIndexPoint newItem = item;
            newItem.mz = pt.x();
            newItem.intensity = pt.y();
            data.push_back(newItem);
        }
    }

    qDebug() << "first point:";
    qDebug() << "mz" << data.at(0)[0] << data.at(0).mz;
    qDebug() << "time or scan index" << data.at(0)[1] << (*data.at(0).time);

    DBSCAN<MzScanIndexPoint, double> dbscan;
    double eps = 1.0;
    uint minPts = 2;

    QElapsedTimer et;
    et.start();
    qDebug() << "Clustering!";
    dbscan.Run(&data, MzScanIndexPoint::dimensions, eps, minPts);
    qDebug() << "Done in" << et.elapsed() << "ms (" << data.size() << " points)";


    const std::vector<uint> &noise= dbscan.Noise;
    const std::vector<std::vector<uint>> &clusters = dbscan.Clusters;

    QFileInfo fi(filePath);
    
    const QString csvFileName = QString("%1_dbscan.csv").arg(fi.completeBaseName());
    const QString csvFilePath = fi.absoluteDir().filePath(csvFileName);

    CsvWriter writer(csvFilePath, "\r\n", ',');
    bool ok = writer.open();
    if (!ok) {
        qWarning() << "Failed to open CSV file.";
        rrr(kError);
    }

    const QStringList header = HillClusterCsvFormatter::header();
    
    ok = writer.writeRow(header);
    
    if (!ok) {
        rrr(kError);
    }

    QStringList row;
    row.reserve(header.size());
    std::generate_n(std::back_inserter(row), header.size(), [] { return QString(); });

    typedef Interval<double> TimeInterval;

    QDebug deb = qDebug();
    int index = 0;

    const double mzRadius = 0.005;
    const double timeRadius = 0.009;

    for (const std::vector<uint> cluster : clusters) {
        // compute bounding box and max intensity
        double maxIntensity = std::numeric_limits<double>::lowest();
        uint maxIntensityIndex = -1;
        
        MzInterval mz;
        TimeInterval time;
        
        for (uint item : cluster) {
            if (data[item].intensity > maxIntensity) {
                maxIntensity = data[item].intensity;
                maxIntensityIndex = item;
            }

            double candidateMz = data[item].mz;
            double candidateTime = (*data[item].time);

            if (mz.isNull()) {
                mz.rstart() = candidateMz;
                mz.rend() = candidateMz;
            } else {
                mz.rstart() = std::min(mz.start(), candidateMz);
                mz.rend() = std::max(mz.end(), candidateMz);
            }

            if (time.isNull()) {
                time.rstart() = candidateTime;
                time.rend() = candidateTime;
            } else {
                time.rstart() = std::min(time.start(), candidateTime);
                time.rend() = std::max(time.end(), candidateTime);
            }
        }
        
        if (mz.start() == mz.end()) {
            mz.rstart() = mz.start() - 0.01;
            mz.rend() = mz.end() + 0.01;
        }

        if (time.start() == time.end()) {
            time.rend() += timeRadius;
        }

        // store cluster

        const QColor clusterStrokeColor = QColor(Qt::black);

        int colorIndex = (index % 17) + 2;
        const QColor clusterFillColor = QColor(static_cast<Qt::GlobalColor>(colorIndex));

        row[HillClusterEntryColumns::MZ_START] = QString::number(mz.start(), 'g', 9);
        row[HillClusterEntryColumns::MZ_END] = QString::number(mz.end(), 'g', 9);
        row[HillClusterEntryColumns::TIME_START] = QString::number(time.start(), 'g', 9);
        row[HillClusterEntryColumns::TIME_END] = QString::number(time.end(), 'g', 9);
        row[HillClusterEntryColumns::LABEL] = QString("c");
        row[HillClusterEntryColumns::INTENSITY] = QString::number(maxIntensity, 'g', 9);
        row[HillClusterEntryColumns::STROKE] = clusterStrokeColor.name(QColor::HexRgb);
        row[HillClusterEntryColumns::FILL] = clusterFillColor.name(QColor::HexRgb);
        row[HillClusterEntryColumns::POINT_COUNT] = QString::number(cluster.size());

        writer.writeRow(row);

        // blue points are rendered as separate rects 

        MzInterval bluePointMz;
        TimeInterval bluePointTime;
        
        const double intensity = data[maxIntensityIndex].intensity;
        bluePointMz.rstart() = data[maxIntensityIndex].mz - mzRadius;
        bluePointMz.rend() = data[maxIntensityIndex].mz + mzRadius;
        
        bluePointTime.rstart() = (*data[maxIntensityIndex].time);
        bluePointTime.rend() = (*data[maxIntensityIndex].time) + timeRadius;

        row[HillClusterEntryColumns::MZ_START] = QString::number(bluePointMz.start(), 'g', 9);
        row[HillClusterEntryColumns::MZ_END] = QString::number(bluePointMz.end(), 'g', 9);
        row[HillClusterEntryColumns::TIME_START] = QString::number(bluePointTime.start(), 'g', 9);
        row[HillClusterEntryColumns::TIME_END] = QString::number(bluePointTime.end(), 'g', 9);
        row[HillClusterEntryColumns::LABEL] = QString("pt");
        row[HillClusterEntryColumns::INTENSITY] = QString::number(intensity, 'g', 9);

        const QColor bluePointColor = QColor(Qt::blue);
        row[HillClusterEntryColumns::STROKE] = bluePointColor.name(QColor::HexRgb);
        row[HillClusterEntryColumns::FILL] = bluePointColor.name(QColor::HexRgb);

        writer.writeRow(row);

        index++;
    }
    
    // identify clusters 

    return e;
}

int main(int argc, char *argv[])
{
    ComInitializer comInitializer;
    QCoreApplication app(argc, argv);

    QCommandLineParser parser;

    parser.addHelpOption();
    parser.addPositionalArgument(QLatin1String("inputFile"),
        QString("Input file containing MS data, supported by MSReader (.raw, .d etc.)"));

    parser.process(app);

    const QStringList args = parser.positionalArguments();
    if (args.count() != 1) {
        parser.showHelp(1);
    }

    QString filePath = args.at(0);
    if (!QFileInfo::exists(filePath)) {
        parser.showHelp(1);
    }

    Err e = runDbScan(filePath);
    if (e != kNoErr) {
        qWarning() << "We failed with " << QString::fromStdString(pmi::convertErrToString(e));
        return 1;
    }
   
    return 0;
}
