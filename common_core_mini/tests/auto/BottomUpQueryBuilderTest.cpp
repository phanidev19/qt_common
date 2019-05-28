    /*
 * Copyright (C) 2019 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Andrew Nichols anichols@proteinmetrics.com
 */

#include "BottomUpQueryBuilder.h"
#include "BottomUpConvolution.h"
#include "BottomUpToIntactDataReader.h"
#include "pmi_core_defs.h"

#include <QtTest>

_PMI_BEGIN

class BottomUpQueryBuilderTest : public QObject
{
    Q_OBJECT
public:
    BottomUpQueryBuilderTest() {}

private Q_SLOTS :
    void initializationOfNullReaderGeneratesError();
    void setQueryAndRunSingleChain();
    void setQueryAndRunLCHC();
    void setQueryAndRunLCHCLCHC();
    void setQueryAndRunLCHCLC();
    void testSetQueryOutOfRangeError();
};

void BottomUpQueryBuilderTest::initializationOfNullReaderGeneratesError()
{
    Err e = kNoErr;
        
    BottomUpToIntactDataReader nullReader;
    
    //// Testing error checking for init method. Will generate errors.
    BottomUpQueryBuilder queryBuilder;
    e = queryBuilder.init(nullReader);
    QCOMPARE(kError, e);
}

void BottomUpQueryBuilderTest::setQueryAndRunSingleChain()
{
    Err e = kNoErr;
    QVector<BottomUpToIntactDataReader::ProteinModPoint> bottomUpData;
    bottomUpData.push_back(BottomUpToIntactDataReader::ProteinModPoint(0, "42", "Q3(oxidationSQ/15.9949)", 0.9));
    bottomUpData.push_back(BottomUpToIntactDataReader::ProteinModPoint(1, "666", "Q3(oxidationSQ/15.9949)", 0.1));

    QHash<BottomUpToIntactDataReader::ProteinId, BottomUpToIntactDataReader::ProteinName> proteinIds;
    proteinIds.insert(0, "HC");
    proteinIds.insert(1, "LC");

    BottomUpToIntactDataReader dataReader;
    dataReader.setModificationsRepository(bottomUpData);
    dataReader.setProteinIds(proteinIds);

    BottomUpQueryBuilder queryBuilder;
    e = queryBuilder.init(dataReader);

    QHash<int, int> proteinIDCount;
    proteinIDCount.insert(0, 1);
    BottomUpQueryBuilder::ProteinQuery query(proteinIDCount);

    //// Set the query and run.
    e = queryBuilder.setProteinQuery(query);
    e = queryBuilder.runConvolutionQuery(); 

    //// Test reconstruction of a single chain.
    const QVector<BottomUpConvolution::DeltaMassStick> & deltaMassSticks = queryBuilder.getDeltaMassSticks();

    point2dList pointsToPlot;
    BottomUpConvolution::DeltaMassStick deltaMassStick0;
    BottomUpConvolution::DeltaMassStick deltaMassStick16;
    for (const BottomUpConvolution::DeltaMassStick & deltaMassStick : deltaMassSticks) {
        pointsToPlot.push_back(point2d(deltaMassStick.deltaMass, deltaMassStick.intensityFraction));
        switch (static_cast<int>(round(deltaMassStick.deltaMass))) {
            case 0:
                deltaMassStick0= deltaMassStick;
                break;
            case 16: ////The value of a single oxidation
                deltaMassStick16 = deltaMassStick;
                break;
            default:
                break;
            }
    }

    point2d point0;
    point2d point16;

    for (const point2d & point : pointsToPlot) {
        switch (static_cast<int>(round(point.x()))) {
            case 0:
                point0 = point;
                break;
            case 16: ////The value of a single oxidation
                point16 = point;
                break;
            default:
                break;
        }
    }
    
    qDebug() << pointsToPlot;

    QCOMPARE(0, qRound(point0.x()));
    QCOMPARE(0.1, point0.y());
    QCOMPARE(16, qRound(point16.x()));
    QCOMPARE(0.9, point16.y());
    QCOMPARE(kNoErr, e);
}

