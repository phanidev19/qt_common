/*
* Copyright (C) 2016-2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef NON_UNIFORM_TILE_BUILDER_H
#define NON_UNIFORM_TILE_BUILDER_H

#include "pmi_core_defs.h"
#include "NonUniformTileRange.h"
#include "pmi_common_ms_export.h"

#include "NonUniformTileStore.h"
#include "NonUniformTileStoreMemory.h"
#include "ProgressBarInterface.h"

_PMI_BEGIN

class ScanIndexNumberConverter;
class MSReader;

class PMI_COMMON_MS_EXPORT NonUniformTileBuilder {

public:    
    NonUniformTileBuilder(const NonUniformTileRange &range);

    void buildNonUniformTiles(MSReader * reader, const ScanIndexNumberConverter &converter, NonUniformTileStore * store, NonUniformTileStore::ContentType type);
    void buildNonUniformTilesNG(MSReader *reader, const ScanIndexNumberConverter &converter,
                                NonUniformTileStore *store, NonUniformTileStore::ContentType type,
                                QSharedPointer<ProgressBarInterface> progress = NoProgress);

    bool buildNonUniformTileSelection(NonUniformTileStore *store,
                                      NonUniformTileStore::ContentType type,
                                      NonUniformTileSelectionStore *selectionStore,
                                      const QRect &tileArea);

    bool buildNonUniformTileHillIndex(NonUniformTileStore *store,
                                      NonUniformTileStore::ContentType type,
                                      NonUniformTileHillIndexStore *selectionStore,
                                      const QRect &tileArea);

private:
    bool flush(NonUniformTileStoreMemory * src, NonUniformTileStore * dst, NonUniformTileStore::ContentType type, int flushCount);

private:
    NonUniformTileRange m_range;
    
};

_PMI_END

#endif // NON_UNIFORM_TILE_BUILDER_H
