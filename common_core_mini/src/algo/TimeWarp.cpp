/*
 * Copyright (C) 2015 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author  : Marshall Bern bern@proteinmetrics.com
 */

#include <iostream>
#include "TimeWarp.h"
#include <MathUtils.h>
#include <math_utils.h>
#include "warp_core.h"
#include "QtSqlRowNodeUtils.h"
#include "PlotBase.h"
#include "PmiQtCommonConstants.h"

using namespace std;

_PMI_BEGIN

static ptrdiff_t GetIndexLessOrEqual(const vector<double> & list, double xloc, bool outOfBoundsOnRightReturnNegativeTwo) {
    vector<double>::const_iterator itr = std::lower_bound(list.begin(), list.end(), xloc);
    //if out of bounds on the right
    if (itr == list.end()) {
        if (outOfBoundsOnRightReturnNegativeTwo)
            return -2;
        else
            return list.size() - 1;
    }

    //911 TODO look into why lower bound is returning itr->x == xloc
    if (*itr == xloc) {
        return itr - list.begin();
    }

    //if out of bounds on the left
    if (itr == list.begin()) {
        return -1;
    }
    return itr - list.begin() - 1;
}

double TimeWarp::mapTime(const std::vector<double> & timeSource, const std::vector<double> & timeTarget, double time) {
    if (timeSource.size() <= 0)
        return time;
    if (timeSource.size() != timeTarget.size()) {
        debugCoreMini() << "timeSource.size() != timeTarget.size()";
        return time;
    }

    //find A's index, get alpha
    int srcIndex = GetIndexLessOrEqual(timeSource, time, false);
    if (srcIndex < 0) { //out of bounds on left
        double delta = timeSource[0] - time;
        return timeTarget[0] - delta;
    }

    // most right element
    if (srcIndex == (timeSource.size() - 1)) {
        return timeTarget.at(srcIndex);
    }

    if (srcIndex >= (int(timeSource.size()) - 1)) {  //out of bounds on right //Don't need to check this: m_timeAnchorA[srcIndex] <= originalTime
        double delta = timeTarget[srcIndex] - time;
        return timeTarget[srcIndex] - delta;
    }
    //The above two conditions should catch the case when there's only one time value.
    //Just to be safe, we should check for the single anchor point case.
    if (timeSource.size() == 1) {
        double delta = timeSource[0] - time;
        return timeTarget[0] - delta;
    }

    double startT = timeSource[srcIndex];
    double endT = timeSource[srcIndex+1];
    double denom = endT-startT;
    double alpha;
    if (denom == 0)
        alpha = 0;
    else
        alpha = (time - startT)/(endT-startT);
    if (alpha < 0) {
        cerr << "warning, alpha < 1, " << alpha << endl;
        alpha = 0;
    }
    if (alpha > 1) {
        cerr << "warning, alpha > 1, " << alpha << endl;
        alpha = 1;
    }

    //interpolate for B
    return MathUtils::lerp(timeTarget[srcIndex], timeTarget[srcIndex + 1], alpha);
}

double TimeWarp::warp(double originalTime) const {
    return mapTime(m_timeAnchorB, m_timeAnchorA, originalTime);
}

double TimeWarp::unwarp(double warpedTime) const {
    return mapTime(m_timeAnchorA, m_timeAnchorB, warpedTime);
}

point2d TimeWarp::warp(const point2d & originalTime) const {
    point2d p = originalTime;
    p.rx() = warp(p.x());
    return p;
}

point2d TimeWarp::unwarp(const point2d & warpedTime) const {
    point2d p = warpedTime;
    p.rx() = unwarp(p.x());
    return p;
}

void TimeWarp::warp(const point2dList & originalList, point2dList & warpedList) const {
    point2d p;
    warpedList.resize(originalList.size());
    for (unsigned int i = 0; i < originalList.size(); i++) {
        p = originalList[i];
        p.rx() = warp(p.x());
        warpedList[i] = p;
    }
}

