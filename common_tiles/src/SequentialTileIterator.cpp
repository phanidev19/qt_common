/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "SequentialTileIterator.h"
#include "TileManager.h"
#include "pmi_common_tiles_debug.h"

_PMI_BEGIN

SequentialTileIterator::SequentialTileIterator(TileManager *manager, const QRect &rect,
                                               const QPoint &level)
    : m_tileArea(rect)
    , m_level(level)
    , m_tileIterator(manager)
    , m_tileX(rect.x())
    , m_tileY(rect.y())
    , m_endPosition(rect.x() + rect.width() - 1, rect.y() + rect.height() - 1)
    , m_firstDone(false)

{
}

SequentialTileIterator::~SequentialTileIterator()
{
}

void SequentialTileIterator::advance()
{
    if (!m_firstDone) {
        m_tileIterator.moveTo(m_level, m_tileX, m_tileY);
        m_lastValue = m_tileIterator.value();
        m_firstDone = true;
        return;
    }

    if (!hasNext()) {
        warningTiles() << "Trying to advance beyond the area";
        return;
    }

    // single tile
    int remainingTileCols = m_tileIterator.numContiguousCols(m_tileX) - 1;
    // the whole area iterated
    int remainingAreaCols = m_tileArea.width() - (m_tileX - m_tileArea.x()) - 1;

    int remainingCols = qMin(remainingTileCols, remainingAreaCols);

    if (remainingCols > 0) {
        m_tileX++;
    } else {
        // switch to next row if available, or switch to next tile if available
        int remainingTileRows = m_tileIterator.numContiguousRows(m_tileY) - 1;

        int remainingAreaRows = m_tileArea.height() - (m_tileY - m_tileArea.y()) - 1;

        int remainingRows = qMin(remainingTileRows, remainingAreaRows);
        if (remainingRows > 0) {
            // switch to next row in tile
            m_tileX = qMax(TileManager::normalizeX(m_tileX), m_tileArea.x());
            m_tileY++;
        } else if (remainingRows == 0) {

            // if we can go to the very next tile
            if (remainingAreaCols > 0) {
                // switch to next tile
                m_tileX++;
                m_tileY = qMax(TileManager::normalizeY(m_tileY), m_tileArea.y());
            } else {
                // remainingAreaCols == 0
                if (remainingAreaRows > 0) {
                    // switch to next row of tiles
                    m_tileX = m_tileArea.x();
                    m_tileY++;
                } else {
                    // we are at the end, we should not get here
                    qFatal("Trying to advance beyond the tile area!");
                    return;
                }
            }
        }
    }

    m_tileIterator.moveTo(m_level, m_tileX, m_tileY);
    m_lastValue = m_tileIterator.value();
}

_PMI_END
