/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "SequentialNonUniformTileIterator.h"
#include "MzScanIndexRect.h"
#include "pmi_common_tiles_debug.h"

#include "ScanIndexInterval.h"

_PMI_BEGIN

SequentialNonUniformTileIterator::SequentialNonUniformTileIterator(NonUniformTileManager *tileManager,
                                                                   const NonUniformTileRange &range,
                                                                   bool doCentroiding,
                                                                   const QRect &tileArea)
    : m_range(range)
    , m_tileIterator(tileManager, range, doCentroiding)
    , m_firstDone(false)
    , m_tileArea(tileArea)
{
    Q_ASSERT(!tileArea.isNull());
    Q_ASSERT(range.tileRect(MzScanIndexRect::fromQRectF(range.area())).contains(tileArea));
    init();
}

void SequentialNonUniformTileIterator::restrictScanIndexInterval(const ScanIndexInterval &interval)
{
    Q_ASSERT((interval.end() - interval.start() + 1) > 0);
    m_firstScanIndex = interval.start();
    m_lastScanIndex = interval.end();

    m_scanIndex = m_firstScanIndex;
}

void SequentialNonUniformTileIterator::init()
{
    m_tileX = m_tileArea.x();
    m_tileY = m_tileArea.y();

    int firstScanIndex = m_range.scanIndexAt(m_tileY);
    int lastTileY = lastTileYIndex();
    int lastScanIndex = qMin(m_range.lastScanIndexAt(lastTileY), m_range.scanIndexMax());

    restrictScanIndexInterval(ScanIndexInterval(firstScanIndex, lastScanIndex));
}

bool SequentialNonUniformTileIterator::hasNext() const
{
    if (isInLastTile() && (m_scanIndex == m_lastScanIndex) && m_firstDone) {
        return false;
    }

    return true;
}

bool SequentialNonUniformTileIterator::isInLastTile() const
{
    return (m_tileX == m_tileArea.right() && m_tileY == m_tileArea.bottom());
}

point2dList SequentialNonUniformTileIterator::next()
{
    advance();
    return m_lastScanPart;
}

void SequentialNonUniformTileIterator::skipRows(int rowCount)
{
    m_scanIndex += rowCount;
}

void SequentialNonUniformTileIterator::skipToNextTile()
{
    const int remainingTileCols = m_tileArea.right() - m_tileX;

    // if we can switch to very next tile, we go there
    if (remainingTileCols > 0) {
        // switch to next tile
        m_tileX++;
        // normalize to first scan index at current tile row
        m_scanIndex = std::max(m_range.scanIndexAt(m_tileY), m_firstScanIndex);
    } else {
        const int remainingTileRows = m_tileArea.bottom() - m_tileY;

        // we can't switch to next tile, we switch to next row of tiles
        if (remainingTileRows > 0) {
            // reset the tile x to the start
            m_tileX = m_tileArea.x();
            // move to next row of tiles
            m_tileY++;
            // reset scan index to first scan of the new tile row
            m_scanIndex = m_range.scanIndexAt(m_tileY);
        } else {
            // we are at the end, we should not get here
            qFatal("Trying to advance beyond the tile area!");
            return;
        }
    }
}

int SequentialNonUniformTileIterator::lastTileXIndex() const
{
    return m_tileArea.x() + m_tileArea.width() - 1;
}

int SequentialNonUniformTileIterator::lastTileYIndex() const
{
    return m_tileArea.y() + m_tileArea.height() - 1;
}

void SequentialNonUniformTileIterator::rewind()
{
    m_firstDone = false;
    m_tileX = m_tileArea.x();
    m_tileY = m_tileArea.y();
    restrictScanIndexInterval(ScanIndexInterval(m_firstScanIndex, m_lastScanIndex));

    m_lastScanPart.clear();
}

void SequentialNonUniformTileIterator::setCacheSize(int tileCount)
{
    m_tileIterator.setCacheSize(tileCount);
}

bool SequentialNonUniformTileIterator::isLastVisitedScanIndexInTile() const
{
    if (m_scanIndex == m_lastScanIndex) {
        return true;
    }
    
    if (m_scanIndex == m_range.lastScanIndexAt(m_tileY)) {
        return true;
    }

    return false;
}

void SequentialNonUniformTileIterator::moveTo(int tileX, int tileY, int scanIndex)
{
    m_tileIterator.moveTo(tileX, tileY, scanIndex);
}

void SequentialNonUniformTileIterator::advance()
{
    if (!m_firstDone) {
        moveTo(m_tileX, m_tileY, m_scanIndex);
        m_lastScanPart = m_tileIterator.value();
        m_firstDone = true;
        return;
    }

    if (!hasNext()) {
        warningTiles() << "Trying to advance beyond tile area!";
        return;
    }

    // single tile has rows
    const int remainingRowsInTile = m_tileIterator.numContiguousRows(m_scanIndex) - 1;
    // the whole are of scans has rows
    const int remainingAreaTileRows = m_lastScanIndex - m_scanIndex;

    const int remainingRows = qMin(remainingRowsInTile, remainingAreaTileRows);

    if (remainingRows > 0) {
        // we switch only to next row in current tile
        skipRows(1);
    } else if (remainingRows == 0) {
        // we switch to different tile here
        skipToNextTile();
    } else {
        QString msg = QString("Unexpected num of rows: %1").arg(remainingRows);
        qFatal(qPrintable(msg));
        return;
    }

    moveTo(m_tileX, m_tileY, m_scanIndex);
    m_lastScanPart = m_tileIterator.value();
}

_PMI_END