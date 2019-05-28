/*
* Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef SCANINDEX_NUMBER_CONVERTER_H
#define SCANINDEX_NUMBER_CONVERTER_H

#include "pmi_core_defs.h"
#include <QVector>
#include "pmi_common_ms_export.h"

#include <ScanInfoDao.h> //TODO: extract class!!

_PMI_BEGIN

class MSReader;

class PMI_COMMON_MS_EXPORT ScanIndexNumberConverter {

public:    
    explicit ScanIndexNumberConverter(const std::vector<ScanInfoRow> &scanNumbers);
    ScanIndexNumberConverter(){};
    //TODO more explicit names

    // return -1 if scanIndex is out of the range
    int toScanNumber(int scanIndex) const;
    // return -1 if given scanNumber is not present
    int toScanIndex(int scanNumber) const;

    // return -1.0 if given scanNumber is not present
    double toScanTime(int scanNumber) const;
    
    // returns only if equal time is present
    //TODO investigate and reuse MSReader::ScanInfoList code somewhat
    int timeToScanNumber(double retTime) const;

    // TODO: fix performance
    int timeToScanIndex(double retTime) const;

    // TODO: merge with same strategy as used in
    // MSReaderBase::ScanInfoList::getBestScanNumberFromScanTime Warning: unstable API used in
    // unfinished hill finder algo
    int getBestScanIndexFromTime(double retTime) const;

    // return -1.0 if given scanNumber is not present
    double scanIndexToScanTime(int scanIndex) const;

    // reader has to have raw file opened!
    static ScanIndexNumberConverter fromMSReader(const QList<msreader::ScanInfoWrapper> &scanInfo);
    static ScanIndexNumberConverter fromMSReader(MSReader * reader);

    int size() const { return static_cast<int>(m_scanNumbers.size()); }

private:
    std::vector<ScanInfoRow> m_scanNumbers;
};

_PMI_END

#endif // SCANINDEX_NUMBER_CONVERTER_H
