/*
* Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "MSReader.h"
#include "CachedMSReader.h"

_PMI_BEGIN

CachedMSReader::CachedMSReader(MSReader * reader, int cacheSize, bool doCentroiding) 
    : m_reader(reader), 
      m_doCentroiding(doCentroiding)
{
    m_cache.setMaxCost(cacheSize);
}


Err CachedMSReader::getScanData(long scanNumber, point2dList ** points)
{
    Err e = kNoErr;
    if (!m_cache.contains(scanNumber)){
        point2dList * data = new point2dList;
        e = m_reader->getScanData(scanNumber, data, m_doCentroiding);
        m_cache.insert(scanNumber, data);
    }

    *points = m_cache.object(scanNumber);
    return e;
}

_PMI_END