void BottomUpQueryBuilderTest::setQueryAndRunLCHC()
{
    Err e = kNoErr;
    QVector<BottomUpToIntactDataReader::ProteinModPoint> bottomUpData;
    bottomUpData.push_back(BottomUpToIntactDataReader::ProteinModPoint(0, "42", "Q3(oxidationSQ/15.9949)", 0.5));
    bottomUpData.push_back(BottomUpToIntactDataReader::ProteinModPoint(1, "666", "Q3(oxidationSQ/15.9949)", 0.5));

    QHash<BottomUpToIntactDataReader::ProteinId, BottomUpToIntactDataReader::ProteinName> proteinIds;
    proteinIds.insert(0, "HC");
    proteinIds.insert(1, "LC");

    BottomUpToIntactDataReader dataReader;
    dataReader.setModificationsRepository(bottomUpData);
    dataReader.setProteinIds(proteinIds);

    BottomUpQueryBuilder queryBuilder;
    e = queryBuilder.init(dataReader);

    QHash<int, int> proteinIDCount;
    proteinIDCount.insert(0, 1);
    proteinIDCount.insert(1, 1);
    BottomUpQueryBuilder::ProteinQuery query(proteinIDCount);

    //// Set the query and run.
    e = queryBuilder.setProteinQuery(query);
    e = queryBuilder.runConvolutionQuery();
    
    const QVector<BottomUpConvolution::DeltaMassStick> & deltaMassSticks = queryBuilder.getDeltaMassSticks();

    point2dList pointsToPlot;
    for (const BottomUpConvolution::DeltaMassStick & deltaMassStick : deltaMassSticks) {
        pointsToPlot.push_back(point2d(deltaMassStick.deltaMass, deltaMassStick.intensityFraction));
    }

    point2d point0;
    point2d point16;
    point2d point32;
    for (const point2d & point : pointsToPlot) {
        switch (static_cast<int>(round(point.x()))) {
            case 0:
                point0 = point;
                break;
            case 16: ////The value of a single oxidation
                point16 = point;
                break;
            case 32: ////The value of a double oxidation
                point32 = point;
                break;
            default:
                break;
        }
    }


    QCOMPARE(25,  static_cast<int>(qRound(point0.y() * 100)));
    QCOMPARE(50, static_cast<int>(qRound(point16.y() * 100)));
    QCOMPARE(25, static_cast<int>(qRound(point32.y() * 100)));
    QCOMPARE(0, static_cast<int>(qRound(point0.x())));
    QCOMPARE(16, static_cast<int>(qRound(point16.x())));
    QCOMPARE(32, static_cast<int>(qRound(point32.x())));
    QCOMPARE(kNoErr, e);
}

void BottomUpQueryBuilderTest::setQueryAndRunLCHCLCHC()
{
    Err e = kNoErr;

    QVector<BottomUpToIntactDataReader::ProteinModPoint> bottomUpData;
    bottomUpData.push_back(BottomUpToIntactDataReader::ProteinModPoint(0, "42", "Q3(oxidationSQ/15.9949)", 0.5));
    bottomUpData.push_back(BottomUpToIntactDataReader::ProteinModPoint(1, "666", "Q3(oxidationSQ/15.9949)", 0.5));

    QHash<BottomUpToIntactDataReader::ProteinId, BottomUpToIntactDataReader::ProteinName> proteinIds;
    proteinIds.insert(0, "HC");
    proteinIds.insert(1, "LC");

    BottomUpToIntactDataReader dataReader;
    dataReader.setModificationsRepository(bottomUpData);
    dataReader.setProteinIds(proteinIds);

    BottomUpQueryBuilder queryBuilder;
    e = queryBuilder.init(dataReader);

    QHash<int, int> proteinIDCount;
    proteinIDCount.insert(0, 2);
    proteinIDCount.insert(1, 2);
    BottomUpQueryBuilder::ProteinQuery query(proteinIDCount);

    //// Set the query and run.
    e = queryBuilder.setProteinQuery(query);
    e = queryBuilder.runConvolutionQuery();

    //// Test reconstruction of a single chain.
    const QVector<BottomUpConvolution::DeltaMassStick> & deltaMassSticks = queryBuilder.getDeltaMassSticks();

    point2dList pointsToPlot;
    for (const BottomUpConvolution::DeltaMassStick & deltaMassStick : deltaMassSticks) {
        pointsToPlot.push_back(point2d(deltaMassStick.deltaMass, deltaMassStick.intensityFraction));
    }

    point2d point0;
    point2d point16;
    point2d point32;
    point2d point48;
    point2d point64;
    for (const point2d & point : pointsToPlot) {
        switch (static_cast<int>(round(point.x()))) {
        case 0:
            point0 = point;
            break;
        case 16: ////The value of a single oxidation
            point16 = point;
            break;
        case 32: ////The value of a double oxidation
            point32 = point;
            break;
        case 48: ////The value of a triple oxidation
            point48 = point;
            break;
        case 64: ////The value of a quadruple oxidation
            point64 = point;
            break;
        default:
            break;
        }
    }

    QCOMPARE(6, static_cast<int>(qRound(point0.y() * 100)));
    QCOMPARE(25, static_cast<int>(qRound(point16.y() * 100)));
    QCOMPARE(37, static_cast<int>((point32.y() * 100))); //// Purposely not rounded
    QCOMPARE(25, static_cast<int>(qRound(point48.y() * 100)));
    QCOMPARE(6, static_cast<int>(qRound(point64.y() * 100)));

    QCOMPARE(0, static_cast<int>(qRound(point0.x())));
    QCOMPARE(16, static_cast<int>(qRound(point16.x() )));
    QCOMPARE(32, static_cast<int>(qRound(point32.x())));
    QCOMPARE(48, static_cast<int>(qRound(point48.x())));
    QCOMPARE(64, static_cast<int>(qRound(point64.x())));
    QCOMPARE(kNoErr, e);
}

