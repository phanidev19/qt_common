/*
* Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/


#include "ScanIndexNumberConverter.h"
#include <algorithm>
#include "MSReader.h"
#include "pmi_common_ms_debug.h"

_PMI_BEGIN

bool compareTime(const ScanInfoRow &left, const ScanInfoRow &right) {
    return left.retTimeMinutes < right.retTimeMinutes;
}

bool compareScanNumber(const ScanInfoRow &left, const ScanInfoRow &right) {
    return left.scanNumber < right.scanNumber;
}

ScanIndexNumberConverter::ScanIndexNumberConverter(const std::vector<ScanInfoRow> &scanNumbers) :m_scanNumbers(scanNumbers)
{
}

int ScanIndexNumberConverter::toScanNumber(int scanIndex) const
{
    if (scanIndex >= 0 && size_t(scanIndex) < m_scanNumbers.size()) {
        return m_scanNumbers[scanIndex].scanNumber;
    }

    return -1;
}

int ScanIndexNumberConverter::timeToScanNumber(double retTime) const
{
    ScanInfoRow row;
    row.retTimeMinutes = retTime;
    auto lower = std::lower_bound(m_scanNumbers.begin(), m_scanNumbers.end(), row, compareTime);
    if (lower->retTimeMinutes == retTime){
        return lower->scanNumber;
    }

    return -1;
}

int ScanIndexNumberConverter::timeToScanIndex(double retTime) const
{
    int scanNumber = timeToScanNumber(retTime);
    return toScanIndex(scanNumber);
}

int ScanIndexNumberConverter::getBestScanIndexFromTime(double retTime) const
{
    ScanInfoRow row;
    row.retTimeMinutes = retTime;
    auto lower = std::lower_bound(m_scanNumbers.begin(), m_scanNumbers.end(), row, compareTime);
    if (lower == m_scanNumbers.end()) {
        return m_scanNumbers.back().scanIndex;
    }

    if (lower == m_scanNumbers.begin()) {
        return m_scanNumbers.front().scanIndex;
    }

    // if we have scan time 0.5, 0.10 and request is for 0.8, we want to return 0.5 for now
    return std::prev(lower)->scanIndex;
}

double ScanIndexNumberConverter::scanIndexToScanTime(int scanIndex) const
{
    if (scanIndex >= 0 && size_t(scanIndex) < m_scanNumbers.size()) {
        return m_scanNumbers[scanIndex].retTimeMinutes;
    }

    return -1.0;
}

int ScanIndexNumberConverter::toScanIndex(int scanNumber) const
{
    ScanInfoRow row;
    row.scanNumber = scanNumber;

    auto lower = std::lower_bound(m_scanNumbers.begin(), m_scanNumbers.end(), row, compareScanNumber);
    
    if (lower->scanNumber == scanNumber) {
        return lower->scanIndex;
    }

    return -1;
}

double ScanIndexNumberConverter::toScanTime(int scanNumber) const
{
    int scanIndex = toScanIndex(scanNumber);
    if (scanIndex != -1) {
        return m_scanNumbers[scanIndex].retTimeMinutes;
    }

    return -1.0;
}

ScanIndexNumberConverter ScanIndexNumberConverter::fromMSReader(const QList<msreader::ScanInfoWrapper> &lockmassList)
{
    std::vector<ScanInfoRow> scanNumbers;
    for (int i = 0; i < lockmassList.size(); ++i) {
        ScanInfoRow row;
        row.scanIndex = i;
        row.scanNumber = lockmassList.at(i).scanNumber;
        row.retTimeMinutes = lockmassList.at(i).scanInfo.retTimeMinutes;
        scanNumbers.push_back(row);
    }

    return ScanIndexNumberConverter(scanNumbers);
}


ScanIndexNumberConverter ScanIndexNumberConverter::fromMSReader(MSReader * reader)
{
    if (!reader->isOpen()) {
        warningMs() << "MSReader is not opened! Scan info cannot be accessed. Open MS file first!";
        return ScanIndexNumberConverter();
    }

    QList<msreader::ScanInfoWrapper> lockmassList;
    reader->getScanInfoListAtLevel(1, &lockmassList);
    return ScanIndexNumberConverter::fromMSReader(lockmassList);
}


_PMI_END


