/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "TileRaster.h"
#include "Tile.h"
#include "TileRange.h"

#include "pmi_common_tiles_debug.h"

#include <QRect>

_PMI_BEGIN

TileRaster::TileRaster(const QRect &tileArea, const TileRange &range)
    : m_tileArea(tileArea)
    , m_converter(range)
{
}

double TileRaster::value(int tileX, int tileY) const
{
    Q_ASSERT(m_tileArea.contains(tileX, tileY));
    Q_ASSERT(!m_data.isEmpty());

    int ix = tileX - m_tileArea.x();
    int iy = tileY - m_tileArea.y();

    int dataPosition = iy * m_tileArea.width() + ix;
    if (dataPosition >= m_data.size() || dataPosition < 0) {
        warningTiles() << "Invalid pos for tile position" << QPoint(tileX, tileY);
        return Tile::DEFAULT_TILE_VALUE;
    }

    return m_data.at(dataPosition);
}

bool TileRaster::isEmpty() const
{
    return m_data.isEmpty();
}

QRect TileRaster::tileArea() const
{
    return m_tileArea;
}

bool TileRaster::allocateTileArea()
{
    try {
        m_data.resize(m_tileArea.width() * m_tileArea.height());
    } catch (const std::bad_alloc &e) {
        warningTiles() << "Allocation failed for raster size" << m_tileArea.size() << e.what();
        return false;
    }

    return true;
}

TileRasterInfo TileRaster::info() const
{
    TileRasterInfo info;
    info.converter = m_converter;
    info.tileArea = m_tileArea;
    return info;
}

_PMI_END
