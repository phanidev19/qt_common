/*
* Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

// common_ms
#include "MSReaderInfo.h"
#include "MSReader.h"
#include "pmi_common_ms_debug.h"

// core_mini
#include <CsvWriter.h>

#include <algorithm>

using namespace pmi;
using namespace pmi::msreader;

//Note: INT_MAX, INT_MIN is not low/high enough.
// If needed use http://en.cppreference.com/w/c/types/limits
// Better, don't use these limits.  See MSReaderInfo::mzMinMax as example.
// See https://proteinmetrics.atlassian.net/browse/ML-2096
//const QPointF INVALID_MIN(INT_MAX, INT_MAX);
//const QPointF INVALID_MAX(INT_MIN, INT_MIN);

MSReaderInfo::MSReaderInfo(MSReader *reader)
    : m_reader(reader)
    , m_dataReady(false)
    , m_doCentroiding(false)
{
}

MSReaderInfo::~MSReaderInfo()
{

}

std::pair<double, double > MSReaderInfo::mzMinMax(bool centroided, bool * ok) const
{
    QList<ScanInfoWrapper> lockmassList;
    m_reader->getScanInfoListAtLevel(1, &lockmassList);

    if (ok) {
        *ok = false;
    }
    bool firstTime = true;
    double bestMin = 0;
    double bestMax = 0;

    // iterate all scan numbers
    for (int i = 0; i < lockmassList.size(); ++i){
        int scanNumber = lockmassList.at(i).scanNumber;
        
        point2dList scanData;
        m_reader->getScanData(scanNumber, &scanData, centroided);

        //It's OK for scan to be empty. This can happen
        if (scanData.empty()){
            continue;
        }

        if (ok) {
            *ok = true;
        }

        double currMin = scanData.front().x();
        double currMax = scanData.back().x();
        if (currMin > currMax) {  //just in case scanData is not sorted.
            std::swap(currMin,currMax);
        }
        if (firstTime) {
            bestMin = currMin;
            bestMax = currMax;
            firstTime = false;
        } else {
            if (bestMin > currMin ) {
                bestMin = currMin;
            }
            if (bestMax < currMax) {
                bestMax = currMax;
            }
        }
    }

    return std::pair<double, double>(bestMin, bestMax);
}

bool compareScanNumbers(const ScanInfoWrapper &left, const ScanInfoWrapper &right){
    return left.scanNumber < right.scanNumber;
}



std::pair<int, int> MSReaderInfo::scanNumberMinMax() const
{
    QList<ScanInfoWrapper> lockmassList;
    m_reader->getScanInfoListAtLevel(1, &lockmassList);

    auto minMaxScanNum = std::minmax_element(lockmassList.begin(), lockmassList.end(), compareScanNumbers);
    return std::pair<int,int>((*minMaxScanNum.first).scanNumber, (*minMaxScanNum.second).scanNumber);
}

int MSReaderInfo::scanNumberCount() const
{
    QList<ScanInfoWrapper> lockmassList;
    m_reader->getScanInfoListAtLevel(1, &lockmassList);

    return lockmassList.size();
}

int MSReaderInfo::maxMzItemsPerScan() const
{
    QList<ScanInfoWrapper> lockmassList;
    m_reader->getScanInfoListAtLevel(1, &lockmassList);

    size_t maxSize = 0;
    
    for (int i = 0; i < lockmassList.size(); ++i) {
        int scanNumber = lockmassList.at(i).scanNumber;
        point2dList scanData;
        m_reader->getScanData(scanNumber, &scanData);
        
        if (scanData.size() > maxSize) {
            maxSize = scanData.size();
        }
    }

    return static_cast<int>(maxSize);

}

void MSReaderInfo::computeMSDataStatistics(const QList<msreader::ScanInfoWrapper> &scanSelection /*= QList<MSReaderBase::ScanInfoWrapper>()*/)
{
    m_scanSelection = scanSelection;
    // if client does not provide subset of selection, use all
    if (m_scanSelection.isEmpty()) {
        selectAllScans();
    }
    fetchStatData();
}

QList<msreader::ScanInfoWrapper> MSReaderInfo::randomScanSelection(int count)
{
    QList<msreader::ScanInfoWrapper> result;

    // generate random selection of scans
    QList<msreader::ScanInfoWrapper> lockmassList;
    m_reader->getScanInfoListAtLevel(1, &lockmassList);

    result.reserve(count);
    for (int i = 0; i < count; i++) {
        qreal normalized = qrand() / double(RAND_MAX);
        int scanIndex = qRound(normalized * (lockmassList.size() - 1));
        result.push_back(lockmassList.at(scanIndex));
    }

    return result;
}