void TimeWarp::warp(point2dList & warpedList) const {
    point2d p;
    for (unsigned int i = 0; i < warpedList.size(); i++) {
        p = warpedList[i];
        p.rx() = warp(p.x());
        warpedList[i] = p;
    }
}

void TimeWarp::unwarp(const point2dList & originalList, point2dList & warpedList) const {
    point2d p;
    warpedList.resize(originalList.size());
    for (unsigned int i = 0; i < originalList.size(); i++) {
        p = originalList[i];
        p.rx() = unwarp(p.x());
        warpedList[i] = p;
    }
}

void TimeWarp::unwarp(point2dList & warpedList) const {
    point2d p;
    for (unsigned int i = 0; i < warpedList.size(); i++) {
        p = warpedList[i];
        p.rx() = unwarp(p.x());
        warpedList[i] = p;
    }
}


Err TimeWarp::constructWarp(const point2dList & input_A_points, const point2dList & input_B_points, const TimeWarpOptions & options) {
    Err e = kNoErr;
    //m_options = options;
    //double minTime, maxTime;
    vector<double> A, B;
    pt2idx index_start, index_end, index_A_start;//, index_B_start;
    vector<int> knotsA, knotsB;
    double scaleA = 1, scaleB = 1;
    point2dList A_points;
    point2dList B_points;

    ptrdiff_t ia = 0;//, ib = 0;
    double time_a = 0, time_b = 0;
    double maxA = 0, maxB = 0, maxRatio = 0;
    ptrdiff_t Segments = 0;

    if (input_A_points.size() <= 0 || input_B_points.size() <= 0) {
        rrr(kBadParameterError);
    }

    //For now, reduce the total number of points by resampling A_points.
    //Later, we should adaptively resample based on critical points and
    //possibly other features (things close to critical points).
    //Also, we want to update the warping dynamic table to be more memory efficient.
    {
        PlotBase input_A_plotbase(input_A_points);
        if (int(input_A_points.size()) > options.maxTotalNumberOfPoints) {
            e = input_A_plotbase.makeResampledPlotMaxPoints(options.maxTotalNumberOfPoints, A_points); ree;
        } else if (!input_A_plotbase.isUniform()) {
            // The following requires that the plots are uniform, so resample.
            qDebug() << "Forced resampling of plot A because it was not uniform.";
            e = input_A_plotbase.makeResampledPlotMaxPoints(static_cast<int>(input_A_points.size()), A_points); ree;
        } else {
            A_points = input_A_points;
        }
        Q_ASSERT(PlotBase(A_points).isUniform());
    }

    // Make sure that pointsB is uniform as well
    {
        PlotBase input_B_plotbase(input_B_points);
        if (!input_B_plotbase.isUniform()) {
            qDebug() << "Forced resampling of plot B because it was not uniform.";
            e = input_B_plotbase.makeResampledPlotMaxPoints(static_cast<int>(input_B_points.size()), B_points); ree;
        } else {
            B_points = input_B_points;
        }
    }

    PlotBase B_plotbase(B_points);
    Q_ASSERT(B_plotbase.isUniform());

    //setup signal normalization
    maxA = std::abs(getMaxPoint(A_points).y());
    maxB = std::abs(getMaxPoint(B_points).y());
    maxRatio = maxA > maxB ? maxA/maxB : maxB/maxA;
    if (maxRatio > options.normalizeScaleFactor) {
        scaleA = 1/(maxA+1e-15);
        scaleB = 1/(maxB+1e-15);
    }

    index_start = 0;
    index_end = static_cast<pt2idx>(A_points.size());

    double b_start=0, b_end=1;
    B_plotbase.getXBound(&b_start, &b_end);

    A.resize(index_end-index_start);
    B.resize(index_end-index_start);
    for (size_t i = 0; i < A.size(); i++) {
        const point2d &p = A_points[i + index_start];
        A[i] = p.y() * scaleA;
        // Resample B to the sampling rate of A, e.g. A is TIC and B is UV
        double b_x = alpha_from_index_len(i, A.size()) * (b_end - b_start) + b_start;
        B[i] = B_plotbase.evaluate(b_x) * scaleB;
    }
    index_A_start = index_start;

    //Note: Array A and B initialized.  Computation should now be done using A and B (no more A_points B_points)
    if (A.size() <= 1 || B.size() <= 1) {
        cerr << "Warping not possible as subarray of intersection intervals not found." << endl;
        cerr << "Asubset.size()=" << A.size() << " Bsubset=" << B.size() << endl;
        rrr(kBadParameterError);
    }

    //Create evenly spaced knots, except the last one, which will be the largest.
    //time_per_index = (A_reference.back().x() - A_reference.front().x())/(A_reference.size()-1.0);
    //chunkSizePre = pmi::round(options.chunkTime / time_per_index);
    Segments = options.numberOfSegments;
    if (options.numberOfSamplesPerSegment > 0) {
        Segments = A.size()/options.numberOfSamplesPerSegment;
    }
    if (options.anchorTimeList.size() <= 0) {
        //Adjust for boundary cases
        double Asegf = double(A.size()) / Segments;
        if (Asegf < 4) {
            Asegf = 4;
            Segments = static_cast<int>(A.size())/Asegf;
            if (Segments <= 0) {
                Segments = 1;
            }
        }
        int Aseg = (int) floor( double(A.size()) / Segments);
        knotsA.resize(Segments+1);
        for (ptrdiff_t i = 0; i < Segments; i++) {
            knotsA[i] = static_cast<int>(i) * Aseg;
        }
        knotsA[Segments] = static_cast<int>(A.size());
    } else {
        //This branch not used currently; but one can imagine using critical points as anchor positions
        std::vector<pt2idx> anchorTimeIndexList;
        std::vector<double> anchorTimeList = options.anchorTimeList;
        std::sort(anchorTimeList.begin(), anchorTimeList.end());
        anchorTimeIndexList = findClosestIndexList(A_points, anchorTimeList);
        knotsA.clear();
        for (size_t i = 0; i < anchorTimeIndexList.size(); i++) {
            if (anchorTimeIndexList[i] >= 0) {
                knotsA.push_back(anchorTimeIndexList[i] - index_A_start);
            }
        }
        knotsA.push_back(static_cast<int>(A.size()));
    }

    warp_core(options.penalty, options.global_skew, A, B, knotsA, knotsB);

    if (knotsA.size() != knotsB.size()) {
        rrr(kError);
    }

    m_timeAnchorA.resize(knotsA.size());
    m_timeAnchorB.resize(knotsB.size());
    for (size_t i = 0; i < knotsA.size(); i++) {
        ia = knotsA[i] + index_A_start;

        if (ia < int(A_points.size()))
            time_a = A_points[ia].x();
        else
            time_a = A_points.back().x();
                
        //Note(Yong 2015-04-15): the -1 is needed to make sure the apex line up for the constraints.
        //But I haven't thought too carefully about if this is the most appropriate place for this patch.
        time_b = alpha_from_index_len(knotsB[i] - 1, A.size())*(b_end - b_start) + b_start;
    
        m_timeAnchorA[i] = time_a;
        m_timeAnchorB[i] = time_b;
    }

    return e;
}



