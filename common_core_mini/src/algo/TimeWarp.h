/*
 * Copyright (C) 2015 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author  : Marshall Bern bern@proteinmetrics.com
 */

#ifndef __TIME_WARP_H__
#define __TIME_WARP_H__

#include <QList>
#include "RowNode.h"
#include "pmi_common_core_mini_export.h"

class QSqlDatabase;
class QString;

_PMI_BEGIN

class PMI_COMMON_CORE_MINI_EXPORT TimeWarp
{
public:
    struct TimeWarpOptions
    {
        int numberOfSegments; //! This defines how the warp algorithm breaks down the input
        double penalty; //! smaller number the more flexible, larger more rigid.
        double B_startTimeOffset; //! We will use to grab more on B's begining to allow some amount of automatic translation.
        int global_skew; //! dynamic programming table diagonal cannot be widers than 2 * global_skew.
        std::vector<double> anchorTimeList;
        int normalizeScaleFactor; //! if the two signal max ratio if larger than this, normalize
        int numberOfSamplesPerSegment;  //! Number of points within each segment.  If this is non-zero, this option overrides other segmenting options.  Use this option instead of others.
        int maxTotalNumberOfPoints; //! Limit the total number to prevent memory allocation issue; later upgrade the warpping dynamic table to be more memory efficient.
        TimeWarpOptions() {
            numberOfSegments = 600;
            penalty = 0;
            B_startTimeOffset = 0;
            global_skew = 500;
            normalizeScaleFactor = 0; //! zero means always normalize before scaling.
            numberOfSamplesPerSegment = 4;  //! make each segment very small.
            maxTotalNumberOfPoints = 10000;
        }
    };

    TimeWarp() {
    }

    ~TimeWarp() {
    }

    void clear() {
        m_timeAnchorA.clear();
        m_timeAnchorB.clear();
    }

    //TODO: rename to warp_B_to_A
    double warp(double originalTime) const;

    //TODO: rename to warp_A_to_B
    double unwarp(double warp) const;

    point2d warp(const point2d & originalTime) const;

    point2d unwarp(const point2d & warp) const;

    void warp(const point2dList & originalList, point2dList & wrapedList) const;

    void warp(point2dList & pList) const;

    void unwarp(const point2dList & originalList, point2dList & wrapedList) const;

    void unwarp(point2dList & pList) const;

    /*!
     * \brief find the optimal anchors of warping of B to A
     * \param A_points reference points
     * \param B_points points to warp onto A
     * \param options warp parameters
     * \return error message
     */
    Err constructWarp(const point2dList & input_A_points, const point2dList & B_points, const TimeWarpOptions & options);


    /*!
     * \brief Find the optimal anchors of warping of B to A, with segment contraints.
     * This will break up the input into segments and run warping on each region.
     * \param A_points input data points for A; must be sorted.
     * \param B_points input data points for B; must be sorted.
     * \param A_constraint constraints for A; must be sorted.
     * \param B_constraint constraints for B; must be sorted.
     * \param options warp parameters
     * \return Err
     */
    Err constructWarpWithContraints(const point2dList & A_points,
                                    const point2dList & B_points,
                                    const std::vector<double> & A_constraint,
                                    const std::vector<double> & B_constraint,
                                    const TimeWarpOptions & options);


    /*!
     * \brief setAnchors direct assign warp anchors
     * \param timeA
     * \param timeB
     * \return
     */
    Err setAnchors(const std::vector<double> & timeA, const std::vector<double> & timeB);

    /*! \brief Returns the internal time anchors. */
    Err getAnchors(std::vector<double> *timeA, std::vector<double> *timeB) const;

    Err saveKnots(const QString &filename) const;

    void appendAnchors(std::vector<double> & alist, std::vector<double> & blist) const {
        alist.insert(alist.end(), m_timeAnchorA.begin(), m_timeAnchorA.end());
        blist.insert(blist.end(), m_timeAnchorB.begin(), m_timeAnchorB.end());
    }

    static double mapTime(const std::vector<double> & timeSource, const std::vector<double> & timeTarget, double time);

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


class PMI_COMMON_CORE_MINI_EXPORT PlotWarpAnchors
{
public:
    struct AnchorInfo
    {
        qlonglong plotsId;
        QMap<int,double> matchNumber_Anchor;
        void clear() {
            plotsId = -1;
            matchNumber_Anchor.clear();
        }
    };

    PlotWarpAnchors() {
    }

    void clear() {
        deleteAllWarpAnchors();
    }

    Err loadFromDatabase(QSqlDatabase & db);

    Err saveToDatabase(QSqlDatabase & db) const;

    /*!
     * \brief Children contains of kPlotsId, kMatchNumber, and kAnchorPosition x-positions
     * \return
     */
    const RowNode & getWarpAnchors() const {
        return m_warpAnchors;
    }

    /*!
     * \brief add warp anchors between the reference and the non-reference.
     * \param selectedList of RowNode
     * \return Err
     */
    Err addWarpAnchors(QList<RowNode*> & selectedList, QString & errMsg);

    /*!
     * \brief change position of anchor
     * \param plotsId kPlotsId value
     * \param matchNumber kMatchNumber value
     * \param anchor_x new position
     * \return Err
     */
    Err editWarpAnchors(qlonglong plotsId, qlonglong matchNumber, double anchor_x);

    /*!
     * \brief delete all matchNumber
     * \param matchNumber
     * \return Err
     */
    Err deleteWarpAnchors(qlonglong matchNumber);

    void deleteAllWarpAnchors();

    /*!
     * \brief get reference and non-reference anchor info
     * \param ref_plotsId
     * \param nonRefPlotsIdList
     * \return
     */
    Err getAnchorInfo(AnchorInfo & refInfo, QList<AnchorInfo> & nonRefInfo) const;

    static Err makeAnchors(const AnchorInfo & ref, const AnchorInfo & nonRef,
                            std::vector<double> & refAnchors, std::vector<double> & nonRefAnchors);

protected:
    qlonglong getNextMatchNumber() const;

    RowNode * getRowNode(qlonglong plotsId, qlonglong matchNumber);
private:
    RowNode m_warpAnchors;
};

_PMI_END

#endif


