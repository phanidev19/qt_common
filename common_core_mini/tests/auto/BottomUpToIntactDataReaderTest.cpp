/*
 * Copyright (C) 2019 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Andrew Nichols anichols@proteinmetrics.com
 */

#include "BottomUpToIntactDataReader.h"
#include "pmi_core_defs.h"

#include <QtTest>

_PMI_BEGIN

class BottomUpToIntactDataReaderTest : public QObject
{
    Q_OBJECT
public:
    BottomUpToIntactDataReaderTest() {}
    private Q_SLOTS :
        void initializeBottomUpDataForTests();
        void retreiveRowsTest();

private:
    QVector<BottomUpToIntactDataReader::ProteinModPoint> bottomUpData;
    QHash<BottomUpToIntactDataReader::ProteinId, BottomUpToIntactDataReader::ProteinName> proteinIds;
    BottomUpToIntactDataReader dataReader;
};


void BottomUpToIntactDataReaderTest::initializeBottomUpDataForTests() {
    
    bottomUpData.push_back(BottomUpToIntactDataReader::ProteinModPoint(0, "42", "Q3(oxidationSQ/15.9949)", 0.5));
    bottomUpData.push_back(BottomUpToIntactDataReader::ProteinModPoint(1, "666", "Q3(oxidationSQ/15.9949)", 0.5));
    bottomUpData.push_back(BottomUpToIntactDataReader::ProteinModPoint(0, "42", "Q3(oxidationSQ/15.9949)", 0.9));
    bottomUpData.push_back(BottomUpToIntactDataReader::ProteinModPoint(1, "666", "Q3(oxidationSQ/15.9949)", 0.1));

    proteinIds.insert(0, "HC");
    proteinIds.insert(1, "LC");

    dataReader.setModificationsRepository(bottomUpData);
    dataReader.setProteinIds(proteinIds);
}

void BottomUpToIntactDataReaderTest::retreiveRowsTest(){

    Err e = kNoErr;

    QVector<BottomUpToIntactDataReader::ProteinModPoint> query;

    //// Check retrieval of one proteinID.
    e = dataReader.selectRowsFromModificationsRepository(QVector<int>({ 0 }), &query);
    QCOMPARE(0.9, query[1].modIntensityPercentage);

    //// Check retrieval of multiple proteinIDs.
    e = dataReader.selectRowsFromModificationsRepository(QVector<int>({ 0, 1 }), &query);
    QCOMPARE(4, query.size());
    QCOMPARE(0.5, query[0].modIntensityPercentage);
    QCOMPARE(0.5, query[1].modIntensityPercentage);
    QCOMPARE(0.9, query[2].modIntensityPercentage);
    QCOMPARE(0.1, query[3].modIntensityPercentage);

    ////Purposely setting indicies out of range for error checking proteinIDs.  Will produce kError
    e = dataReader.selectRowsFromModificationsRepository(QVector<int>({ 2 }), &query);
    QCOMPARE(kError, e);

}

_PMI_END

    QTEST_MAIN(pmi::BottomUpToIntactDataReaderTest)

#include "BottomUpToIntactDataReaderTest.moc"
