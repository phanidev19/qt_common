/*
* Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/


#include "ScanIndexTimeConverter.h"
#include "TimeWarp.h"
#include "MSReader.h"

_PMI_BEGIN

ScanIndexTimeConverter::ScanIndexTimeConverter()
{

}

ScanIndexTimeConverter::ScanIndexTimeConverter(const std::vector<double> &time, const std::vector<double> &scanIndex) :m_time(time), m_scanIndex(scanIndex)
{

}

double ScanIndexTimeConverter::timeToScanIndex(double time) const
{
    return TimeWarp::mapTime(m_time, m_scanIndex, time);
}

double ScanIndexTimeConverter::scanIndexToTime(double scanIndex) const
{
    return TimeWarp::mapTime(m_scanIndex, m_time, scanIndex);
}

std::vector<double> pmi::ScanIndexTimeConverter::times() const
{
    return m_time;
}

ScanIndexTimeConverter ScanIndexTimeConverter::fromMSReader(MSReader * reader)
{
    ScanIndexTimeConverter converter;
    if (!reader->isOpen()) {
        return converter;
    }

    QList<msreader::ScanInfoWrapper> lockmassList;
    reader->getScanInfoListAtLevel(1, &lockmassList);

    std::vector<double> indices;
    std::vector<double> retTimes;
    for (int i = 0; i < lockmassList.size(); ++i) {
        indices.push_back(i);
        retTimes.push_back(lockmassList.at(i).scanInfo.retTimeMinutes);
    }
    
    return ScanIndexTimeConverter(retTimes, indices);
}

_PMI_END


