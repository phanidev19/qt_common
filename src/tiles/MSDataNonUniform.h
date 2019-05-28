/*
* Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef MSDATA_NON_UNIFORM_H
#define MSDATA_NON_UNIFORM_H

// common_ms
#include "MSReaderTypes.h"
#include "ScanIndexNumberConverter.h"
#include "pmi_common_ms_export.h"

// common_tiles
#include "NonUniformTileRange.h"
#include "NonUniformTileStore.h"
#include "NonUniformTileManager.h"
#include "RandomNonUniformTileIterator.h"

// common_stable
#include "common_errors.h"
#include "common_math_types.h"
#include "pmi_core_defs.h"

#include <QVector>

class QDir;
class QRect;

_PMI_BEGIN

//TODO: add docs
class PMI_COMMON_MS_EXPORT MSDataNonUniform {

public:    
    MSDataNonUniform(NonUniformTileStore * store, const NonUniformTileRange &range, const ScanIndexNumberConverter &converter);
    
    MSDataNonUniform(NonUniformTileManager *tileManager, const NonUniformTileRange &range, const ScanIndexNumberConverter &converter);

    ~MSDataNonUniform();

    Err getScanData(long scanNumber, point2dList *points, bool doCentroiding = false) const;
    
    //! \brief for given XICWindow provides list of scan numbers and respective list of points
    //! TODO: think how to name this API: XICs are done on centroided data, this one allows you to check profile data too
    Err getXICData(const msreader::XICWindow &win, QVector<int> *scanNumbers, QVector<point2dList> *points,
                   bool doCentroiding = true) const;

    //! \brief computes XICData sums by fetching complete scans from tiles 
    Err getXICData(const msreader::XICWindow &win, point2dList *points);
    
    //! \brief this optimized version of getXICData tries to minimize the amount of db tile fetching
    //!
    //! It is achieved by not fetching the complete scan numbers out of the tile storage, but by iterating
    //! over respective tile by tile
    Err getXICDataNG(const msreader::XICWindow &win, point2dList *points);

    //! \brief Provide XIC for area defined only by tile rect
    Err getXICDataSequentialIterator(const QRect &tileRect, point2dList *points);

    //! \brief Computes XIC using MzScanIndexIterator
    Err getXICDataMzScanIndexIterator(const msreader::XICWindow &win, point2dList *points);

    const ScanIndexNumberConverter &converter() const { return m_converter; }

    //! \brief for given XICWindow computes the position of the first tile index (top-left of the QRect) and the count of tiles
    //! in two dimensions (tiles are conceptually 2d grid of tiles)
    QRect tileRect(const msreader::XICWindow &win) const;

    //! \brief Writes uniformly sampled data specified by XIC window into single rectangular buffer (one big tile)
    //! This buffer is mapped with TileRange provided (world-coordinates <-> tile coordinates)
    //!
    //! @param win - specifies which mz range and time range should be fetched 
    //! @param range - this range is used for mapping between world-coordinates and tile coordinates
    //! @param doCentroiding - true if data should be centroided, false if profile data should be written
    //! @param buffer - provide empty buffer for data, this function allocates space for it
    Err writeUniformData(const msreader::XICWindow &win, const TileRange &range, bool doCentroiding,
                         QVector<double> *buffer) const;

    //! \brief exports given tile pos to CSV file, file name will be constructed 
    // based on the tile properties like tile position and mz range for given tile
    bool exportTile(const QPoint &tilePos, NonUniformTileStore::ContentType type,
                    const QDir &dir) const;

    //! \brief returns only portion of the given scan number between [mzStart, mzEnd] both included
    Err getScanDataPart(int scanIndex, double mzStart, double mzEnd, point2dList *points, bool doCentroiding = false) const;

    //! \brief returns the tile manager, it is always present
    NonUniformTileManager *tileManager() const { return m_manager; }

private:
    // TODO: put this together in NonUniformTileDocumentPart?!
    mutable NonUniformTileManager *m_manager;
    bool m_ownedManager = false;
    NonUniformTileRange m_range;
    ScanIndexNumberConverter m_converter;

#ifdef PMI_QT_COMMON_BUILD_TESTING
    friend class MSDataNonUniformAdapterTest;
#endif
};

_PMI_END

#endif // MSDATA_NON_UNIFORM_H
