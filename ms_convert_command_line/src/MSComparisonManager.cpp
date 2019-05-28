/*
 * Copyright (C) 2017-2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author  : Sam Bogar (sbogar@proteinmetrics.com)
 */
#include "MSComparisonManager.h"
#include "ComparisonStructs.h"
#include <qfileinfo.h>

void zeroOutMaxDiff(point2d *maxDiffOne, point2d *maxDiffTwo)
{
    if (maxDiffOne) {
        maxDiffOne->setX(0);
        maxDiffOne->setY(0);
    }
    if (maxDiffTwo) {
        maxDiffTwo->setX(0);
        maxDiffTwo->setY(0);
    }
}

pmi::Err MSComparisonManager::readFromOneMSReader(pmi::MSReader *msReader, bool isFirstFile)
{
    pmi::Err e = pmi::kNoErr;
    long total;
    long start;
    long end;

    msReader->getNumberOfSpectra(&total, &start, &end);
    for (long currentScanNumber = start; currentScanNumber <= end; currentScanNumber++) {
        ComparisonStructs::MetaInfoForScanInOneFile MetaInfoForScanInOneFile;
        e = msReader->getScanInfo(currentScanNumber, &MetaInfoForScanInOneFile.scanInfo);
        ree;
        e = msReader->getScanPrecursorInfo(currentScanNumber,
                                           &MetaInfoForScanInOneFile.precursorInfo);
        ree;
        MetaInfoForScanInOneFile.scanNumber = currentScanNumber;
        isFirstFile ? m_firstFileScans.insert(MetaInfoForScanInOneFile)
                    : m_secondFileScans.insert(MetaInfoForScanInOneFile);
    }
    return pmi::kNoErr;
}

pmi::Err MSComparisonManager::readFromFiles(const QString &file1, const QString &file2)
{
    pmi::Err e = pmi::kNoErr;
    pmi::MSReader *reader = pmi::MSReader::Instance();

    e = reader->openFile(file1);
    ree;
    e = readFromOneMSReader(reader, true);
    ree;
    e = reader->closeFile();
    ree;
    e = reader->openFile(file2);
    ree;
    e = readFromOneMSReader(reader, false);
    ree;
    e = reader->closeFile();
    ree;
    return e;
}

/*!
 * \brief Compares the metaInformation of two files, and returns true if they have no/acceptable
 * levels of difference, and false otherwise
 */
bool MSComparisonManager::compareMetaInfo(const ComparisonStructs::MetaInfoForScanInOneFile &a,
                                          const ComparisonStructs::MetaInfoForScanInOneFile &b,
                                          ComparisonStructs::MetaInfoCompare *result)
{
    DiffTolerances diffTolerances;

    result->retTimeMinutesDiff = a.scanInfo.retTimeMinutes - b.scanInfo.retTimeMinutes;
    result->scanLevelDiff = a.scanInfo.scanLevel - b.scanInfo.scanLevel;
    result->nScanNumberDiff = a.precursorInfo.nScanNumber - b.precursorInfo.nScanNumber;
    result->dIsolationMassDiff = a.precursorInfo.dIsolationMass - b.precursorInfo.dIsolationMass;
    result->dMonoIsoMassDiff = a.precursorInfo.dMonoIsoMass - b.precursorInfo.dMonoIsoMass;
    result->nChargeStateDiff = a.precursorInfo.nChargeState - b.precursorInfo.nChargeState;

    if (result->retTimeMinutesDiff > diffTolerances.retTimeMinutesDiffTolerance
        || result->nScanNumberDiff > diffTolerances.nScanNumberDiffTolerance
        || result->dIsolationMassDiff > diffTolerances.dIsolationDiffTolerance
        || result->dMonoIsoMassDiff > diffTolerances.dMonoIsoMassDiffTolerance
        || result->nChargeStateDiff > diffTolerances.nChargeStateDiffTolerance) {
        return false;
    }

    return true;
}

/*!
 * \brief Populates the given set pointers with the scans shared between the files, the scans only
 * found in the first file, and the scans only found in the second file, respectively.
 */
