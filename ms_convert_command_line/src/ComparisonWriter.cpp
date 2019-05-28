/*
 * Copyright (C) 2017-2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author  : Sam Bogar (sbogar@proteinmetrics.com)
 */

#include "ComparisonWriter.h"
#include <qfileinfo.h>

enum ColumnTitlesIndices {
    retTimeIndex = 0,
    scanLevelIndex,
    scanMethodIndex,
    dIsolationMassIndex,
    dMonoIsoMassIndex,
    lowerWindowOffsetIndex,
    upperWindowOffsetIndex,
    nChargeStateIndex,
    nScanNumberIndex,
    ColumnTitlesIndicesLength
};

enum ColumnDataTypes { isInt, isFloat };

void ComparisonWriter::putMetaInfoIntoQStringList(
    const ComparisonStructs::MetaInfoForScanInOneFile &MetaInfoForScanInOneFile, QStringList *list)
{
    *list << MetaInfoForScanInOneFile.scanInfo.nativeId
          << QString::number(MetaInfoForScanInOneFile.scanInfo.retTimeMinutes)
          << QString::number(MetaInfoForScanInOneFile.scanInfo.scanLevel)
          << QString::number(MetaInfoForScanInOneFile.scanInfo.scanMethod)
          << QString::number(MetaInfoForScanInOneFile.precursorInfo.dIsolationMass)
          << QString::number(MetaInfoForScanInOneFile.precursorInfo.dMonoIsoMass)
          << QString::number(MetaInfoForScanInOneFile.precursorInfo.lowerWindowOffset)
          << QString::number(MetaInfoForScanInOneFile.precursorInfo.upperWindowOffset)
          << QString::number(MetaInfoForScanInOneFile.precursorInfo.nChargeState)
          << QString::number(MetaInfoForScanInOneFile.precursorInfo.nScanNumber);
}

void ComparisonWriter::writeScanSetToFile(const QString &fileName,
    const QSet<ComparisonStructs::MetaInfoForScanInOneFile> &source)
{
    QList<QStringList> listList;
    QStringList firstRow;

    firstRow << "native ID"
             << "retTimeMinutes"
             << "scanLevel"
             << "scanMethod"
             << "dIsolationMass"
             << "dMonoIsoMass"
             << "lowerWindowOffset"
             << "upperWindowOffset"
             << "nchargeState"
             << "nScanNumber";
    listList.append(firstRow);
    for (QSet<ComparisonStructs::MetaInfoForScanInOneFile>::const_iterator iterator
         = source.begin();
         iterator != source.end(); iterator++) {
        QStringList stringList;

        putMetaInfoIntoQStringList(*iterator, &stringList);
        listList.append(stringList);
    }
    pmi::CsvWriter::writeFile(fileName, listList, "\n", ',');
}

ColumnDataTypes returnValueType(ColumnTitlesIndices value)
{
    if (value == scanLevelIndex || value == scanMethodIndex || value == lowerWindowOffsetIndex
        || value == upperWindowOffsetIndex || value == nChargeStateIndex
        || value == nScanNumberIndex) {
        return isInt;
    } else {
        return isFloat;
    }
}

void checkForPointsOfMaxDiff(const QStringList &diffStringList, QList<QStringList> pointsOfMaxDiff,
                             QString nativeID)
{
    std::cout << "A" << std::endl;
    for (int i = 0; i < ColumnTitlesIndicesLength; i++) {
        if (returnValueType((ColumnTitlesIndices)i) == isFloat
                ? diffStringList[i + 2].toFloat() > pointsOfMaxDiff[i][2].toFloat()
                : diffStringList[i + 2].toInt() > pointsOfMaxDiff[i][2].toInt()) {
            pointsOfMaxDiff[i][1] = nativeID;
            pointsOfMaxDiff[i][2] = diffStringList[i + 2];
        }
    }
    std::cout << "B" << std::endl;
}

void instantiatePointsOfMaxDiff(QList<QStringList> pointsOfMaxDiff)
{
    pointsOfMaxDiff 
        << (QStringList() << "Greatest retTimeMinutes diff:"
            << ""
            << "")
        << (QStringList() << "Greatest scanLevel diff"
            << ""
            << "")
        << (QStringList() << "Greatest scanMethod diff"
            << ""
            << "")
        << (QStringList() << "Greatest dIsolationMass diff"
            << ""
            << "")
        << (QStringList() << "Greatest dMonoIsoMass diff"
            << ""
            << "")
        << (QStringList() << "Greatest lowerWindowOffset diff"
            << ""
            << "")
        << (QStringList() << "Greatest upperWindowOffset diff"
            << ""
            << "")
        << (QStringList() << "Greatest nchargeState diff"
            << ""
            << "")
        << (QStringList() << "Greatest nScanNumber diff"
            << ""
            << "");
}

