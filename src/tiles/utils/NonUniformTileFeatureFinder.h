/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef NON_UNIFORM_TILE_FEATURE_FINDER_H
#define NON_UNIFORM_TILE_FEATURE_FINDER_H

#include <pmi_core_defs.h>
#include "pmi_common_ms_export.h"

#include "IntensityPriorityQueue.h"

#include <QScopedPointer>


class QString;
class QRectF;

_PMI_BEGIN

class MSDataNonUniformAdapter;
class NonUniformTileFeatureFinderPrivate;

class PMI_COMMON_MS_EXPORT NonUniformTileFeatureFinder {

public:    
    NonUniformTileFeatureFinder(MSDataNonUniformAdapter *doc);
    ~NonUniformTileFeatureFinder();

    //! \brief How many intensities should be on output
    void setTopKIntensities(int count);
    
    //! \brief Finds local maxima, but only within tile boundaries
    int findLocalMaxima();

    void saveToCsv(const QString &csvFilePath);
    
private:
    //! \brief area is in world coordinates: mz, scan index
    NonUniformIntensityPriorityQueue searchTileArea(const QRectF& area);

private:
    QScopedPointer<NonUniformTileFeatureFinderPrivate> d;
    Q_DISABLE_COPY(NonUniformTileFeatureFinder)
};

_PMI_END

#endif // NON_UNIFORM_TILE_FEATURE_FINDER_H