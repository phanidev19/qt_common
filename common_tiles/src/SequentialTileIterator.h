/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef SEQUENTIAL_TILE_ITERATOR_H
#define SEQUENTIAL_TILE_ITERATOR_H

#include "RandomTileIterator.h"
#include "pmi_common_tiles_export.h"
#include "pmi_core_defs.h"

#include <QPoint>
#include <QRect>

_PMI_BEGIN

class TileManager;

class PMI_COMMON_TILES_EXPORT SequentialTileIterator
{

public:
    SequentialTileIterator(TileManager *manager, const QRect &rect, const QPoint &level);
    ~SequentialTileIterator();

    int x() const { return m_tileX; }

    int y() const { return m_tileY; }

    bool hasNext() const { return (m_tileX != m_endPosition.x() || m_tileY != m_endPosition.y()); }

    double next()
    {
        advance();
        return m_lastValue;
    }

    double value() { return m_lastValue; }

private:
    void advance();

private:
    QRect m_tileArea;
    QPoint m_level;

    int m_tileX;
    int m_tileY;

    QPoint m_endPosition;

    RandomTileIterator m_tileIterator;

    double m_lastValue;

    bool m_firstDone;
};

_PMI_END

#endif // SEQUENTIAL_TILE_ITERATOR_H