pmi::Err MSComparisonManager::findSharedScans(
    QSet<ComparisonStructs::MetaInfoForScanInBothFiles> *intersectNativeID,
    QSet<ComparisonStructs::MetaInfoForScanInOneFile> *scansPresentOnlyInFirstFile,
    QSet<ComparisonStructs::MetaInfoForScanInOneFile> *scansPresentOnlyInSecondFile)
{
    QSet<ComparisonStructs::MetaInfoForScanInOneFile> intersectSet(m_firstFileScans);
    QSet<ComparisonStructs::MetaInfoForScanInOneFile> tmpScansPresentOnlyInFirstFile(m_firstFileScans);
    QSet<ComparisonStructs::MetaInfoForScanInOneFile> tmpScansPresentOnlyInSecondFile(m_secondFileScans);

    ComparisonStructs::MetaInfoForScanInOneFile testing;

    tmpScansPresentOnlyInFirstFile.subtract(m_secondFileScans);
    intersectSet.intersect(m_secondFileScans);
    tmpScansPresentOnlyInSecondFile.subtract(m_firstFileScans);

    scansPresentOnlyInFirstFile->swap(tmpScansPresentOnlyInFirstFile);
    scansPresentOnlyInSecondFile->swap(tmpScansPresentOnlyInSecondFile);
    for (QSet<ComparisonStructs::MetaInfoForScanInOneFile>::iterator iterator
         = intersectSet.begin();
         iterator != intersectSet.end(); iterator++) {
        ComparisonStructs::MetaInfoForScanInBothFiles obj;
        obj.nativeId = iterator->scanInfo.nativeId;
        obj.firstFileMetaInfo = *m_firstFileScans.find(*iterator);
        obj.secondFileMetaInfo = *m_secondFileScans.find(*iterator);
        intersectNativeID->insert(obj);
    }
    return pmi::kNoErr;
}

void writeXYPointToFileProfile(pmi::CsvWriter *csvWriter,
                               const ComparisonStructs::UserInput &userInput,
                               const ComparisonStructs::MetaInfoForScanInBothFiles &metaInfo,
                               const pmi::PlotBase &plotBase, const point2d &point,
                               bool isOutOfBounds)
{
    csvWriter->writeRow(QStringList() << userInput.firstFile << metaInfo.nativeId
                                      << QString::number(point.x()) << QString::number(point.y()));
    if (isOutOfBounds) {
        csvWriter->writeRow(QStringList()
                            << userInput.secondFile << metaInfo.nativeId << "out of bounds"
                            << "out of bounds");
        csvWriter->writeRow(QStringList() << "difference:"
                                          << "n/a"
                                          << "n/a"
                                          << "n/a");
    } else {
        csvWriter->writeRow(QStringList() << userInput.secondFile << metaInfo.nativeId
                                          << QString::number(point.x())
                                          << QString::number(plotBase.evaluate(point.x())));
        csvWriter->writeRow(QStringList()
                            << "difference:" << metaInfo.nativeId << "n/a"
                            << QString::number(point.y() - plotBase.evaluate(point.x())));
    }
}

