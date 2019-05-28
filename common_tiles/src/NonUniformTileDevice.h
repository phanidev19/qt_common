/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef NONUNIFORM_TILE_DEVICE_H
#define NONUNIFORM_TILE_DEVICE_H

#include "pmi_common_tiles_export.h"

#include <pmi_core_defs.h>

#include "NonUniformTileManager.h"
#include "NonUniformTileRange.h"
#include "NonUniformTileStore.h"
#include "RandomNonUniformTileIterator.h"

_PMI_BEGIN
/*
 * \brief The NonUniformTileDevice class holds together the basic elements of NonUniform tile
 *
 * Tile manager holds ms1 tiles within range provided, content says if the tiles have centroided
 * signal or not
 */
class PMI_COMMON_TILES_EXPORT NonUniformTileDevice
{

public:
    NonUniformTileDevice(NonUniformTileManager *tileManager, const NonUniformTileRange &range,
                         NonUniformTileStore::ContentType content);

    NonUniformTileDevice();

    ~NonUniformTileDevice();

    NonUniformTileRange range() const;

    NonUniformTileStore::ContentType content() const;

    NonUniformTileManager *tileManager();

    RandomNonUniformTileIterator createRandomIterator();

    bool doCentroiding() const;

    bool isNull() const;

private:
    Q_DISABLE_COPY(NonUniformTileDevice)
    class Private;
    const QScopedPointer<Private> d;
};

_PMI_END

#endif // NONUNIFORM_TILE_DEVICE_H