Err TimeWarp::saveKnots(const QString &filename) const {
    Err e = kNoErr;
    QFile file(filename);
    if(!file.open(QIODevice::WriteOnly)) {
        e = kFileOpenError; ree;
    }

    QTextStream out(&file);
    for (unsigned int i = 0; i < m_timeAnchorA.size(); i++) {
        out << i << "," << m_timeAnchorA[i] << "," << m_timeAnchorB[i] << endl;
    }

    return e;
}

Err TimeWarp::setAnchors(const std::vector<double> & timeA, const std::vector<double> & timeB) {
    Err e = kNoErr;

    if (timeA.size() != timeB.size()) {
        debugCoreMini() << "timeA" << timeA.size();
        debugCoreMini() << "timeB" << timeB.size();
        rrr(kBadParameterError);
    }

    m_timeAnchorA.clear();
    m_timeAnchorB.clear();

    //Make sure all the times are in order. If not, remove them from becoming anchors
    for (unsigned int i = 0; i < timeA.size(); i++) {
        m_timeAnchorA.push_back(timeA[i]);
        m_timeAnchorB.push_back(timeB[i]);
    }
    //just sort for now, instead of removing them.
    stable_sort(m_timeAnchorA.begin(), m_timeAnchorA.end());
    stable_sort(m_timeAnchorB.begin(), m_timeAnchorB.end());

    return e;
}