long MSComparisonManager::compareOneScan2dPointDataProfile(
    const pmi::point2dList &firstPoints, const pmi::point2dList &secondPoints,
    const ComparisonStructs::UserInput &userInput,
    const ComparisonStructs::MetaInfoForScanInBothFiles &metaInfo, pmi::CsvWriter *csvWriter,
    ComparisonStructs::DiffInfo *diffInfo)
{
    DiffTolerances diffTolerances;
    static bool isFirstCall = true;
    pmi::PlotBase plotBase(secondPoints);
    double lowerBoundCheckVal;
    double upperBoundCheckVal;
    double tmpEvaluateVal;
    double normalizedDiffForOne;
    long diffCount = 0;
    point2d maxDiff[2];
    point2d maxNonZeroDiff[2];

    zeroOutMaxDiff(&maxDiff[0], &maxDiff[1]);
    if (secondPoints.empty()) {
        return static_cast<long>(firstPoints.size());
    }
    lowerBoundCheckVal = secondPoints[0].x();
    upperBoundCheckVal = secondPoints[secondPoints.size() - 1].x();
    for (pmi::point2dList::const_iterator iter = firstPoints.begin(); iter != firstPoints.end();
         iter++) {
        if (iter->x() < lowerBoundCheckVal || iter->x() > upperBoundCheckVal) {
            diffCount++;
            if (userInput.writeFull) {
                writeXYPointToFileProfile(csvWriter, userInput, metaInfo, plotBase, *iter, true);
            }
        } else {
            tmpEvaluateVal = plotBase.evaluate(iter->x());
            normalizedDiffForOne = (iter->y() - tmpEvaluateVal) / iter->y();
            if (std::abs(iter->y() - tmpEvaluateVal) > std::abs(maxDiff[1].y())) {
                maxDiff[0].setX(iter->x());
                maxDiff[0].setY(iter->y());
                maxDiff[1].setX(nan(""));
                maxDiff[1].setY(iter->y() - tmpEvaluateVal);
            }
            if (iter->y() != 0 && tmpEvaluateVal != 0
                && std::abs(iter->y() - tmpEvaluateVal) > std::abs(maxDiff[1].y())) {
                maxNonZeroDiff[0].setX(iter->x());
                maxNonZeroDiff[0].setY(iter->y());
                maxNonZeroDiff[1].setX(nan(""));
                maxNonZeroDiff[1].setY(iter->y() - tmpEvaluateVal);
            }
            if (std::abs(normalizedDiffForOne) > diffTolerances.normalizedYDiffTolerance) {
                diffCount++;
                if (userInput.writeFull) {
                    writeXYPointToFileProfile(csvWriter, userInput, metaInfo, plotBase, *iter,
                                              false);
                }
            }
        }
    }
    if (isFirstCall) {
        ComparisonStructs::DiffInfoForOneScan diffInfoForThisScan;
        ComparisonStructs::DiffInfoForOneScan nonZeroDiffInfoForThisScan;

        diffInfoForThisScan.nativeId = metaInfo.nativeId;
        diffInfoForThisScan.coordinateOfMaxDiff = maxDiff[0];
        diffInfoForThisScan.maxDiff = maxDiff[1];
        diffInfo->maxDiffs.insert(diffInfoForThisScan);

        nonZeroDiffInfoForThisScan.nativeId = metaInfo.nativeId;
        nonZeroDiffInfoForThisScan.coordinateOfMaxDiff = maxNonZeroDiff[0];
        nonZeroDiffInfoForThisScan.maxDiff = maxNonZeroDiff[1];
        diffInfo->maxNonZeroDiffs.insert(nonZeroDiffInfoForThisScan);
    }
    if (std::abs(maxDiff[1].y()) > std::abs(diffInfo->biggestDiffInFile.y())) {
        diffInfo->biggestDiffInFile.setX(maxDiff[1].x());
        diffInfo->biggestDiffInFile.setY(maxDiff[1].y());
        diffInfo->coordinateOfBiggestDiffInFile.setX(maxDiff[0].x());
        diffInfo->coordinateOfBiggestDiffInFile.setY(maxDiff[0].y());
        diffInfo->scanWithGreatestDiff = metaInfo;
    }
    isFirstCall = (isFirstCall == true) ? false : true;
    return diffCount;
}

/*
** TO-DO:
**  Allow for distinct positive/negative tolerances
**  Implement a system for user-inputted tolerances
*/

void writeXYPointToFileCentroid(const ComparisonStructs::UserInput &userInput,
                                const ComparisonStructs::MetaInfoForScanInBothFiles &metaInfo,
                                const point2d &firstPoint, const point2d &secondPoint,
                                pmi::CsvWriter *csvWriter)
{
    csvWriter->writeRow(QStringList()
                        << userInput.firstFile << metaInfo.nativeId
                        << QString::number(firstPoint.x()) << QString::number(firstPoint.y()));
    csvWriter->writeRow(QStringList()
                        << userInput.secondFile << metaInfo.nativeId
                        << QString::number(secondPoint.x()) << QString::number(secondPoint.y()));
    csvWriter->writeRow(QStringList() << "difference:" << metaInfo.nativeId
                                      << QString::number(firstPoint.x() - secondPoint.x())
                                      << QString::number(firstPoint.y() - secondPoint.y()));
}


void printMissingXYPoints(unsigned long iterator, const pmi::point2dList &pointList, pmi::CsvWriter *csvWriter, ComparisonStructs::UserInput userInput, ComparisonStructs::MetaInfoForScanInBothFiles metaInfo)
{
    while (iterator < pointList.size()) {
        csvWriter->writeRow(QStringList() << userInput.firstFile << metaInfo.nativeId
            << QString::number(pointList[iterator].x())
            << QString::number(pointList[iterator].y()));
        csvWriter->writeRow(QStringList() << userInput.secondFile << metaInfo.nativeId << "NULL"
            << "NULL");
        csvWriter->writeRow(QStringList() << "difference:" << metaInfo.nativeId
            << "N/A"
            << "N/A");
        iterator++;
    }
}

