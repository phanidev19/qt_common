/*
* Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef RANDOM_NON_UNIFORM_TILE_ITERATOR
#define RANDOM_NON_UNIFORM_TILE_ITERATOR

#include "NonUniformTileRange.h"
#include "NonUniformTileManager.h"

#include "NonUniformTilePoint.h"

#include "pmi_common_tiles_debug.h"
#include "pmi_common_tiles_export.h"

// common_stable
#include <common_math_types.h>
#include <pmi_core_defs.h>

#include <QDebug>


_PMI_BEGIN

template <class T>
class RandomNonUniformTileIteratorBase {

public:
    RandomNonUniformTileIteratorBase(NonUniformTileManagerBase<T> *tileManager,
                                 const NonUniformTileRange &range, bool doCentroiding)
        : m_tileManager(tileManager)
        , m_range(range)
    {
        Q_ASSERT(tileManager);
        m_contentType = doCentroiding ? NonUniformTileStoreType::ContentMS1Centroided : NonUniformTileStoreType::ContentMS1Raw;
    }

    void moveTo(int tileX, int tileY, int scanIndex)
    {
        QPoint tilePos(tileX, tileY);
        m_lastValue.tilePos = tilePos;
        m_lastValue.scanIndex = scanIndex;
        m_lastValue.internalIndex = scanIndex - m_range.scanIndexAt(tileY);
        
    }

    void moveTo(int scanIndex, double mz) {
        int tileX = m_range.tileX(mz);
        int tileY = m_range.tileY(scanIndex);
        moveTo(tileX, tileY, scanIndex);
    }

    const T &value() const { 
        // tile is QVector-based, so the copy is "free"
        const NonUniformTileBase<T> tile = m_tileManager->loadTile(m_lastValue.tilePos, m_contentType);
        if (tile.isNull() || tile.isEmpty()) {
            if (tile.isNull()) {
                qWarning() << "Null tile!" << m_lastValue.tilePos << "contentType" << m_contentType;
            }

            return NonUniformTileBase<T>::defaultTileValue();
        } else {
            return tile.value(m_lastValue.internalIndex);
        }
    }

    //! \brief how many scan indexes are stored contiguously in the tile
    int numContiguousRows(int scanIndex) const {
        int offset = m_range.tileOffset(scanIndex);
        return m_range.scanIndexTileLength() - offset;
    }

    void setCacheSize(int cacheSize) { 
        m_tileManager->setCacheSize(cacheSize); 
    }

private:
    NonUniformTileManagerBase<T> *m_tileManager;
    NonUniformTileRange m_range;
    
    NonUniformTilePoint m_lastValue;
    
    NonUniformTileStoreType::ContentType m_contentType;
};

typedef RandomNonUniformTileIteratorBase<point2dList> RandomNonUniformTileIterator;

typedef RandomNonUniformTileIteratorBase<QBitArray> RandomNonUniformTileSelectionIterator;

typedef RandomNonUniformTileIteratorBase<QVector<int>> RandomNonUniformTileHillIndexIterator;

_PMI_END

#endif // RANDOM_NON_UNIFORM_TILE_ITERATOR