void BottomUpQueryBuilderTest::setQueryAndRunLCHCLC()
{
    Err e = kNoErr;
    QVector<BottomUpToIntactDataReader::ProteinModPoint> bottomUpData;
    bottomUpData.push_back(BottomUpToIntactDataReader::ProteinModPoint(0, "42", "Q3(oxidationSQ/15.9949)", 0.5));
    bottomUpData.push_back(BottomUpToIntactDataReader::ProteinModPoint(1, "666", "Q3(oxidationSQ/15.9949)", 0.5));

    QHash<BottomUpToIntactDataReader::ProteinId, BottomUpToIntactDataReader::ProteinName> proteinIds;
    proteinIds.insert(0, "HC");
    proteinIds.insert(1, "LC");

    BottomUpToIntactDataReader dataReader;
    dataReader.setModificationsRepository(bottomUpData);
    dataReader.setProteinIds(proteinIds);

    BottomUpQueryBuilder queryBuilder;
    e = queryBuilder.init(dataReader);

    QHash<int, int> proteinIDCount;
    proteinIDCount.insert(0, 1);
    proteinIDCount.insert(1, 2);
    BottomUpQueryBuilder::ProteinQuery query(proteinIDCount);

    //// Set the query and run.
    e = queryBuilder.setProteinQuery(query);
    e = queryBuilder.runConvolutionQuery();

    //// Test reconstruction of a single chain.
    const QVector<BottomUpConvolution::DeltaMassStick> & deltaMassSticks = queryBuilder.getDeltaMassSticks();
    point2dList pointsToPlot;
    for (const BottomUpConvolution::DeltaMassStick & deltaMassStick : deltaMassSticks) {
        pointsToPlot.push_back(point2d(deltaMassStick.deltaMass, deltaMassStick.intensityFraction));
    }

    point2d point0;
    point2d point16;
    point2d point32;
    point2d point48;
    for (const point2d & point : pointsToPlot) {
        switch (static_cast<int>(round(point.x()))) {
        case 0: ////THe value of a single oxidation
            point0 = point;
            break;
        case 16: ////THe value of a double oxidation
            point16 = point;
            break;
        case 32: ////THe value of a triple oxidation
            point32 = point;
            break;
        case 48: ////THe value of a quadruple oxidation
            point48 = point;
            break;
        default:
            break;
        }
    }

    QCOMPARE(12, static_cast<int>((point0.y() * 100)));
    QCOMPARE(37, static_cast<int>((point16.y() * 100)));
    QCOMPARE(37, static_cast<int>((point32.y() * 100)));
    QCOMPARE(12, static_cast<int>((point48.y() * 100)));

    QCOMPARE(0, static_cast<int>(qRound(point0.x())));
    QCOMPARE(16, static_cast<int>(qRound(point16.x())));
    QCOMPARE(32, static_cast<int>(qRound(point32.x())));
    QCOMPARE(48, static_cast<int>(qRound(point48.x())));
    QCOMPARE(kNoErr, e);
}

void BottomUpQueryBuilderTest::testSetQueryOutOfRangeError()
{
    Err e = kNoErr;

    QVector<BottomUpToIntactDataReader::ProteinModPoint> bottomUpData;
    bottomUpData.push_back(BottomUpToIntactDataReader::ProteinModPoint(0, "42", "Q3(oxidationSQ/15.9949)", 0.5));
    bottomUpData.push_back(BottomUpToIntactDataReader::ProteinModPoint(1, "666", "Q3(oxidationSQ/15.9949)", 0.5));

    QHash<BottomUpToIntactDataReader::ProteinId, BottomUpToIntactDataReader::ProteinName> proteinIds;
    proteinIds.insert(0, "HC");
    proteinIds.insert(1, "LC");

    BottomUpToIntactDataReader dataReader;
    dataReader.setModificationsRepository(bottomUpData);
    dataReader.setProteinIds(proteinIds);

    //// Everything in here should throw errors.

    BottomUpQueryBuilder queryBuilder;
    e = queryBuilder.init(dataReader);

    //// Test for proteinID out of range.
    QHash<int, int> proteinIDCount;
    proteinIDCount.insert(2, 1);
    BottomUpQueryBuilder::ProteinQuery query(proteinIDCount);
    e = queryBuilder.setProteinQuery(query);
    QCOMPARE(kError, e);
}

_PMI_END

QTEST_MAIN(pmi::BottomUpQueryBuilderTest)

#include "BottomUpQueryBuilderTest.moc"
