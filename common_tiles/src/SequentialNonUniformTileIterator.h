/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef SEQUENTIAL_NON_UNIFORM_TILE_ITERATOR_H
#define SEQUENTIAL_NON_UNIFORM_TILE_ITERATOR_H

#include "NonUniformTileRange.h"
#include "NonUniformTileManager.h"
#include "RandomNonUniformTileIterator.h"
#include "ScanIndexInterval.h"
#include "pmi_common_tiles_export.h"

#include <common_math_types.h>
#include <pmi_core_defs.h>

#include <QRectF>

_PMI_BEGIN

class NonUniformTileRange;

/**
 * @brief SequentialNonUniformTileIterator provides efficient way to iterate over Non-uniform tiles.
 *
 * It abstracts away all the loops that you need, all you need to specify is the tile area you want to
 * iterate and this iterator will visit every tile element in the most efficient way. x(), y() and
 * scanIndex() provides you the current information about where the iterator is pointing currently. 
 * mzStart() and mzEnd() provides current information about mz range of tile iterator points to.
 */
class PMI_COMMON_TILES_EXPORT SequentialNonUniformTileIterator
{

public:
    /*
     * @param area - mz, scanIndex domain rectangle that you want to iterate
     *
     * @note area must be within @param range
     */
    SequentialNonUniformTileIterator(NonUniformTileManager *tileManager, const NonUniformTileRange &range,
                                     bool doCentroiding, const QRect &tileArea);

    ~SequentialNonUniformTileIterator() {}

    //! \brief call after init() if you want to restrict the interval
    // TODO: think about better API, e.g. factory method to properly initialize 
    // or optional parameter for constructor 
    void restrictScanIndexInterval(const ScanIndexInterval &interval);

    //! \brief Coordinate of the tile X (0-based row index)
    int x() const { return m_tileX; }
    
    //! \brief Coordinate of the tile Y (0-based column index )
    int y() const { return m_tileY; }

    //! \brief Returns already processed scan index returned in value()
    int scanIndex() const { return m_scanIndex; }

    //! \brief Current mz interval iterator points to [mzStart, mzEnd)
    //  @see also NonUniformTileRange::mzTileInterval to obtain same info
    double mzStart() { return m_range.mzAt(m_tileX); }
    double mzEnd() { return m_range.mzAt(m_tileX + 1); }

    //! \brief return true if there are more tile elements available 
    bool hasNext() const;

    //! \brief Move to next tile element and provide it's value
    point2dList next();

    //! \brief Value of the last tile element visited
    point2dList value() const { return m_lastScanPart; }

    NonUniformTileRange range() const { return m_range; }

    //! \brief last tile index X that iterator operates on
    int lastTileXIndex() const;

    //! \brief last tile index Y that iterator operates on
    int lastTileYIndex() const;

    //! \brief allows you to restart the iteration from beginning
    void rewind();

    //! \brief used for optimization: allows you to set how many tiles
    // will be cached by iterator: useful only in use-case if you rewind
    // The sole purpose of this function is to provide a means of fine tuning performance
    // In general, you will rarely ever need to call this function. 
    void setCacheSize(int tileCount);

    //! \brief return true if iterator's  currently visited scan index is the last in the tile
    // can be used to signalize that we switch to next tile
    bool isLastVisitedScanIndexInTile() const;

private:
    void skipRows(int rowCount);
    void skipToNextTile();
    
    bool isInLastTile() const;

    void advance();

    void init();

private:
    void moveTo(int tileX, int tileY, int scanIndex);

private:
    NonUniformTileRange m_range;

    point2dList m_lastScanPart;

    RandomNonUniformTileIterator m_tileIterator;

    bool m_firstDone;

    QRect m_tileArea; // tile indexes

    int m_tileX;
    int m_tileY;
    int m_scanIndex;

    int m_firstScanIndex;
    int m_lastScanIndex;
};

_PMI_END

#endif // SEQUENTIAL_NON_UNIFORM_TILE_ITERATOR_H