/*
* Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef NON_UNIFORM_TILE_H
#define NON_UNIFORM_TILE_H

#include "pmi_core_defs.h"

#include "common_math_types.h"
#include "pmi_common_tiles_export.h"

#include "NonUniformTileBase.h"

_PMI_BEGIN

typedef NonUniformTileBase<point2dList> NonUniformTile;

typedef NonUniformTileBase<QBitArray> NonUniformSelectionTile;

typedef NonUniformTileBase<QVector<int>> NonUniformHillIndexTile;

_PMI_END

#endif // NON_UNIFORM_TILE_H
