/*
* Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef NON_UNIFORM_TILE_STORE_H
#define NON_UNIFORM_TILE_STORE_H

#include <common_math_types.h>
#include <pmi_core_defs.h>

#include "NonUniformTileStoreBase.h"

_PMI_BEGIN

typedef NonUniformTileStoreBase<point2dList> NonUniformTileStore;

typedef NonUniformTileStoreBase<QBitArray> NonUniformTileSelectionStore;

typedef NonUniformTileStoreBase<QVector<int>> NonUniformTileHillIndexStore;

template class PMI_COMMON_TILES_EXPORT NonUniformTileStoreBase<point2dList>;

template class PMI_COMMON_TILES_EXPORT NonUniformTileStoreBase<QBitArray>;

template class PMI_COMMON_TILES_EXPORT NonUniformTileStoreBase<QVector<int>>;

_PMI_END

Q_DECLARE_METATYPE(pmi::NonUniformTileStore::ContentType);

#endif // NON_UNIFORM_TILE_STORE_H