/*
** TO-DO:
**  resolve index-out-of-range error for meta-data greatest diff extraction
**  (new as of switching from array to QList<QStringList>)
*/

void ComparisonWriter::writeNativeIDSetToFile(
    const QString &firstFilePath, const QString &secondFilePath, const QString &outputFileName,
    const QSet<ComparisonStructs::MetaInfoForScanInBothFiles> &source)
{
    QFileInfo firstFile(firstFilePath);
    QFileInfo secondFile(secondFilePath);
    QList<QStringList> listList;
    QStringList firstRow;
    QList<QStringList> pointsOfMaxDiff;

    instantiatePointsOfMaxDiff(pointsOfMaxDiff);

    firstRow << "file name"
             << "native ID"
             << "retTimeMinutes"
             << "scanLevel"
             << "scanMethod"
             << "dIsolationMass"
             << "dMonoIsoMass"
             << "lowerWindowOffset"
             << "upperWindowOffset"
             << "nchargeState"
             << "nScanNumber";
    listList.append(firstRow);

    QString firstFileName = firstFile.completeBaseName();
    QString secondFileName = secondFile.completeBaseName();
    for (QSet<ComparisonStructs::MetaInfoForScanInBothFiles>::const_iterator iterator
         = source.begin();
         iterator != source.end(); iterator++) {
        QStringList firstStringList;
        QStringList secondStringList;
        QStringList diffStringList;

        firstStringList << firstFileName;
        putMetaInfoIntoQStringList(iterator->firstFileMetaInfo, &firstStringList);
        listList.append(firstStringList);
        secondStringList << secondFileName;
        putMetaInfoIntoQStringList(iterator->secondFileMetaInfo, &secondStringList);
        listList.append(secondStringList);
        diffStringList << "difference:";
        putMetaInfoDiffIntoQStringList(*iterator, &diffStringList);
        //checkForPointsOfMaxDiff(diffStringList, pointsOfMaxDiff, iterator->nativeId);
        listList.append(diffStringList);
    }
    listList.prepend(QStringList() << "");
   /* for (int i = ColumnTitlesIndicesLength - 1; i >= 0; i--) {
        listList.prepend(pointsOfMaxDiff[i]);
    }*/
    listList.prepend(QStringList() << ""
                                   << "native ID"
                                   << "Quantity of Difference");

    pmi::CsvWriter::writeFile(outputFileName, listList, "\n", ',');
}

void ComparisonWriter::putMetaInfoDiffIntoQStringList(
    const ComparisonStructs::MetaInfoForScanInBothFiles &scan, QStringList *list)
{
    *list << "NULL"
          << QString::number(scan.secondFileMetaInfo.scanInfo.retTimeMinutes
                             - scan.firstFileMetaInfo.scanInfo.retTimeMinutes)
          << QString::number(scan.secondFileMetaInfo.scanInfo.scanLevel
                             - scan.firstFileMetaInfo.scanInfo.scanLevel)
          << QString::number(scan.secondFileMetaInfo.scanInfo.scanMethod
                             - scan.firstFileMetaInfo.scanInfo.scanMethod)
          << QString::number(scan.secondFileMetaInfo.precursorInfo.dIsolationMass
                             - scan.firstFileMetaInfo.precursorInfo.dIsolationMass)
          << QString::number(scan.secondFileMetaInfo.precursorInfo.dMonoIsoMass
                             - scan.firstFileMetaInfo.precursorInfo.dMonoIsoMass)
          << QString::number(scan.secondFileMetaInfo.precursorInfo.lowerWindowOffset
                             - scan.firstFileMetaInfo.precursorInfo.lowerWindowOffset)
          << QString::number(scan.secondFileMetaInfo.precursorInfo.upperWindowOffset
                             - scan.firstFileMetaInfo.precursorInfo.upperWindowOffset)
          << QString::number(scan.secondFileMetaInfo.precursorInfo.nChargeState
                             - scan.firstFileMetaInfo.precursorInfo.nChargeState)
          << QString::number(scan.secondFileMetaInfo.precursorInfo.nScanNumber
                             - scan.firstFileMetaInfo.precursorInfo.nScanNumber);
}

