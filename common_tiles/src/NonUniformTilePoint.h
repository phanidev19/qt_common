/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef NONUNIFORM_POINT_H
#define NONUNIFORM_POINT_H

#include "pmi_common_tiles_export.h"

#include <pmi_core_defs.h>

#include <QPoint>

_PMI_BEGIN

/**
 * @brief The NonUniformTilePoint class serves as pointer to location in the tile, it points to mz,
 * intensity point
 *
 * To optain the mz, intensity value, you can use RandomNonUniformTileIterator::moveTo(tilePos,
 * scanIndex).value().at(internalIndex)
 *
 */
struct PMI_COMMON_TILES_EXPORT NonUniformTilePoint {
    static const QPoint INVALID_TILE_POSITION;
    
    // tile location
    QPoint tilePos = INVALID_TILE_POSITION;
    // location within tile
    int scanIndex = 0;
    int internalIndex = 0; // equal to the mz point

    bool isValid() const {
        return tilePos != INVALID_TILE_POSITION;
    }
};

_PMI_END

#endif // NONUNIFORM_POINT_H