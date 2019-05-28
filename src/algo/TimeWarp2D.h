/*
 * Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author  : Michael Georgoulopoulos mgeorgoulopoulos@gmail.com
 */

#ifndef __TIME_WARP_2D_H__
#define __TIME_WARP_2D_H__

#include <common_errors.h>
#include <pmi_common_ms_export.h>
#include <pmi_core_defs.h>
#include <WarpElement.h>

#include <QList>

class QSqlDatabase;
class QString;

_PMI_BEGIN

/*!
    \brief This class represents a "time warp", which is a mapping from the time domain of
        machine file A to machine file B.

    * Use warp() to transform a time value from A to B.
    * Use unwarp() for the opposite.

    Normally, an object of this class will be initialized by \a MSMultiSampleTimeWarp2D.
*/
class PMI_COMMON_MS_EXPORT TimeWarp2D
{
public:
    struct TimeWarpOptions {
        int numberOfSegments;            //! This defines how the warp algorithm breaks down the input
        double stretchPenalty;            //! smaller number the more flexible, larger more rigid.
        double startTimeOffsetB;        //! We will use to grab more on B's begining to allow some amount
                                        //! of automatic translation.
        int globalSkew;                    //! dynamic programming table diagonal cannot be widers than 2 *
                                        //! global_skew.
        std::vector<double> anchorTimeList;
        int normalizeScaleFactor;        //! if the two signal max ratio if larger than this, normalize
        int numberOfSamplesPerSegment;    //! Number of points within each segment.  If this is
                                        //! non-zero, this option overrides other segmenting options.
                                        //! Use this option instead of others.
        int maxTotalNumberOfPoints;        //! Limit the total number to prevent memory allocation issue;
                                        //! later upgrade the warpping dynamic table to be more memory
                                        //! efficient.
        double mzMatchPpm;                //! ppm to consider two mz values as a "match"

        TimeWarpOptions()
        {
            numberOfSegments = 600;
            stretchPenalty = 0;
            startTimeOffsetB = 0;
            globalSkew = 500;
            normalizeScaleFactor = 0; //! zero means always normalize before scaling.
            numberOfSamplesPerSegment = 4; //! make each segment very small.
            maxTotalNumberOfPoints = 10000;
            mzMatchPpm = 100.0;
        }
    };

    TimeWarp2D() {}

    ~TimeWarp2D() {}

    void clear()
    {
        m_timeAnchorA.clear();
        m_timeAnchorB.clear();
    }

    // TODO: rename to warp_B_to_A
    double warp(double originalTime) const;

    // TODO: rename to warp_A_to_B
    double unwarp(double warp) const;

    point2d warp(const point2d &originalTime) const;

    point2d unwarp(const point2d &warp) const;

    void warp(const point2dList &originalList, point2dList &wrapedList) const;

    void warp(point2dList &pList) const;

    void unwarp(const point2dList &originalList, point2dList &wrapedList) const;

    void unwarp(point2dList &pList) const;

    /*!
     * \brief find the optimal anchors of warping of B to A
     * \param A_points reference points
     * \param B_points points to warp onto A
     * \param options warp parameters
     * \return error message
     */
    Err constructWarp(const TimedWarpElementList &inputPointsA, const TimedWarpElementList &pointsB,
                      const TimeWarpOptions &options);

    /*!
     * \brief Find the optimal anchors of warping of B to A, with segment contraints.
     * This will break up the input into segments and run warping on each region.
     * \param A_points input data points for A; must be sorted.
     * \param B_points input data points for B; must be sorted.
     * \param A_constraint constraints for A; must be sorted.
     * \param B_constraint constraints for B; must be sorted.
     * \param options warp parameters
     * \return kNoErr on success, kBadParameterError when invalid parameters are passed
     */
    Err constructWarpWithContraints(const TimedWarpElementList &A_points,
                                    const TimedWarpElementList &B_points,
                                    const std::vector<double> &A_constraint,
                                    const std::vector<double> &B_constraint,
                                    const TimeWarpOptions &options);

    /*!
     * \brief setAnchors direct assign warp anchors
     * \param timeA
     * \param timeB
     * \return
     */
    Err setAnchors(const std::vector<double> &timeA, const std::vector<double> &timeB);

    /*! \brief Returns the internal time anchors. */
    Err getAnchors(std::vector<double> *timeA, std::vector<double> *timeB) const;

    Err saveKnots(const QString &filename) const;

    void appendAnchors(std::vector<double> &alist, std::vector<double> &blist) const
    {
        alist.insert(alist.end(), m_timeAnchorA.begin(), m_timeAnchorA.end());
        blist.insert(blist.end(), m_timeAnchorB.begin(), m_timeAnchorB.end());
    }

    static double mapTime(const std::vector<double> &timeSource,
                          const std::vector<double> &timeTarget, double time);

private:
    /// These vectors contain mapping between two time space.
    ///   Example: A: 0,10,20,30
    ///   Example: B: 1,12,18,34
    ///   Given time at A as 10, what time would it be in B?
    ///   Answer: 12
    ///   Given time at B as 18, what time would it be in A?
    ///   Answer: 20
    ///   Given time at B as 26 (18+34)/2 what time would it be in A?
    ///   Answer: 25
    std::vector<double> m_timeAnchorA;
    std::vector<double> m_timeAnchorB;
};

_PMI_END

#endif