bool compareIntensities(const QPointF& left, const QPointF& right){
    return left.y() < right.y();
}

VectorStats<double> MSReaderInfo::fetchMzMinStats()
{
    return fetchStats(m_mzMinData);
}

VectorStats<double> MSReaderInfo::fetchMzMaxStats()
{
    return fetchStats(m_mzMaxData);
}

VectorStats<double> MSReaderInfo::fetchMzStepStats()
{
    return fetchStats(m_mzStep);
}

VectorStats<double> MSReaderInfo::fetchMinIntensityStats()
{
    return fetchStats(m_intensityMin);
}

VectorStats<double> MSReaderInfo::fetchMaxIntensityStats()
{
    return fetchStats(m_intensityMax);
}

VectorStats<double> MSReaderInfo::fetchStats(const std::vector<double> &data)
{
    if (!m_dataReady) {
        computeMSDataStatistics();
    }
    return VectorStats<double>(data);
}

void MSReaderInfo::fetchStatData()
{
    // fetch minimums
    m_mzMinData.clear();
    m_mzMaxData.clear();
    m_intensityMin.clear();
    m_intensityMax.clear();
    m_mzStep.clear();

    for (int i = 0; i < m_scanSelection.size(); ++i){

        int scanNumber = m_scanSelection.at(i).scanNumber;

        point2dList scanData;
        m_reader->getScanData(scanNumber, &scanData, m_doCentroiding);

        if (scanData.size() > 0) {
            qreal mzMin; 
            qreal mzMax; 
            qreal mzStep; 
            mzStats(scanData, &mzMin, &mzMax, &mzStep);
            m_mzMinData.push_back(mzMin);
            m_mzMaxData.push_back(mzMax);
            m_mzStep.push_back(mzStep);

            qreal minIntensity;
            qreal maxIntensity;
            intensityStats(scanData, &minIntensity, &maxIntensity);
            m_intensityMin.push_back(minIntensity);
            m_intensityMax.push_back(maxIntensity);

        } else {
            warningMs() << "null scan data for scan number" << scanNumber << "at index" << i;
        }
    }

    m_dataReady = true;
}

void MSReaderInfo::selectAllScans()
{
    m_scanSelection.clear();
    Err e = m_reader->getScanInfoListAtLevel(1, &m_scanSelection); eee_absorb;
}

void MSReaderInfo::mzStats(const point2dList &scanData, qreal * mzMin, qreal *mzMax, qreal *mzStep)
{
    *mzMin = scanData.front().x(); // We expect the min value to be the first one
    *mzMax = scanData.back().x();
    *mzStep = (*mzMax - *mzMin) / (scanData.size() - 1);
}

void MSReaderInfo::intensityStats(const pmi::point2dList &scanData, qreal *minIntensity, qreal *maxIntensity)
{
    auto minMaxPoint = std::minmax_element(scanData.begin(), scanData.end(), compareIntensities);
    *minIntensity = (*minMaxPoint.first).y();
    *maxIntensity = (*minMaxPoint.second).y();
}

quint32 MSReaderInfo::pointCount(int msLevel, bool doCentroiding)
{
    selectAllScans();

    quint32 result = 0;
    for (const msreader::ScanInfoWrapper &scanInfo : m_scanSelection) {
        point2dList data;
        Err e = m_reader->getScanData(scanInfo.scanNumber, &data, doCentroiding); eee_absorb;
        result += static_cast<quint32>(data.size());
    }

    return result;
}

bool MSReaderInfo::dumpScansToCsv(const QString &filePath)
{
    selectAllScans();

    CsvWriter scanDataWriter(filePath);
    if (!scanDataWriter.open()) {
        return false;
    }

    QStringList line = { QString(), QString(), QString() };
    for (int i = 0; i < m_scanSelection.size(); ++i) {
        point2dList data;
        Err e = m_reader->getScanData(m_scanSelection.at(i).scanNumber, &data, m_doCentroiding);
        if (e != kNoErr) {
            warningMs() << "Failed to fetch scan" << m_scanSelection.at(i).scanNumber
                        << "with centroiding enabled:" << m_doCentroiding;
            return false;
        }

        line[0] = QString::number(i); // scanNumber
        for (const point2d &pt : data) {
            double mz = pt.x();
            double intensity = pt.y();
            line[1] = QString::number(mz, 'f', 15);
            line[2] = QString::number(intensity, 'f', 15);
            bool ok = scanDataWriter.writeRow(line);
            
            if (!ok) {
                warningMs() << "Failed to write out CSV row!";
                return false;
            }
        
        }
    }

    return true;
}
