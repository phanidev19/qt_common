/*
 * Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 */

#include <MSReader.h>
#include "MS1PrefixSum.h"
#include <QtTest>

_PMI_BEGIN

const int SCAN_NUMBERS_TO_CACHE = 500;

class MS1PrefixSumTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testFindClosestAvailableScanNumber();
    void testFindClosestAvailableScanNumberEvery5th();
    void testFindClosestAvailableScanNumberEvery5thStartAt5();
    void testFindClosestAvailableScanNumberInEmptyCache();

private:
    void insertClosestNumber(const MS1PrefixSum& prefixSum, long scanNumber, QMap<long, bool>* cacheToInsertTo);
};

void MS1PrefixSumTest::insertClosestNumber(const MS1PrefixSum& prefixSum, long scanNumber, QMap<long, bool>* cacheToInsertTo)
{
    const long num = prefixSum.findClosestAvailableScanNumber(scanNumber);
    QVERIFY(num != LONG_MIN);
    cacheToInsertTo->insert(num, 1);
}

void MS1PrefixSumTest::testFindClosestAvailableScanNumber()
{
    MS1PrefixSum prefixSum(MSReader::Instance());
    QMap<long, bool> toCompareAgainstCache;

    // cache all even numbers
    const int gapBetweenNumbers = 2;
    for (int evenNumber = 2; evenNumber <= SCAN_NUMBERS_TO_CACHE; evenNumber += gapBetweenNumbers) {
        prefixSum.m_cachedScanNumbers.insert(evenNumber, 1);
    }
    // check what you get when you search for all odd numbers.
    for (int oddScanNumber = 1; oddScanNumber <= SCAN_NUMBERS_TO_CACHE; oddScanNumber += gapBetweenNumbers) {
        insertClosestNumber(prefixSum, oddScanNumber, &toCompareAgainstCache);
    }

    QCOMPARE(prefixSum.m_cachedScanNumbers, toCompareAgainstCache); // Test same distance passed, all keys / values match

    QCOMPARE(prefixSum.findClosestAvailableScanNumber(INT_MIN),
             (prefixSum.m_cachedScanNumbers.constBegin()).key()); // Find closest to a very large negative number
    QCOMPARE(prefixSum.findClosestAvailableScanNumber(INT_MAX),
             (prefixSum.m_cachedScanNumbers.constEnd()-1).key()); // Find closest to a very large number much greater than greatest cache scan
}

void MS1PrefixSumTest::testFindClosestAvailableScanNumberEvery5th()
{
    // cache every fifth number starting at 0
    MS1PrefixSum prefixSum(MSReader::Instance());
    QMap<long, bool> toCompareAgainstCache;
    const int gapBetweenNumbers = 5;
    for (int fifthNumber = 0; fifthNumber < SCAN_NUMBERS_TO_CACHE; fifthNumber += gapBetweenNumbers) {
        prefixSum.m_cachedScanNumbers.insert(fifthNumber, 1);
    }
    // Check to see if it rounds to the closest correctly if closest is lower
    for (int fifthNumber = 0; fifthNumber < SCAN_NUMBERS_TO_CACHE; fifthNumber += 5) {
        // in C++, integer division rounds down
        const int closerToLesserValueComparisonNumber = fifthNumber + gapBetweenNumbers / 2;
        insertClosestNumber(prefixSum, closerToLesserValueComparisonNumber, &toCompareAgainstCache);
    }
    QCOMPARE(prefixSum.m_cachedScanNumbers, toCompareAgainstCache); // All keys / values match
}

void MS1PrefixSumTest::testFindClosestAvailableScanNumberEvery5thStartAt5()
{
    // cache every fifth number starting at 5
    MS1PrefixSum prefixSum(MSReader::Instance());
    QMap<long, bool> toCompareAgainstCache;
    const int gapBetweenNumbers = 5;
    for (int fifthNumber = 5; fifthNumber < SCAN_NUMBERS_TO_CACHE; fifthNumber += gapBetweenNumbers) {
        prefixSum.m_cachedScanNumbers.insert(fifthNumber, 1);
    }
    // Check to see if it rounds to the closest correctly if closest is lower
    for (int fifthNumber = 0; fifthNumber < SCAN_NUMBERS_TO_CACHE; fifthNumber += 5) {
        // in C++, integer division rounds down. + 1 makes it always round up.
        const int closerToUpperValueComparisonNumber = fifthNumber + gapBetweenNumbers / 2 + 1;
        insertClosestNumber(prefixSum, closerToUpperValueComparisonNumber, &toCompareAgainstCache);
    }
    QCOMPARE(prefixSum.m_cachedScanNumbers, toCompareAgainstCache); // All keys / values match
}

void MS1PrefixSumTest::testFindClosestAvailableScanNumberInEmptyCache()
{
    MS1PrefixSum prefixSum(MSReader::Instance());
    QCOMPARE(prefixSum.findClosestAvailableScanNumber(1), LONG_MIN); // Find in empty cache
}

_PMI_END

QTEST_MAIN(pmi::MS1PrefixSumTest)

#include "MS1PrefixSumTest.moc"
