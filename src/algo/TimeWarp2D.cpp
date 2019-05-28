/*
 * Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author  : Marshall Bern bern@proteinmetrics.com
 */

#include "TimeWarp2D.h"

#include <WarpCore2D.h>
#include <MathUtils.h>
#include <math_utils.h>

using namespace std;

_PMI_BEGIN

static ptrdiff_t GetIndexLessOrEqual(const vector<double> &list, double xloc,
                                     bool outOfBoundsOnRightReturnNegativeTwo)
{
    vector<double>::const_iterator itr = std::lower_bound(list.begin(), list.end(), xloc);
    // if out of bounds on the right
    if (itr == list.end()) {
        if (outOfBoundsOnRightReturnNegativeTwo)
            return -2;
        else
            return list.size() - 1;
    }

    // 911 TODO look into why lower bound is returning itr->x == xloc
    if (*itr == xloc) {
        return itr - list.begin();
    }

    // if out of bounds on the left
    if (itr == list.begin()) {
        return -1;
    }
    return itr - list.begin() - 1;
}

double TimeWarp2D::mapTime(const std::vector<double> &timeSource,
                                 const std::vector<double> &timeTarget, double time)
{
    if (timeSource.size() <= 0)
        return time;
    if (timeSource.size() != timeTarget.size()) {
        // debugCoreMini() << "timeSource.size() != timeTarget.size()";
        return time;
    }

    // find A's index, get alpha
    int srcIndex = GetIndexLessOrEqual(timeSource, time, false);
    if (srcIndex < 0) { // out of bounds on left
        double delta = timeSource[0] - time;
        return timeTarget[0] - delta;
    }

    // most right element
    if (srcIndex == (timeSource.size() - 1)) {
        return timeTarget.at(srcIndex);
    }

    if (srcIndex >= (int(timeSource.size()) - 1)) { // out of bounds on right //Don't need to check
                                                    // this: m_timeAnchorA[srcIndex] <= originalTime
        double delta = timeTarget[srcIndex] - time;
        return timeTarget[srcIndex] - delta;
    }
    // The above two conditions should catch the case when there's only one time value.
    // Just to be safe, we should check for the single anchor point case.
    if (timeSource.size() == 1) {
        double delta = timeSource[0] - time;
        return timeTarget[0] - delta;
    }

    double startT = timeSource[srcIndex];
    double endT = timeSource[srcIndex + 1];
    double denom = endT - startT;
    double alpha;
    if (denom == 0)
        alpha = 0;
    else
        alpha = (time - startT) / (endT - startT);
    if (alpha < 0) {
        cerr << "warning, alpha < 1, " << alpha << endl;
        alpha = 0;
    }
    if (alpha > 1) {
        cerr << "warning, alpha > 1, " << alpha << endl;
        alpha = 1;
    }

    // interpolate for B
    return MathUtils::lerp(timeTarget[srcIndex], timeTarget[srcIndex + 1], alpha);
}

double TimeWarp2D::warp(double originalTime) const
{
    return mapTime(m_timeAnchorB, m_timeAnchorA, originalTime);
}

double TimeWarp2D::unwarp(double warpedTime) const
{
    return mapTime(m_timeAnchorA, m_timeAnchorB, warpedTime);
}

point2d TimeWarp2D::warp(const point2d &originalTime) const
{
    point2d p = originalTime;
    p.rx() = warp(p.x());
    return p;
}

point2d TimeWarp2D::unwarp(const point2d &warpedTime) const
{
    point2d p = warpedTime;
    p.rx() = unwarp(p.x());
    return p;
}

void TimeWarp2D::warp(const point2dList &originalList, point2dList &warpedList) const
{
    point2d p;
    warpedList.resize(originalList.size());
    for (unsigned int i = 0; i < originalList.size(); i++) {
        p = originalList[i];
        p.rx() = warp(p.x());
        warpedList[i] = p;
    }
}

