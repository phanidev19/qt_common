/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef TILE_RASTER_H
#define TILE_RASTER_H

#include "pmi_common_tiles_export.h"
#include "pmi_core_defs.h"

#include "TilePositionConverter.h"

#include <QRect>
#include <QVector>

_PMI_BEGIN

class PMI_COMMON_TILES_EXPORT TileRasterInfo
{
public:
    TileRasterInfo()
        : converter(TileRange())
    {
    }

    TilePositionConverter converter;
    QRect tileArea; // tile coordinates
};

/**
 * @brief Provides single buffer for tiles.
 *
 * We have tile (@see Tile class) that holds Tile::WIDTH * Tile::HEIGHT of intensity points
 * You might have use-case where you merge particular set of tiles into planar, rectangular raster-
 * bitmap of intensities. For example SpectrogramPlot in QAL is using this feature to integrate with
 * Qwt's classes properly.
 *
 */
class PMI_COMMON_TILES_EXPORT TileRaster
{

public:
    TileRaster(const QRect &tileArea, const TileRange &range);

    double value(int tileX, int tileY) const;

    bool isEmpty() const;

    void clear() { m_data.clear(); }

    QRect tileArea() const;

    QVector<double> *data() { return &m_data; }

    QVector<double> data() const { return m_data; }

    TilePositionConverter converter() const { return m_converter; }

    TileRange range() const { return m_converter.range(); }

    //! \brief resizes buffer to the memory that the tile area occupies
    // Same APIs require to have pre-allocated space
    bool allocateTileArea();

    TileRasterInfo info() const;

private:
    TilePositionConverter m_converter;
    QRect m_tileArea; // tile coordinates
    QVector<double> m_data;
};

_PMI_END

#endif // TILE_RASTER_H