void ComparisonWriter::writeSummaryInfoToFile(
    const QSet<ComparisonStructs::MetaInfoForScanInBothFiles> &scansPresentInBothFiles,
    const ComparisonStructs::UserInput &userInput, const ComparisonStructs::DiffInfo &diffInfo,
    const QString fileName)
{
    QString firstFile = QFileInfo(userInput.firstFile).baseName();
    QString secondFile = QFileInfo(userInput.secondFile).baseName();
    ComparisonStructs::MetaInfoCompare metaInfoCompareForOne;
    pmi::CsvWriter csvWriter(fileName, "\n", ',');
    bool XYIsDifferent = false;

    if (firstFile == secondFile) {
        firstFile = QFileInfo(userInput.firstFile).completeBaseName();
        secondFile = QFileInfo(userInput.secondFile).completeBaseName();
    }

    csvWriter.open();
    if (userInput.compareByIndex || !userInput.isCentroided) {
        csvWriter.writeRow(QStringList()
                            << (!userInput.isCentroided ? "Scan with greatest difference in y:"
                                                        : "scan with greatest difference in x:"));
        csvWriter.writeRow(QStringList() << "native id"
                                          << "x/y coordinate of greatest diff"
                                          << "difference in x at point"
                                          << "difference in y at point");
        csvWriter.writeRow(QStringList()
                            << diffInfo.scanWithGreatestDiff.nativeId
                            << QString::number(diffInfo.coordinateOfBiggestDiffInFile.x()) + "/"
                                + QString::number(diffInfo.coordinateOfBiggestDiffInFile.y())
                            << QString::number(diffInfo.biggestDiffInFile.x())
                            << QString::number(diffInfo.biggestDiffInFile.y()));
    }
    QStringList columnTitles;
    columnTitles << "filename"
                 << "native ID"
                 << "retTimeMinutes"
                 << "scanLevel"
                 << "scanMethod"
                 << "dIsolationMass"
                 << "dMonoIsoMass"
                 << "lowerWindowOffset"
                 << "upperWindowOffset"
                 << "nchargeState"
                 << "nScanNumber"
                 << "x/y similar?";
    if (userInput.isCentroided && userInput.compareByIndex) {
        columnTitles << "x coordinate of greatest x-diff"
                     << "y coordinate of greatest x-diff"
                     << "max x diff"
                     << "y-diff at max x diff point"
                     << "normalized y-diff (y - diff / total y)";
    } else {
        columnTitles << "x coordinate of greatest y-diff"
                     << "y coordinate of greatest y-diff"
                     << "max y diff"
                     << "normalized y-diff (y - diff / total y)"
                     << "x/y of greatest y-diff where there neither value is 0"
                     << "y-diff quantity"
                     << "normalized y-diff";
    }
    csvWriter.writeRow(columnTitles);
    ComparisonStructs::DiffInfoForOneScan forSearching;
    for (QSet<ComparisonStructs::MetaInfoForScanInBothFiles>::const_iterator iterator
         = scansPresentInBothFiles.begin();
         iterator != scansPresentInBothFiles.end(); iterator++) {
        QStringList firstStringList;
        QStringList secondStringList;

        forSearching.nativeId = iterator->nativeId;
        QSet<ComparisonStructs::DiffInfoForOneScan>::const_iterator diffInfoForThisScan
            = diffInfo.maxDiffs.find(forSearching);
        QSet<ComparisonStructs::DiffInfoForOneScan>::const_iterator nonZeroDiffInfoForThisScan
            = diffInfo.maxNonZeroDiffs.find(forSearching);

        XYIsDifferent = diffInfo.significantDifferences.contains(*iterator);
        firstStringList << firstFile;
        putMetaInfoIntoQStringList(iterator->firstFileMetaInfo, &firstStringList);
        firstStringList << (XYIsDifferent ? "true" : "false")
                        << QString::number(diffInfoForThisScan->coordinateOfMaxDiff.x())
                        << QString::number(diffInfoForThisScan->coordinateOfMaxDiff.y());
        if (userInput.isCentroided && userInput.compareByIndex) {
            firstStringList << QString::number(diffInfoForThisScan->maxDiff.x())
                            << QString::number(diffInfoForThisScan->maxDiff.y())
                            << QString::number(diffInfoForThisScan->maxDiff.y()
                                               / diffInfoForThisScan->coordinateOfMaxDiff.y());
        } else {
            firstStringList << QString::number(diffInfoForThisScan->maxDiff.y())
                            << QString::number(
                                   diffInfoForThisScan->coordinateOfMaxDiff.y() == 0
                                       ? 1
                                       : diffInfoForThisScan->maxDiff.y()
                                           / diffInfoForThisScan->coordinateOfMaxDiff.y())
                            << 
                                QString("%1%2%3")
                .arg(QString::number(nonZeroDiffInfoForThisScan->coordinateOfMaxDiff.x())).arg("/")
                .arg(QString::number(nonZeroDiffInfoForThisScan->coordinateOfMaxDiff.y()))
                            << QString::number(nonZeroDiffInfoForThisScan->maxDiff.y())
                            << QString::number(nonZeroDiffInfoForThisScan->maxDiff.y()
                                               / diffInfoForThisScan->coordinateOfMaxDiff.y());
        }
        csvWriter.writeRow(firstStringList);
        secondStringList << secondFile;
        putMetaInfoIntoQStringList(iterator->secondFileMetaInfo, &secondStringList);
        firstStringList << (XYIsDifferent ? "true" : "false");
        csvWriter.writeRow(secondStringList);
    }
}