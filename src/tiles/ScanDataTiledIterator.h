/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/


#ifndef SCANDATA_TILED_ITERATOR_H
#define SCANDATA_TILED_ITERATOR_H

#include "pmi_core_defs.h"

#include <QtGlobal>

#include "NonUniformTileRange.h"
#include "common_math_types.h"
#include "pmi_common_ms_export.h"

_PMI_BEGIN

//! \brief Iterates the scan data in the intervals given by tile's mz range from NonUniformTileRange
class PMI_COMMON_MS_EXPORT ScanDataTiledIterator
{
public:
    ScanDataTiledIterator(const point2dList * scanData, const NonUniformTileRange &range, int tileStart, int tileCount);

    bool hasNext() const {
        return m_currentTile < m_tileStart + m_tileCount;
    }

    point2dList next();


private:
    int m_tileStart;
    int m_tileCount;

    int m_currentTile;
    point2dList::const_iterator m_lastEnd;

    NonUniformTileRange m_range;
    const point2dList * m_scanData;

    Q_DISABLE_COPY(ScanDataTiledIterator)
};

_PMI_END

#endif // SCANDATA_TILED_ITERATOR_H