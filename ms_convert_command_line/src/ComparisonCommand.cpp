/*
 * Copyright (C) 2017-2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author  : Sam Bogar (sbogar@proteinmetrics.com)
 */

#include "ComparisonCommand.h"
#include "ComparisonStructs.h"
#include "ComparisonWriter.h"
#include "MSComparisonManager.h"
#include <qfileinfo.h>

ComparisonCommand::ComparisonCommand(PMCommandLineParser *parser)
    : MSReaderCommand(parser)
{
}

ComparisonCommand::~ComparisonCommand()
{
}

struct OutputNames {
    QString nonOverlappingFirstFileName;
    QString nonOverlappingSecondFileName;
    QString overlappingFileName;
    QString summaryFileName;
};

void getFileNames(const ComparisonStructs::UserInput &userInput, OutputNames *outputNames)
{
    QFileInfo firstFile(userInput.firstFile);
    QFileInfo secondFile(userInput.secondFile);

    outputNames->nonOverlappingFirstFileName
        = QString("%1%2%3%4").arg(userInput.outputPath).arg("\\SCANS_PRESENT_ONLY_IN_").arg(firstFile.completeBaseName()).arg(".csv");
    outputNames->nonOverlappingSecondFileName = 
        QString("%1%2%3%4").arg(userInput.outputPath).arg("\\SCANS_PRESENT_ONLY_IN_").arg(secondFile.completeBaseName()).arg(".csv");
    outputNames->overlappingFileName = 
        QString("%1%2%3%4%5%6").arg(userInput.outputPath).arg("\\META_DATA_SUMMARY_").arg(firstFile.baseName()).arg("_AND_").arg(secondFile.baseName()).arg(".csv");
    outputNames->summaryFileName = 
        QString("%1%2%3%4%5%6").arg(userInput.outputPath).arg("\\XY_SUMMARY_OF_").arg(firstFile.baseName()).arg("_AND_").arg(secondFile.baseName()).arg(".csv");
}

pmi::Err ComparisonCommand::execute()
{
    pmi::Err e = pmi::kNoErr;

    OutputNames outputNames;

    MSComparisonManager comparisonManager;
    ComparisonWriter comparisonWriter;

    QSet<ComparisonStructs::MetaInfoForScanInBothFiles> scansPresentInBothFiles;
    QSet<ComparisonStructs::MetaInfoForScanInOneFile> scansPresentOnlyInFirstFile;
    QSet<ComparisonStructs::MetaInfoForScanInOneFile> scansPresentOnlyInSecondFile;

    ComparisonStructs::UserInput userInput;

    if (!m_parser->validateAndParseForComparisonCommand()) {
        rrr(pmi::kBadParameterError);
    }
    userInput.writeFull = m_parser->writeFullXYDiff();
    userInput.isCentroided = m_parser->doCentroiding();
    userInput.compareByIndex = m_parser->compareByIndex();

    if (m_parser->sourceFiles().size() == 2) {
        userInput.firstFile = m_parser->sourceFiles().at(0);
        userInput.secondFile = m_parser->sourceFiles().at(1);
        userInput.outputPath = m_parser->outputPath();
    }
    else {
        return pmi::kBadParameterError;
    }

    getFileNames(userInput, &outputNames);
    e = comparisonManager.readFromFiles(userInput.firstFile, userInput.secondFile); ree;
    std::cout << "1" << std::endl;
    e = comparisonManager.findSharedScans(&scansPresentInBothFiles, &scansPresentOnlyInFirstFile,
        &scansPresentOnlyInSecondFile); ree;
    std::cout << "2" << std::endl;
    ComparisonStructs::DiffInfo diffInfo
        = comparisonManager.compare2dPointData(scansPresentInBothFiles, userInput);
    std::cout << "3" << std::endl;
    if (scansPresentOnlyInFirstFile.size() != 0) {
        comparisonWriter.writeScanSetToFile(outputNames.nonOverlappingFirstFileName,
            scansPresentOnlyInFirstFile);
    }
    if (scansPresentOnlyInSecondFile.size() != 0) {
        comparisonWriter.writeScanSetToFile(outputNames.nonOverlappingSecondFileName,
            scansPresentOnlyInSecondFile);
    }
    if (scansPresentInBothFiles.size() != 0) {
        comparisonWriter.writeNativeIDSetToFile(userInput.firstFile, userInput.secondFile,
            outputNames.overlappingFileName,
            scansPresentInBothFiles);
    }

    comparisonWriter.writeSummaryInfoToFile(scansPresentInBothFiles, userInput, diffInfo,
        outputNames.summaryFileName);

    return e;
}