void TimeWarp2D::warp(point2dList &warpedList) const
{
    point2d p;
    for (unsigned int i = 0; i < warpedList.size(); i++) {
        p = warpedList[i];
        p.rx() = warp(p.x());
        warpedList[i] = p;
    }
}

void TimeWarp2D::unwarp(const point2dList &originalList, point2dList &warpedList) const
{
    point2d p;
    warpedList.resize(originalList.size());
    for (unsigned int i = 0; i < originalList.size(); i++) {
        p = originalList[i];
        p.rx() = unwarp(p.x());
        warpedList[i] = p;
    }
}

void TimeWarp2D::unwarp(point2dList &warpedList) const
{
    point2d p;
    for (unsigned int i = 0; i < warpedList.size(); i++) {
        p = warpedList[i];
        p.rx() = unwarp(p.x());
        warpedList[i] = p;
    }
}

Err TimeWarp2D::constructWarp(const TimedWarpElementList &inputPointsA,
                                    const TimedWarpElementList &inputPointsB,
                                    const TimeWarpOptions &options)
{
    Err e = kNoErr;
    // m_options = options;
    // double minTime, maxTime;
    vector<WarpElement> A, B;
    pt2idx indexStart;
    pt2idx indexEnd;
    pt2idx indexStartA;
    vector<int> knotsA;
    vector<int> knotsB;
    double scaleA = 1;
    double scaleB = 1;
    TimedWarpElementList pointsA;
    TimedWarpElementList pointsB;

    ptrdiff_t ia = 0; //, ib = 0;
    double timeA = 0;
    double timeB = 0;
    double maxA = 0;
    double maxB = 0;
    double maxRatio = 0;
    ptrdiff_t segmentCount = 0;

    if (inputPointsA.size() <= 0 || inputPointsB.size() <= 0) {
        rrr(kBadParameterError);
    }

    // For now, reduce the total number of points by resampling A_points.
    // Later, we should adaptively resample based on critical points and
    // possibly other features (things close to critical points).
    if (int(inputPointsA.size()) > options.maxTotalNumberOfPoints) {
        e = inputPointsA.makeResampledPlotMaxPoints(options.maxTotalNumberOfPoints, options.mzMatchPpm, pointsA); ree;
    } else if (!inputPointsA.isUniform()) {
        // The following requires that the plots are uniform, so resample.
        qDebug() << "Forced resampling of plot A because it was not uniform.";
        e = inputPointsA.makeResampledPlotMaxPoints(static_cast<int>(inputPointsA.size()), options.mzMatchPpm, pointsA); ree;
    } else {
        pointsA = inputPointsA;
    }

    Q_ASSERT(pointsA.isUniform());

    // Make sure that pointsB is uniform as well
    if (!inputPointsB.isUniform()) {
        qDebug() << "Forced resampling of plot B because it was not uniform.";
        e = inputPointsB.makeResampledPlotMaxPoints(static_cast<int>(inputPointsB.size()), options.mzMatchPpm, pointsB); ree;
    } else {
        pointsB = inputPointsB;
    }

    Q_ASSERT(pointsB.isUniform());

    // setup signal normalization
    maxA = std::abs(getMaxPoint(pointsA).warpElement.intensity());
    maxB = std::abs(getMaxPoint(pointsB).warpElement.intensity());
    maxRatio = maxA > maxB ? maxA / maxB : maxB / maxA;
    if (maxRatio > options.normalizeScaleFactor) {
        scaleA = 1 / (maxA + std::numeric_limits<double>::epsilon());
        scaleB = 1 / (maxB + 1e-15);
    }

    indexStart = 0;
    indexEnd = static_cast<pt2idx>(pointsA.size());

    double bStart = 0;
    double bEnd = 1;
    pointsB.getXBound(&bStart, &bEnd);

    A.resize(indexEnd - indexStart);
    B.resize(indexEnd - indexStart);
    for (size_t i = 0; i < A.size(); i++) {
        const TimedWarpElement &p = pointsA[i + indexStart];
        A[i] = p.warpElement;
        A[i].scale(scaleA);
        // Resample B to the sampling rate of A, e.g. A is TIC and B is UV
        double b_x = alpha_from_index_len(i, A.size()) * (bEnd - bStart) + bStart;
        B[i] = pointsB.evaluate(options.mzMatchPpm, b_x);
        B[i].scale(scaleB);
    }
    indexStartA = indexStart;

    // Note: Array A and B initialized.  Computation should now be done using A and B (no more
    // A_points B_points)
    if (A.size() <= 1 || B.size() <= 1) {
        cerr << "Warping not possible as subarray of intersection intervals not found." << endl;
        cerr << "Asubset.size()=" << A.size() << " Bsubset=" << B.size() << endl;
        e = kBadParameterError; ree;
    }

    // Create evenly spaced knots, except the last one, which will be the largest.
    // time_per_index = (A_reference.back().x() - A_reference.front().x())/(A_reference.size()-1.0);
    // chunkSizePre = pmi::round(options.chunkTime / time_per_index);
    segmentCount = options.numberOfSegments;
    if (options.numberOfSamplesPerSegment > 0) {
        segmentCount = A.size() / options.numberOfSamplesPerSegment;
    }
    if (options.anchorTimeList.size() <= 0) {
        // Adjust for boundary cases
        double segAFloat = double(A.size()) / segmentCount;
        const int minimumSampleCountPerSegment = 4;
        if (segAFloat < static_cast<double>(minimumSampleCountPerSegment)) {
            segAFloat = static_cast<double>(minimumSampleCountPerSegment);
            segmentCount = static_cast<int>(A.size()) / segAFloat;
            if (segmentCount <= 0) {
                segmentCount = 1;
            }
        }
        int segA = (int)floor(double(A.size()) / segmentCount);
        knotsA.resize(segmentCount + 1);
        for (ptrdiff_t i = 0; i < segmentCount; i++) {
            knotsA[i] = static_cast<int>(i) * segA;
        }
        knotsA[segmentCount] = static_cast<int>(A.size());
    } else {
        // This branch not used currently; but one can imagine using critical points as anchor
        // positions
        std::vector<int> anchorTimeIndexList;
        std::vector<double> anchorTimeList = options.anchorTimeList;
        std::sort(anchorTimeList.begin(), anchorTimeList.end());
        anchorTimeIndexList = findClosestIndexList(pointsA, anchorTimeList);
        knotsA.clear();
        for (size_t i = 0; i < anchorTimeIndexList.size(); i++) {
            if (anchorTimeIndexList[i] >= 0) {
                knotsA.push_back(anchorTimeIndexList[i] - indexStartA);
            }
        }
        knotsA.push_back(static_cast<int>(A.size()));
    }

    // Warp
    WarpCore2D warpCore;
    warpCore.setGlobalSkew(options.globalSkew);
    warpCore.setStretchPenalty(options.stretchPenalty);
    warpCore.setMzMatchPpm(options.mzMatchPpm);
    e = warpCore.constructWarp(A, B, knotsA, &knotsB); ree;

    if (knotsA.size() != knotsB.size()) {
        e = kError; ree;
    }

    m_timeAnchorA.resize(knotsA.size());
    m_timeAnchorB.resize(knotsB.size());
    for (size_t i = 0; i < knotsA.size(); i++) {
        ia = knotsA[i] + indexStartA;

        if (ia < int(pointsA.size()))
            timeA = pointsA[ia].timeInMinutes;
        else
            timeA = pointsA.back().timeInMinutes;

        // Note(Yong 2015-04-15): the -1 is needed to make sure the apex line up for the
        // constraints.  But I haven't thought too carefully about if this is the most appropriate
        // place for this patch.
        timeB = alpha_from_index_len(knotsB[i] - 1, A.size()) * (bEnd - bStart) + bStart;

        m_timeAnchorA[i] = timeA;
        m_timeAnchorB[i] = timeB;
    }

    return e;
}

