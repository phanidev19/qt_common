/*
* Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/


#include "MSReaderTesting.h"


_PMI_BEGIN

MSReaderTesting::MSReaderTesting() :m_canOpen(false)
{

}

const QString MSReaderTesting::MAGIC_FILENAME = QStringLiteral("MAGIC_STRING_GOES_HERE_pmi_test");

bool MSReaderTesting::canOpen(const QString &filename) const
{
    if (!m_canOpen) {
        return false;
    }

    if (filename.endsWith(MAGIC_FILENAME)) {
        return true;
    }

    return false;
}

Err MSReaderTesting::getScanData(long scanNumber, point2dList *points,
                                 bool do_centroiding /*= false*/,
                                 msreader::PointListAsByteArrays *pointListAsByteArrays /*= NULL*/)
{
    *points = m_scanData;
    return kNoErr;
}

Err MSReaderTesting::getScanInfo(long scanNumber, msreader::ScanInfo *obj) const
{
    obj->retTimeMinutes = 0.02;
    obj->scanLevel = 1;
    return kNoErr;
}

Err MSReaderTesting::getNumberOfSpectra(long *totalNumber, long *startScan, long *endScan) const
{
    *totalNumber = 1;
    *startScan = 0;
    *endScan = 0;

    return kNoErr;
}

_PMI_END