pmi::point2dList MSComparisonManager::compareCentroided2dPointsByIndex(
    const pmi::point2dList &firstPoints, const pmi::point2dList &secondPoints,
    const ComparisonStructs::UserInput &userInput, pmi::CsvWriter *csvWriter,
    const ComparisonStructs::MetaInfoForScanInBothFiles &metaInfo,
    ComparisonStructs::DiffInfo *diffInfo)
{
    static bool isFirstCall = true;
    unsigned long iterator = 0;
    unsigned long diffCount = 0;
    double currentMaxDiff = 0;
    point2d maxDiff[2];
    pmi::point2dList differences;
    ComparisonStructs::DiffInfoForOneScan diffInfoForThisScan;

    zeroOutMaxDiff(&maxDiff[0], &maxDiff[1]);

    while (iterator < firstPoints.size() && iterator < secondPoints.size()) {
        if (firstPoints[iterator].x() != secondPoints[iterator].x()
            || firstPoints[iterator].y() != secondPoints[iterator].y()) {
            if (std::abs(firstPoints[iterator].x() - secondPoints[iterator].x()) > currentMaxDiff) {
                maxDiff[0].setX(firstPoints[iterator].x());
                maxDiff[0].setY(firstPoints[iterator].y());
                maxDiff[1].setX(firstPoints[iterator].x() - secondPoints[iterator].x());
                maxDiff[1].setY(firstPoints[iterator].y() - secondPoints[iterator].y());
                currentMaxDiff = std::abs(firstPoints[iterator].x() - secondPoints[iterator].x());
            }
            if (userInput.writeFull) {
                //these 'writeFull' function calls are the only places where the csvWriter is needed.
                //Is there a better way to get the writer to that point than passing it as argument all the way through the stack?
                    //Like maybe have the writer be a private member variable?
                writeXYPointToFileCentroid(userInput, metaInfo, firstPoints[iterator],
                    secondPoints[iterator], csvWriter);
            }
            differences.push_back(firstPoints[iterator]);
            diffCount++;
        }
        iterator++;
    }
    if (userInput.writeFull) {
        if (iterator < firstPoints.size()) {
            printMissingXYPoints(iterator, firstPoints, csvWriter, userInput, metaInfo);
        }
        else if (iterator < secondPoints.size()) {
            printMissingXYPoints(iterator, secondPoints, csvWriter, userInput, metaInfo);
        }
    }
    diffInfoForThisScan.nativeId = metaInfo.nativeId;
    diffInfoForThisScan.coordinateOfMaxDiff = maxDiff[0];
    diffInfoForThisScan.maxDiff = maxDiff[1];
    diffInfo->maxDiffs.insert(diffInfoForThisScan);

    if (std::abs(maxDiff[1].x()) > std::abs(diffInfo->biggestDiffInFile.x())) {
        diffInfo->biggestDiffInFile.setX(maxDiff[1].x());
        diffInfo->biggestDiffInFile.setY(maxDiff[1].y());
        diffInfo->coordinateOfBiggestDiffInFile.setX(maxDiff[0].x());
        diffInfo->coordinateOfBiggestDiffInFile.setY(maxDiff[0].y());
        diffInfo->scanWithGreatestDiff = metaInfo;
    }
    return differences;
}

pmi::point2dList MSComparisonManager::compareCentroided2dPointsByCoordinate(
    const pmi::point2dList &firstPoints, const pmi::point2dList &secondPoints,
    const ComparisonStructs::UserInput &userInput, pmi::CsvWriter *csvWriter,
    const ComparisonStructs::MetaInfoForScanInBothFiles &metaInfo)
{
    DiffTolerances diffTolerances;
    double normalizedDiffForOne;
    point2d currentFirstPoint;
    point2d currentSecondPoint;
    unsigned long firstPointIterator = 0;
    unsigned long secondPointIterator = 0;
    long diffCount = 0;
    pmi::point2dList differences;

    while (firstPointIterator < firstPoints.size() && secondPointIterator < secondPoints.size()) {
        currentFirstPoint = firstPoints[firstPointIterator];
        currentSecondPoint = secondPoints[secondPointIterator];
        if (currentFirstPoint.x()
                < currentSecondPoint.x() + diffTolerances.xProximityToBeConsideredSamePointProfile
            && currentFirstPoint.x()
                > currentSecondPoint.x() - diffTolerances.xProximityToBeConsideredSamePointProfile) {
            firstPointIterator++;
            secondPointIterator++;
            normalizedDiffForOne
                = (currentFirstPoint.y() - currentSecondPoint.y()) / currentFirstPoint.y();
            if (normalizedDiffForOne > diffTolerances.normalizedYDiffTolerance) {
                if (userInput.writeFull)
                    writeXYPointToFileCentroid(userInput, metaInfo, currentFirstPoint,
                                               currentSecondPoint, csvWriter);
                diffCount++;
            }
        } else {
            diffCount++;
            if (userInput.writeFull)
                writeXYPointToFileCentroid(userInput, metaInfo, currentFirstPoint,
                                           currentSecondPoint, csvWriter);
            if (currentFirstPoint.x()
                > currentSecondPoint.x() + diffTolerances.xProximityToBeConsideredSamePointProfile) {
                differences.push_back(currentSecondPoint);
                secondPointIterator++;
            } else {
                differences.push_back(currentFirstPoint);
                firstPointIterator++;
            }
        }
    }
    return differences;
}

