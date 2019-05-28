/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef NONUNIFORM_TILE_HILL_FINDER_H
#define NONUNIFORM_TILE_HILL_FINDER_H

#include "pmi_common_ms_export.h"

#include "NonUniformTileManager.h"
#include "NonUniformTileStoreBase.h"

#include <common_errors.h>
#include <common_math_types.h>
#include <pmi_core_defs.h>

#include <QScopedPointer>
#include <QVector>

_PMI_BEGIN

class MzScanIndexRect;
class NonUniformFeatureFindingSession;
struct NonUniformTilePoint;
class NonUniformHill;

/*!
\class NonUniformTileHillFinder
\brief NonUniformTileHillFinder is responsible for finding hills in provided NonUniform tiles

\ingroup tiles

\reentrant
*/
class PMI_COMMON_MS_EXPORT NonUniformTileHillFinder
{

public:
    /**
     * ZeroBounded - Main hill is built in a way where we grow from the most intense point til we
     * hit N consecutive empty scans
     *
     * ZScoreIntegration - Hill is built using integration with z-score algorithm: peaks in fixed
     * XIC window (+/- N minutes) are identified. Bounds of the peak in which the most
     * intense point is, are used to build the final hill.
     */
    enum class HillAlgorithm { ZeroBounded, ZScoreIntegration };

    explicit NonUniformTileHillFinder(NonUniformFeatureFindingSession *session);
    ~NonUniformTileHillFinder();

    NonUniformHill explainPeak(NonUniformTileSelectionManager *selectionTileManager, double mz, int scanIndex);

    /**
    * \brief Provides a mean to find hill based on other parent hill
    * \a parentHill and \a neighborMzValue is used as proxy/hint where to look for the neighbor hill
    */
    NonUniformHill explainNeighbor(NonUniformTileSelectionManager *selectionTileManager,
                         double neighborMzValue, const MzScanIndexRect &parentHill);

    void setMzTolerance(double mzTolerance);
    double mzTolerance() const;

    void makeHill(NonUniformTileSelectionManager *selectionTileManager, const MzScanIndexRect &area,
                  MzScanIndexRect *hillArea, QVector<NonUniformTilePoint> *hillPoints);

    /**
    * \brief Builds a hill with at least the point that tilePoint points to.
    * 
    * Hill is built in a way that it is guaranteed that the hill is not empty:
    * Existing tile point is taken and everything that is within \see mzTolerance() within
    * the same scan is added to the hill. 
    *
    * If \a tilePoint is not valid, empty hill is returned.
    */
    NonUniformHill makeDefaultHill(const NonUniformTilePoint &tilePoint);

    /**
    * \brief Changes the selection status of the \a points to being processed, selected
    */
    void markPointsAsProcessed(const QVector<NonUniformTilePoint> &points,
        NonUniformTileSelectionManager *selectionTileManager);

    /** 
    * \brief resets the id auto-incremented to initial value
    */
    void resetId();

    /**
    * \brief provides next id, auto-incremented with every call
    */
    int nextId();

private:
    //! \brief @see enum HillAlgorithm
    NonUniformHill buildHillWithNonZeroSignal(NonUniformTileSelectionManager *selectionTileManager, double mz, int scanIndex);
    
    //! \brief @see enum HillAlgorithm
    NonUniformHill buildHillWithIntegration(NonUniformTileSelectionManager *selectionTileManager, double mz, int scanIndex);
    //! \brief Computes XIC but respects selection; conventional XIC would be computed without
    //! selection support and thus include points that were already processed
    //! @see e.g. MSDataNonUniform::getXICDataNG(), MSReader::getXICData()
    Err getXICDataWithSelection(const MzScanIndexRect &window, point2dList &points);

private:
    Q_DISABLE_COPY(NonUniformTileHillFinder)
    class Private;
    const QScopedPointer<Private> d;
};

_PMI_END

#endif // NONUNIFORM_TILE_HILL_FINDER_H