/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef MZ_SCANINDEX_NONUNIFORM_TILE_RECT_ITERATOR_H
#define MZ_SCANINDEX_NONUNIFORM_TILE_RECT_ITERATOR_H

#include "pmi_common_tiles_export.h"
#include "ScanIndexInterval.h"

#include "NonUniformTileManager.h"

// common_core_mini
#include "MzInterval.h"

// stable
#include <common_math_types.h>
#include <pmi_core_defs.h>

// Qt
#include <QScopedPointer>

_PMI_BEGIN

class NonUniformTileRange;
class MzScanIndexRect;

/**
* @brief MzScanIndexNonUniformTileRectIterator provides iterator for NonUniform tiles 
*
* It abstracts away all the details needed to access particular area defined by mz and scan index
* in the most efficient way: every tile from NonUniform tiles will be fetched just once 
*/
class PMI_COMMON_TILES_EXPORT MzScanIndexNonUniformTileRectIterator
{

public:
    MzScanIndexNonUniformTileRectIterator(NonUniformTileManager *tileManager,
                                          const NonUniformTileRange &range, bool doCentroiding,
                                          const MzScanIndexRect &area);

    ~MzScanIndexNonUniformTileRectIterator();

    //! \brief provides information to iterator's tile X index
    int x() const;
    //! \brief provides information to iterator's tile Y index
    int y() const;
    //! \brief provides information to iterator's current scan index
    int scanIndex() const;

    //! \brief provides information to iterator's current mz range
    double mzStart() const;
    double mzEnd() const;

    bool hasNext() const;

    point2dList next();

    point2dList value() const;

    int internalIndex() const;

private:
    void init();

    void advance();

    void extractValidMzRange(const point2dList &scanPart);

private:
    Q_DISABLE_COPY(MzScanIndexNonUniformTileRectIterator)
    class Private;
    const QScopedPointer<Private> d;
};

_PMI_END

#endif // MZ_SCANINDEX_NONUNIFORM_TILE_RECT_ITERATOR_H