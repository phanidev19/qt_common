/*
 *  Copyright (C) 2017 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author  : Yashpalsinh Gohil (yash.gohiil@gmail.com)
 */

#include "ExtractCalibrationPointsCommand.h"
#include "MSReader.h"
#include <QDir>

namespace
{
const char COMMA_CHAR = ',';
const char *NEW_LINE = "\r\n";
}

ExtractCalibrationPointsCommand::ExtractCalibrationPointsCommand(PMCommandLineParser *parser)
    : MSReaderCommand(parser)
{
}

pmi::Err ExtractCalibrationPointsCommand::execute()
{
    if (!m_parser->validateAndParseForCalibrationPointsCommand()) {
        return pmi::kBadParameterError;
    }

    pmi::Err e = pmi::kNoErr;
    QString outputPath(m_parser->outputPath());
    QDir dir(outputPath);

    if (!dir.exists()) {
        if (!dir.mkpath(outputPath)) {
            qWarning() << "Unable to create directory";
            return pmi::kError;
        }
    }

    QList<double> massList = m_parser->lockMassList();
    double tolerance = m_parser->tolerance();

    QString sourceFile(m_parser->sourceFile());

    pmi::MSReader *reader = pmi::MSReader::Instance();
    e = reader->openFile(sourceFile);
    ree;

    QList<pmi::msreader::ScanInfoWrapper> lockMassList;
    e = reader->getScanInfoListAtLevel(1, &lockMassList);
    ree;

    const QString filePath(QDir(outputPath).filePath("Sample.csv"));

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        e = pmi::kFileOpenError;
        ree;
    }

    QTextStream out(&file);
    out.setRealNumberPrecision(15);

    out << "Scan number,time";

    for (double mass : massList) {
        out << COMMA_CHAR << "input:" << mass << COMMA_CHAR << "Intensity" << COMMA_CHAR
            << "Difference";
    }

    out << NEW_LINE;

    for (const pmi::msreader::ScanInfoWrapper &lockMass : lockMassList) {
        pmi::point2dList points;
        e = reader->getScanData(lockMass.scanNumber, &points, true);
        ree;

        out << lockMass.scanNumber << COMMA_CHAR << lockMass.scanInfo.retTimeMinutes;

        for (double mass : massList) {
            double mzTolerance = mass / 1000000 * tolerance;
            double massMin = mass - mzTolerance;
            double massMax = mass + mzTolerance;

            pmi::point2dList pointList
                = pmi::PlotBase(points).getPointsBetween(massMin, massMax, true);

            if (pointList.empty()) {
                out << COMMA_CHAR << COMMA_CHAR << COMMA_CHAR;
                continue;
            }

            QPointF highestPeak = pointList[0];

            for (const QPointF &point : pointList) {
                if (highestPeak.y() < point.y()) {
                    highestPeak = point;
                }
            }

            out << COMMA_CHAR << highestPeak.x() << COMMA_CHAR << highestPeak.y() << COMMA_CHAR
                << highestPeak.x() - mass;
        }
        out << NEW_LINE;
    }
    file.close();
    return e;
}