Err TimeWarp::getAnchors(std::vector<double> *timeA, std::vector<double> *timeB) const
{
    Err e = kNoErr;

    *timeA = m_timeAnchorA;
    *timeB = m_timeAnchorB;

    return e;
}

Err TimeWarp::constructWarpWithContraints(const point2dList & A_points,
                                const point2dList & B_points,
                                const std::vector<double> & A_constraint,
                                const std::vector<double> & B_constraint,
                                const TimeWarpOptions & _options)
{
    Err e = kNoErr;
    clear();

    if (!isSorted(A_constraint) || !isSorted(B_constraint) || !isSortedAscendingX(A_points) || !isSortedAscendingX(B_points)) {
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

        //Add boundary points to contraints
        A_conts.push_back(A_points.front().x());
        A_conts.insert(A_conts.end(), A_constraint.begin(), A_constraint.end());
        A_conts.push_back(A_points.back().x());

        B_conts.push_back(B_points.front().x());
        B_conts.insert(B_conts.end(), B_constraint.begin(), B_constraint.end());
        B_conts.push_back(B_points.back().x());

        //Partition input data into parts and send each one off for warpping.
        for (int i = 1; i < int(A_conts.size()); i++) {
            double a1 = A_conts[i-1];
            double a2 = A_conts[i];
            double b1 = B_conts[i-1];
            double b2 = B_conts[i];
            point2dList outA, outB;
            TimeWarp warp;

            getPointsInBetween(A_points, a1, a2, outA, false);
            getPointsInBetween(B_points, b1, b2, outB, false);

            warp.constructWarp(outA, outB, opts);
            m_timeAnchorA.push_back(a1);
            m_timeAnchorB.push_back(b1);
            warp.appendAnchors(m_timeAnchorA, m_timeAnchorB);
            m_timeAnchorA.push_back(a2);
            m_timeAnchorB.push_back(b2);
        }

        //Shouldn't need it, but do currently.  The logic above isn't sound.
        stable_sort(m_timeAnchorA.begin(), m_timeAnchorA.end());
        stable_sort(m_timeAnchorB.begin(), m_timeAnchorB.end());
    }

    return e;
}

Err PlotWarpAnchors::loadFromDatabase(QSqlDatabase & db) {
    Err e = kNoErr;
    QSqlQuery q = makeQuery(db,true);

    const QString cmd_warp_anchors_template = "\
    SELECT w.PlotsId, w.MatchNumber, w.AnchorPosition + t.TranslateX as AnchorPosition, s.Type as SamplesType \
    FROM PlotsWarpAnchors w \
    JOIN Plots t ON t.Id = w.PlotsId \
    JOIN Samples s ON s.Id = t.SamplesId \
    ORDER BY w.MatchNumber \
    ";
//    WHERE PlotsId = %1 \
//    .arg(plotsId)

    e = QEXEC_CMD(q, cmd_warp_anchors_template); ree;
    extract(q, &m_warpAnchors);

    return e;
}

