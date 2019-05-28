/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "NonUniformTilePartIterator.h"

#include "pmi_common_tiles_debug.h"

_PMI_BEGIN

const int INVALID_TILE_PART_COORDINATE = -1;
const point2d INVALID_NON_UNIFORM_TILE_VALUE = point2d(-1, -1);

NonUniformTilePartIterator::NonUniformTilePartIterator(const point2dList &scanPart, double mzStart,
                                                       double mzEnd)
    : m_tilePart(scanPart)
    , m_tilePartX(INVALID_TILE_PART_COORDINATE)
    , m_tilePartEndX(INVALID_TILE_PART_COORDINATE)
    , m_lastValue(INVALID_NON_UNIFORM_TILE_VALUE)
{
    Q_ASSERT(mzStart <= mzEnd);
    init(mzStart, mzEnd);
}

bool NonUniformTilePartIterator::isNull() const
{
    return (m_tilePartEndX == m_tilePartEndX) && (m_tilePartEndX == INVALID_TILE_PART_COORDINATE);
}

int NonUniformTilePartIterator::numContiguousCols() const
{
    Q_ASSERT(!isNull());
    return m_tilePartEndX - (m_tilePartX + 1);
}

void NonUniformTilePartIterator::init(double mzStart, double mzEnd)
{
    const point2d mzStartPt(mzStart, 0);
    auto lower
        = std::lower_bound(m_tilePart.cbegin(), m_tilePart.cend(), mzStartPt, point2d_less_x);

    const point2d mzEndPt(mzEnd, 0);
    auto upper = std::upper_bound(m_tilePart.cbegin(), m_tilePart.cend(), mzEndPt, point2d_less_x);

    // initialize "before" first element
    m_tilePartX = lower - m_tilePart.begin() - 1;
    m_tilePartEndX = upper - m_tilePart.begin();
}

void NonUniformTilePartIterator::advance()
{
    if (!hasNext()) {
        warningTiles() << "Trying to advance beyond the area";
        return;
    }

    int remaining = m_tilePartEndX - (m_tilePartX + 1);
    if (remaining > 0) {
        m_tilePartX++;
    } else {
        // we should not get here, @see hasNext() called above
        qFatal("Trying to advance beyond the tile area");
        return;
    }

    m_lastValue = m_tilePart.at(m_tilePartX);
}

_PMI_END