Err TimeWarp2D::saveKnots(const QString &filename) const
{
    Err e = kNoErr;
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        e = kFileOpenError; ree;
    }

    QTextStream out(&file);
    for (unsigned int i = 0; i < m_timeAnchorA.size(); i++) {
        out << i << "," << m_timeAnchorA[i] << "," << m_timeAnchorB[i] << endl;
    }

    return e;
}

Err TimeWarp2D::setAnchors(const std::vector<double> &timeA, const std::vector<double> &timeB)
{
    Err e = kNoErr;

    if (timeA.size() != timeB.size()) {
        // debugCoreMini() << "timeA" << timeA.size();
        // debugCoreMini() << "timeB" << timeB.size();
        rrr(kBadParameterError);
    }

    m_timeAnchorA.clear();
    m_timeAnchorB.clear();

    // Make sure all the times are in order. If not, remove them from becoming anchors
    for (unsigned int i = 0; i < timeA.size(); i++) {
        m_timeAnchorA.push_back(timeA[i]);
        m_timeAnchorB.push_back(timeB[i]);
    }
    // just sort for now, instead of removing them.
    stable_sort(m_timeAnchorA.begin(), m_timeAnchorA.end());
    stable_sort(m_timeAnchorB.begin(), m_timeAnchorB.end());

    return e;
}

