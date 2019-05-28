/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef NONUNIFORM_TILE_INTENSITY_INDEX_H
#define NONUNIFORM_TILE_INTENSITY_INDEX_H

#include <pmi_core_defs.h>

#include "pmi_common_tiles_export.h"

#include "NonUniformTilePoint.h"

#include <QScopedPointer>

_PMI_BEGIN

class IntensityIndexEntry
{
public:
    NonUniformTilePoint pos;
    double intensity = 0.0;
};

/**
 * @brief The NonUniformTileIntensityIndex class provides indexing of the tiles for faster lookups
 * of maximum intensity Tile entries are indexed
 *
 */
class PMI_COMMON_TILES_EXPORT NonUniformTileIntensityIndex
{

public:
    NonUniformTileIntensityIndex();
    ~NonUniformTileIntensityIndex();

    void insertIndexEntry(IntensityIndexEntry *entry);

    void updateIntensity(const IntensityIndexEntry &updatedEntryContent);

    QList<IntensityIndexEntry *> topIntensityEntries();

    void clear();

    QVector<IntensityIndexEntry *> indexEntries() const;

    double topIntensity() const;

    void dumpToCsvFile(const QString &filePath) const;

private:
    Q_DISABLE_COPY(NonUniformTileIntensityIndex)
    class Private;
    const QScopedPointer<Private> d;
};


_PMI_END

#endif // NONUNIFORM_TILE_INTENSITY_INDEX_H