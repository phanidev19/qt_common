/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/


#ifndef NONUNIFORM_TILE_MAXINTENSITY_FINDER_H
#define NONUNIFORM_TILE_MAXINTENSITY_FINDER_H

#include "NonUniformTileManager.h"
#include "pmi_common_tiles_export.h"

#include <pmi_core_defs.h>

#include <QScopedPointer>

_PMI_BEGIN

class IntensityIndexEntry;
class NonUniformTileRange;
class NonUniformTileIntensityIndex;
class NonUniformTileDevice;

/*!
\class NonUniformTileMaxIntensityFinder
\brief NonUniformTileMaxIntensityFinder can index NonUniform tiles maximum intensity for fast lookup

\ingroup tiles

\reentrant
*/
class PMI_COMMON_TILES_EXPORT NonUniformTileMaxIntensityFinder
{

public:
    NonUniformTileMaxIntensityFinder(NonUniformTileDevice *device);

    ~NonUniformTileMaxIntensityFinder();

    void createIndexForAll(NonUniformTileIntensityIndex *index);

    void createIndexForTiles(const QRect &tileRect, NonUniformTileIntensityIndex *index);

    void updateIndexForTiles(NonUniformTileSelectionManager *selectionTileManager,
                             const QList<QPoint> &tilePositions,
                             NonUniformTileIntensityIndex *index);

    // for every tile in tile rect finds the position of the maxima and it's value
    void indexTileRect(NonUniformTileManager *tileManager, NonUniformTileStoreType::ContentType type,
                       const QRect &tileRect, QVector<IntensityIndexEntry *> *entries);

    void indexTileWithSelection(NonUniformTileManager *tileManager,
                                NonUniformTileSelectionManager *selectionTileManager, const QPoint &tilePos,
                                IntensityIndexEntry *indexEntry);

    // sets how many threads will be used for operations that support multi-threading
    void setThreadCount(int threadCount);

private:
    static bool isMaxima(const point2d &currentIntensity, const point2d &currentMaxima);

private:
    Q_DISABLE_COPY(NonUniformTileMaxIntensityFinder)
    class Private;
    const QScopedPointer<Private> d;
};

_PMI_END

#endif // NONUNIFORM_TILE_MAXINTENSITY_FINDER_H