Err TimeWarp2D::getAnchors(std::vector<double> *timeA, std::vector<double> *timeB) const
{
    Err e = kNoErr;

    *timeA = m_timeAnchorA;
    *timeB = m_timeAnchorB;
    
    return e;
}

Err TimeWarp2D::constructWarpWithContraints(const TimedWarpElementList &A_points,
                                                  const TimedWarpElementList &B_points,
                                                  const std::vector<double> &A_constraint,
                                                  const std::vector<double> &B_constraint,
                                                  const TimeWarpOptions &_options)
{
    Err e = kNoErr;
    clear();

    if (!isSorted(A_constraint) || !isSorted(B_constraint) || !A_points.isSortedAscendingTime()
        || !B_points.isSortedAscendingTime()) {
        rrr(kBadParameterError);
    }
    if (A_constraint.size() != B_constraint.size()) {
        rrr(kBadParameterError);
    }
    if (A_constraint.size() == 0) {
        e = constructWarp(A_points, B_points, _options); ree;
    } else {
        TimeWarpOptions opts = _options;
        std::vector<double> A_conts;
        std::vector<double> B_conts;

        // Add boundary points to contraints
        A_conts.push_back(A_points.front().timeInMinutes);
        A_conts.insert(A_conts.end(), A_constraint.begin(), A_constraint.end());
        A_conts.push_back(A_points.back().timeInMinutes);

        B_conts.push_back(B_points.front().timeInMinutes);
        B_conts.insert(B_conts.end(), B_constraint.begin(), B_constraint.end());
        B_conts.push_back(B_points.back().timeInMinutes);

        // Partition input data into parts and send each one off for warpping.
        for (int i = 1; i < int(A_conts.size()); i++) {
            double a1 = A_conts[i - 1];
            double a2 = A_conts[i];
            double b1 = B_conts[i - 1];
            double b2 = B_conts[i];
            TimedWarpElementList outA, outB;
            TimeWarp2D warp;

            getPointsInBetween(A_points, a1, a2, outA, false);
            getPointsInBetween(B_points, b1, b2, outB, false);

            warp.constructWarp(outA, outB, opts);
            m_timeAnchorA.push_back(a1);
            m_timeAnchorB.push_back(b1);
            warp.appendAnchors(m_timeAnchorA, m_timeAnchorB);
            m_timeAnchorA.push_back(a2);
            m_timeAnchorB.push_back(b2);
        }

        // Shouldn't need it, but do currently.  The logic above isn't sound.
        stable_sort(m_timeAnchorA.begin(), m_timeAnchorA.end());
        stable_sort(m_timeAnchorB.begin(), m_timeAnchorB.end());
    }

    return e;
}

_PMI_END
