/*
 *  Copyright (C) 2017 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author  : Yashpalsinh Gohil (yash.gohiil@gmail.com)
 */

#include "ExtractMassIntensityCommand.h"
#include "MSReader.h"
#include <QDir>
#include <QStringList>

ExtractMassIntensityCommand::ExtractMassIntensityCommand(PMCommandLineParser *parser)
    : MSReaderCommand(parser)
{
}

ExtractMassIntensityCommand::~ExtractMassIntensityCommand()
{
}

pmi::Err ExtractMassIntensityCommand::execute()
{
    if (!m_parser->validateAndParseForMassIntensityCommand()) {
        return pmi::kBadParameterError;
    }

    QString outputPath(m_parser->outputPath());
    QDir dir(outputPath);
    long totalScan;
    long startScan;
    long endScan;

    if (!dir.exists()) {
        if (!dir.mkpath(outputPath)) {
            qWarning() << "Unable to create directory";
            return pmi::kError;
        }
    }

    pmi::MSReader *reader = pmi::MSReader::Instance();
    pmi::Err e = reader->openFile(m_parser->sourceFile());
    ree;

    e = reader->getNumberOfSpectra(&totalScan, &startScan, &endScan);
    ree;

    long start = m_parser->startScan();
    if (start < startScan && start != -1) {
        qWarning() << "Start scan number should be higher than" << startScan;
        return pmi::kBadParameterError;
    }

    if (start != -1) {
        startScan = start;
    }

    long end = m_parser->endScan();
    if (end > endScan) {
        qWarning() << "End scan number should be less than" << endScan;
        return pmi::kBadParameterError;
    }

    if (end != -1) {
        endScan = end;
    }

    bool centroid = m_parser->doCentroiding();
    long padding = log10(totalScan) + 1;

    for (int index = startScan; index <= endScan; ++index) {
        pmi::msreader::ScanInfo scanInfo;
        e = reader->getScanInfo(index, &scanInfo);
        ree;

        pmi::point2dList points;
        e = reader->getScanData(index, &points, centroid);
        ree;

        const QString fileName(QString("Sample_%1-%2.csv")
                                   .arg(scanInfo.scanLevel)
                                   .arg(index, padding, 10, QChar('0')));
        const QString filePath(QDir(outputPath).filePath(fileName));

        e = pmi::PlotBase(points).saveDataToFile(filePath);
        ree;
        qInfo() << "File created" << filePath;
    }

    return e;
}
