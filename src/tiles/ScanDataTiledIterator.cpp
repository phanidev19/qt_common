/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/


#include "ScanDataTiledIterator.h"


_PMI_BEGIN

ScanDataTiledIterator::ScanDataTiledIterator(const point2dList * scanData, const NonUniformTileRange &range, int tileStart, int tileCount) :
    m_tileStart(tileStart),
    m_tileCount(tileCount),
    m_currentTile(tileStart),

    m_lastEnd(scanData->cbegin()),
    m_range(range),
    m_scanData(scanData)
{
}


point2dList ScanDataTiledIterator::next()
{
    double mzStart = m_range.mzAt(m_currentTile);
    double mzEnd = m_range.mzAt(m_currentTile + 1);

    point2dList filteredMz;

    point2d mzStartPt(mzStart, 0);
    auto lower = std::lower_bound(m_lastEnd, m_scanData->cend(), mzStartPt, point2d_less_x);

    point2d mzEndPt(mzEnd, 0);
    auto upper = std::lower_bound(lower, m_scanData->cend(), mzEndPt, point2d_less_x);

    filteredMz.insert(filteredMz.end(), lower, upper);

    m_lastEnd = upper;
    m_currentTile++;

    return filteredMz;
}


_PMI_END