Err PlotWarpAnchors::saveToDatabase(QSqlDatabase & db) const {
    Err e = kNoErr;
    QSqlQuery q = makeQuery(db,true);
    QMap<qlonglong, double> plotsId_transX;

    e = QEXEC_CMD(q, "SELECT Id as PlotsId, TranslateX FROM Plots"); ree;
    while(q.next()) {
        qlonglong plotsId = q.value(0).toLongLong();
        double translate_x = q.value(1).toDouble();
        plotsId_transX[plotsId] = translate_x;
    }

    transaction_begin(db);
    e = QEXEC_CMD(q, "DELETE FROM PlotsWarpAnchors"); ree;
    e = QPREPARE(q, "INSERT INTO PlotsWarpAnchors(PlotsId, MatchNumber, AnchorPosition) VALUES(?,?,?)"); ree;

    for (int i =0; i < m_warpAnchors.getChildListSize(); i++) {
        const RowNode * child = m_warpAnchors.getChild(i);
        qlonglong plotsId = child->getDataAt(kPlotsId).toLongLong();
        qlonglong matchNumber = child->getDataAt(kMatchNumber).toLongLong();
        double anchor_pos = child->getDataAt(kAnchorPosition).toDouble();
        if (plotsId_transX.contains(plotsId)) {
            anchor_pos -= plotsId_transX[plotsId];
        } else {
            rrr(kBadParameterError);
        }
        q.bindValue(0,plotsId);
        q.bindValue(1,matchNumber);
        q.bindValue(2,anchor_pos);
        e = QEXEC_NOARG(q); ree;
    }
    transaction_end(db);

    return e;
}

qlonglong PlotWarpAnchors::getNextMatchNumber() const
{
    qlonglong matchNumber = 0;

    for (int i =0; i < m_warpAnchors.getChildListSize(); i++) {
        const RowNode * child = m_warpAnchors.getChild(i);
        if (matchNumber < child->getDataAt(kMatchNumber).toLongLong())
            matchNumber = child->getDataAt(kMatchNumber).toLongLong();
    }

    return matchNumber+1;
}

Err PlotWarpAnchors::getAnchorInfo(AnchorInfo & ref, QList<AnchorInfo> & nonRefList) const
{
    Err e = kNoErr;

    ref.clear();
    nonRefList.clear();

    QMap<qlonglong, AnchorInfo> refMap;
    QMap<qlonglong, AnchorInfo> nonRefMap;

    for (int i =0; i < m_warpAnchors.getChildListSize(); i++) {
        const RowNode * child = m_warpAnchors.getChild(i);
        qlonglong plotsId = child->getDataAt(kPlotsId).toLongLong();
        double anchor_pos = child->getDataAt(kAnchorPosition).toDouble();
        int matchNumber = child->getDataAt(kMatchNumber).toInt();
        //911 can't use plotsId
        if (child->getDataAt(kSamplesType).toString() == kReference) {
            refMap[plotsId].plotsId = plotsId;
            refMap[plotsId].matchNumber_Anchor[matchNumber] = anchor_pos;
        } else {
            nonRefMap[plotsId].plotsId = plotsId;
            nonRefMap[plotsId].matchNumber_Anchor[matchNumber] = anchor_pos;
        }
    }

    if (refMap.size() != 1) {
        rrr(kError);
    }
    ref = *refMap.begin();

    foreach(const AnchorInfo & info, nonRefMap) {
        nonRefList.push_back(info);
    }

    return e;
}

/*!
 * \brief add warp anchors between the reference and the non-reference.
 * \param selectedList of RowNode
 * \return Err
 */