long MSComparisonManager::compareOneScan2dPointDataCentroid(
    const pmi::point2dList &firstPoints, const pmi::point2dList &secondPoints,
    const ComparisonStructs::UserInput &userInput,
    const ComparisonStructs::MetaInfoForScanInBothFiles &metaInfo, pmi::CsvWriter *csvWriter,
    ComparisonStructs::DiffInfo *diffInfo)
{
    pmi::point2dList differences;
    point2d maxDiff[2];

    if (secondPoints.empty()) {
        ComparisonStructs::DiffInfoForOneScan diffInfoForThisScan;

        diffInfoForThisScan.nativeId = metaInfo.nativeId;
        zeroOutMaxDiff(&diffInfoForThisScan.coordinateOfMaxDiff, &diffInfoForThisScan.maxDiff);
        diffInfo->maxDiffs.insert(diffInfoForThisScan);
        return static_cast<long>(firstPoints.size());
    } else if (userInput.compareByIndex) {
        differences = compareCentroided2dPointsByIndex(firstPoints, secondPoints, userInput,
                                                       csvWriter, metaInfo, diffInfo);
    } else {
        differences = compareCentroided2dPointsByCoordinate(firstPoints, secondPoints, userInput,
                                                            csvWriter, metaInfo);
    }

    return static_cast<long>(differences.size());
}

ComparisonStructs::DiffInfo MSComparisonManager::compare2dPointData(
	const QSet<ComparisonStructs::MetaInfoForScanInBothFiles> &overlap,
	const ComparisonStructs::UserInput &userInput)
{
	point2d maxDiff;

	long diffCount = 0;
	pmi::point2dList firstPoints;
	pmi::point2dList secondPoints;
	QString firstFile = QFileInfo(userInput.firstFile).baseName();
	QString secondFile = QFileInfo(userInput.secondFile).baseName();
    const QString filePath = QString("%1%2%3%4%5").arg(userInput.outputPath).arg("\\FullXYComparisonOf")
                            .arg(firstFile).arg("And").arg(secondFile).arg(".csv");
    pmi::CsvWriter csvWriter(filePath, "\n", ',');
	ComparisonStructs::DiffInfo diffInfo;
	pmi::point2dList maxDiffs;
	QSet<ComparisonStructs::MetaInfoForScanInBothFiles> significantDifferences;
	pmi::MSReader *reader = pmi::MSReader::Instance();

	zeroOutMaxDiff(&diffInfo.biggestDiffInFile, &diffInfo.coordinateOfBiggestDiffInFile);


    for (QSet<ComparisonStructs::MetaInfoForScanInBothFiles>::const_iterator iter = overlap.begin(); iter != overlap.end(); iter++) {
        pmi::point2dList firstPoints;
        pmi::point2dList secondPoints;

        reader->openFile(userInput.firstFile);
        reader->getScanData(iter->firstFileMetaInfo.scanNumber, &firstPoints);
        reader->openFile(userInput.secondFile);
        reader->getScanData(iter->secondFileMetaInfo.scanNumber, &secondPoints);
        if (userInput.isCentroided) {
            compareOneScan2dPointDataCentroid(firstPoints, secondPoints, userInput, *iter, &csvWriter, &diffInfo);
        }
        else {
            compareOneScan2dPointDataProfile(firstPoints, secondPoints, userInput, *iter, &csvWriter, &diffInfo);
        }
    }
	return diffInfo;
}