Err PlotWarpAnchors::addWarpAnchors(QList<RowNode*> & _selectedList, QString & errMsg)
{
    Err e = kNoErr;
    QList<RowNode*> selectedList;
    errMsg.clear();
    qlonglong nextMatchNumber = -1;

    //Verify input selection.
    //TODO: consider what happens if the exactly same anchors are added again, or if something very close gets added.
    //Or if the newly added ones cross existing anchors.

    //Find the reference and make sure all of the others
    int ref_count = 0;
    QMap<qlonglong,int> plotsId_count;
    for (int i = 0; i < _selectedList.size(); i++) {
        RowNode * node = _selectedList[i];
        if (node->getChildListSize() > 0) {
            debugCoreMini() << "Ignoring row with children, peaksid:" << node->getDataAt(kPeaksId);
            continue;
        }
        selectedList.push_back(node);

        qlonglong plotsId = node->getDataAt(kPlotsId).toLongLong();
        if (plotsId_count.contains(plotsId)) {
            plotsId_count[plotsId] += 1;
        } else {
            plotsId_count[plotsId] = 1;
        }

        if (node->getDataAt(kSamplesType).toString() == kReference) {
            ref_count++;
        }
    }

    if (ref_count == 0) {
        errMsg += "There must be a peak selected from the reference trace.\n";
    }
    if (plotsId_count.size() <= 1) {
        errMsg += "One reference and at least one non-reference trace must have peak selection.\n";
    }
    foreach(int plotsId, plotsId_count.keys()) {
        int count = plotsId_count[plotsId];
        if (count != 1) {
            errMsg += QString("There must be exactly one peak per trace (PlotsId,count) = %1,%2\n").arg(plotsId).arg(count);
        }
    }
    if (errMsg.size() > 0) {
        errMsg = "The given selection could not be added for warping because:\n" + errMsg;
        rrr(kBadParameterError);
    }

    nextMatchNumber = getNextMatchNumber();
    for (int i = 0; i < _selectedList.size(); i++) {
        RowNode * node = _selectedList[i];
        if (node->isGroupNode()) {
            debugCoreMini() << "Ignoring row with children, PlotsId:" << node->getDataAt(kPlotsId);
            continue;
        }

        RowNode * newNode = m_warpAnchors.addNewChildMatchRootDataSize();
        newNode->getDataAt(kPlotsId) = node->getDataAt(kPlotsId);
        newNode->getDataAt(kMatchNumber) = nextMatchNumber;
        newNode->getDataAt(kAnchorPosition) = node->getDataAt(kApexTime);
        newNode->getDataAt(kSamplesType) = node->getDataAt(kSamplesType);
    }

    return e;
}

RowNode *
PlotWarpAnchors::getRowNode(qlonglong plotsId, qlonglong matchNumber)
{
    for (int i = 0; i < m_warpAnchors.getChildListSize(); i++)
    {
        RowNode * node = m_warpAnchors.getChild(i);
        if (node->getDataAt(kPlotsId).toLongLong() == plotsId &&
            node->getDataAt(kMatchNumber).toLongLong() == matchNumber)
        {
            return node;
        }
    }
    return NULL;
}

/*!
 * \brief change position of anchor
 * \param plotsId kPlotsId value
 * \param matchNumber kMatchNumber value
 * \param anchor_x new position
 * \return Err
 */
Err PlotWarpAnchors::editWarpAnchors(qlonglong plotsId, qlonglong matchNumber, double anchor_x)
{
    Err e = kNoErr;

    RowNode * node = getRowNode(plotsId, matchNumber);
    if (node) {
        node->getDataAt(kAnchorPosition) = anchor_x;
    } else {
        rrr(kBadParameterError);
    }

    return e;
}

/*!
 * \brief delete all matchNumber
 * \param matchNumber
 * \return Err
 */
Err PlotWarpAnchors::deleteWarpAnchors(qlonglong matchNumber)
{
    Err e = kNoErr;

    for (int i = m_warpAnchors.getChildListSize()-1; i >= 0; i--) {
        RowNode * child = m_warpAnchors.getChild(i);
        if (child->getDataAt(kMatchNumber).toLongLong() == matchNumber)
            m_warpAnchors.deleteChild(i);
    }

    return e;
}

void PlotWarpAnchors::deleteAllWarpAnchors()
{
    m_warpAnchors.clearChildren();
}


Err PlotWarpAnchors::makeAnchors(const AnchorInfo & ref, const AnchorInfo & nonRef,
                        std::vector<double> & refAnchors, std::vector<double> & nonRefAnchors)
{
    Err e = kNoErr;
    refAnchors.clear();
    nonRefAnchors.clear();

    const QMap<int,double> & refMA = ref.matchNumber_Anchor;
    const QMap<int,double> & nonRefMA = nonRef.matchNumber_Anchor;

    QList<int> keys = refMA.keys();
    foreach(int matchNumber, keys) {
        if (nonRefMA.contains(matchNumber)) {
            refAnchors.push_back(refMA[matchNumber]);
            nonRefAnchors.push_back(nonRefMA[matchNumber]);
        }
    }

    std::sort(refAnchors.begin(), refAnchors.end());
    std::sort(nonRefAnchors.begin(), nonRefAnchors.end());

    return e;
}

_PMI_END
