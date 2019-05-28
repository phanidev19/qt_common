/*
 * Copyright (C) 2012-2014 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 */

#include "PlotBase.h"
#include "PointListUtils.h"
#include "Calibration.h"
#include "MathUtils.h"
#include "pmi_common_core_mini_debug.h"

#include <math_utils.h>
#include <qt_utils.h>
#include <math_utils_chemistry.h>
#include <plot_utils.h>

#include <QFile>
#include <QTextStream>
#include <QStringList>
#include <QVector2D>
#include <QLine>

#include <float.h>
#include <string>

using namespace std;
_PMI_BEGIN

/*!
 * \brief getIntersection returns the intersection
 * \param l1 line one
 * \param l2 line two
 * \param intersectionPoint the intersection, if it exists
 * \return true if intersection found.
 */
static bool
getIntersection(const line2d & l1, const line2d & l2, point2d & intersectionPoint, double det_fudge = 1e-30) {
    const point2d & p1 = l1.a;
    const point2d & p2 = l1.b;
    const point2d & p3 = l2.a;
    const point2d & p4 = l2.b;

    double det = (p1.x()-p2.x())*(p3.y()-p4.y())-(p1.y()-p2.y())*(p3.x()-p4.x());
    if (det == 0 )
        return false;
    if (is_close(det,0,det_fudge)) {
        return false;
    }

    intersectionPoint.rx() = ((p1.x()*p2.y()-p1.y()*p2.x())*(p3.x()-p4.x())-(p1.x()-p2.x())*(p3.x()*p4.y()-p3.y()*p4.x()))/(det);
    intersectionPoint.ry() = ((p1.x()*p2.y()-p1.y()*p2.x())*(p3.y()-p4.y())-(p1.y()-p2.y())*(p3.x()*p4.y()-p3.y()*p4.x()))/(det);
    return true;
}


/*!
 * Compute area of the trapezoid, above or below the x-axis.
 * If the two input points cross the x-axis, then compute them as two seperate triangles.
 * note: trapezoid area = (a+b)/2 * h
 */
static double
computeAreaTrapezoid(const point2d & p1, const point2d & p2, PlotBase::ComputeAreaMethod method)
{
    double h1,h2,area_A,area_B,a,b,h;

    //zero crossing handled as two seperate triangles, one positive and other negative area
    if ((p1.y() > 0 && p2.y() < 0) || (p1.y() < 0 && p2.y() > 0)) {
        point2d intersectionPoint;
        bool val = getIntersection(line2d(point2d(p1.x(),0),point2d(p2.x(),0)),
                                   line2d(p1,p2),
                                   intersectionPoint);
        if (!is_between(p1.x(),p2.x(),intersectionPoint.x()) || !val) {
            debugCoreMini() << "warning, intersection not found, returning zero area: p1,p2,intersection, val =" << p1 << "," << p2 << "," << intersectionPoint << " val=" << val;
            return 0;
        }

        h1 = intersectionPoint.x() - p1.x();
        h2 = p2.x() - intersectionPoint.x();
        //Note: one of these area will be negative; this is needed for computing area between two curves.
        area_A = std::abs(h1) * p1.y();
        area_B = std::abs(h2) * p2.y();
        if (method == PlotBase::ComputeAreaMethod_IgonoreNegativeAreas) {
            if (area_A < 0) {
                area_A = 0;
            }
            if (area_B < 0) {
                area_B = 0;
            }
        }
        return 0.5 * (area_A + area_B);
    }

    a = p1.y();
    b = p2.y();
    h = p2.x() - p1.x();
    area_A = h * (a+b)*0.5;
    if (method == PlotBase::ComputeAreaMethod_IgonoreNegativeAreas && area_A < 0) {
        area_A = 0;
    }
    return area_A;
}

/*
class FooGoo {
public:
    FooGoo() {
        double area1,area2;
        area1 = computeAreaTrapezoid(point2d(1,1), point2d(4,-2));
        area2 = computeAreaTrapezoid(point2d(1,-1), point2d(4,2));
    }
};
FooGoo sdfioj;
*/

void PlotBase::applyRandomYScale(double scaleAmount) {
    for (unsigned int i = 0; i < m_pointList.size(); i++) {
        point2d p = m_pointList[i];
        p.ry() = p.y() * (1 + rand01()*scaleAmount);
        m_pointList[i] = p;
    }
}

std::vector<point2d>
PlotBase::getAreaPointList(double t_start, double t_end, bool includeOutOfDomainEndpoints, bool useBinarySearch) const
{
    point2d start, end;
    std::vector<point2d> areaPointList;

    if (m_pointList.size() <= 0)
        return areaPointList;

    start.rx() = t_start;
    end.rx() = t_end;
    start.ry() = evaluate(start.x());// * rand01() * 1.5;  //scaled for debugging purposes.
    end.ry() = evaluate(end.x());// * rand01()* 1.5;

    //The start & end only get added if it's inside the m_pointList domain.
    //Trying to recall why this is, and it's probably due to XIC computation:
    //If no values are found at t_start, it should not try to compute triangle area between start and m_pointList[0].
    //This would add extra synthetic/undesired area value.
    if (start.x() > m_pointList[0].x())
        areaPointList.push_back(start);
    else {
        //out side of domain
        if (includeOutOfDomainEndpoints) {
            areaPointList.push_back(start);
        }
    }
    if (useBinarySearch && m_isSortedAscendingX) {
        pt2idx idx = getIndexLessOrEqual(start.x());
        const int start_idx = std::max(0, idx);
        for (unsigned int i = start_idx; i < m_pointList.size(); i++) {
            const point2d & point = m_pointList[i];
            if (start.x() <= point.x() && point.x() <= end.x() ) {
                areaPointList.push_back(point);
            } else if (point.x() > end.x())
                break;
        }
    } else {
        for (unsigned int i = 0; i < m_pointList.size(); i++) {
            const point2d & point = m_pointList[i];
            if (start.x() <= point.x() && point.x() <= end.x() ) {
                areaPointList.push_back(point);
            }
        }
    }
    if (end.x() < m_pointList.back().x())
        areaPointList.push_back(end);
    else {
        //out side of domain
        if (includeOutOfDomainEndpoints) {
            areaPointList.push_back(end);
        }
    }
    return areaPointList;
}

inline void updateMinMaxPoint(const point2d & newpoint, point2d * currMin, point2d * currMax) {
    if (newpoint.y() > currMax->y()) {
        *currMax = newpoint;
    }
    if (newpoint.y() < currMin->y()) {
        *currMin = newpoint;
    }
}

bool PlotBase::getMinMaxPoints(double t_start, double t_end, point2d *minPoint,
                               point2d *maxPoint) const
{
    *minPoint = point2d(0, 0);
    *maxPoint = point2d(1, 1);

    if (m_pointList.size() <= 0)
        return false;

    const point2d start(t_start, evaluate(t_start));
    const point2d end(t_end, evaluate(t_end));

    *minPoint = start;
    *maxPoint = start;
    updateMinMaxPoint(end, minPoint, maxPoint);

    if (m_isSortedAscendingX) {
        pt2idx idx = getIndexLessOrEqual(start.x());
        const int start_idx = std::max(0, idx);
        for (unsigned int i = start_idx; i < m_pointList.size(); i++) {
            const point2d & point = m_pointList[i];
            if (start.x() <= point.x() && point.x() <= end.x()) {
                updateMinMaxPoint(point, minPoint, maxPoint);
            }
            else if (point.x() > end.x())
                break;
        }
    }
    else {
        for (unsigned int i = 0; i < m_pointList.size(); i++) {
            const point2d & point = m_pointList[i];
            if (start.x() <= point.x() && point.x() <= end.x()) {
                updateMinMaxPoint(point, minPoint, maxPoint);
            }
        }
    }
    return true;
}


std::vector<point2d>
PlotBase::getPointsBetween(double t_start, double t_end, bool useBinarySearch) const
{
    point2d start, end;
    std::vector<point2d> areaPointList;

    if (m_pointList.size() <= 0)
        return areaPointList;

    start.rx() = t_start;
    end.rx() = t_end;
    start.ry() = evaluate(start.x());// * rand01() * 1.5;  //scaled for debugging purposes.
    end.ry() = evaluate(end.x());// * rand01()* 1.5;

    //The start & end only get added if it's inside the m_pointList domain.
    //Trying to recall why this is, and it's probably due to XIC computation:
    //If no values are found at t_start, it should not try to compute triangle area between start and m_pointList[0].
    //This would add extra synthetic/undesired area value.
    if (useBinarySearch && m_isSortedAscendingX) {
        pt2idx idx = getIndexLessOrEqual(start.x());
        const int start_idx = std::max(0, idx);
        for (unsigned int i = start_idx; i < m_pointList.size(); i++) {
            const point2d & point = m_pointList[i];
            if (start.x() <= point.x() && point.x() <= end.x()) {
                areaPointList.push_back(point);
            }
            else if (point.x() > end.x())
                break;
        }
    }
    else {
        for (unsigned int i = 0; i < m_pointList.size(); i++) {
            const point2d & point = m_pointList[i];
            if (start.x() <= point.x() && point.x() <= end.x()) {
                areaPointList.push_back(point);
            }
        }
    }
    return areaPointList;
}


std::vector<point2d>
PlotBase::getDataPoints(double t_start, double t_end, bool useBinarySearch) const
{
    std::vector<point2d> areaPointList;

    if (m_pointList.size() <= 0)
        return areaPointList;

    if (useBinarySearch && m_isSortedAscendingX) {
        pt2idx idx = getIndexLessOrEqual(t_start);
        const int start_idx = std::max(0, idx);
        for (unsigned int i = start_idx; i < m_pointList.size(); i++) {
            const point2d & point = m_pointList[i];
            if (t_start <= point.x() && point.x() <= t_end ) {
                areaPointList.push_back(point);
            } else if (point.x() > t_end)
                break;
        }
    } else {
        for (unsigned int i = 0; i < m_pointList.size(); i++) {
            const point2d & point = m_pointList[i];
            if (t_start <= point.x() && point.x() <= t_end ) {
                areaPointList.push_back(point);
            }
        }
    }
    return areaPointList;
}

void PlotBase::getPoints(double t_start, double t_end, bool inclusive, point2dList & plist) const {
    getPointsInBetween(m_pointList, t_start, t_end, plist, inclusive);
}

/*!
 * \brief getPointIndexBetween
 * \param plist
 * \param t_start
 * \param t_end
 * \param inclusive if true and if t_start and t_end matches in the input, those are considred as part of the output.
 * \param idx_start
 * \param idx_end
 */
void getPointIndexBetween(const point2dList & plist,
                        double t_start, double t_end, bool inclusive,
                        pt2idx * idx_start, pt2idx * idx_end)
{
    if (plist.size() <= 0)
        return;
    *idx_start = *idx_end = -1;

    pt2idx idx = GetIndexLessOrEqual(plist, t_start, false);
    const int start_idx = std::max(0,idx);
    for (int i = start_idx; i < int(plist.size()); i++) {
        const point2d & point = plist[i];
        if (t_start == point.x()) {
            if (inclusive) {
                if (*idx_start == -1) *idx_start = i;
                *idx_end = i;
            }
        }
        else if (t_start < point.x() && point.x() < t_end ) {
            if (*idx_start == -1) *idx_start = i;
            *idx_end = i;
        } else if (point.x() == t_end) {
            if (inclusive) {
                if (*idx_start == -1) *idx_start = i;
                *idx_end = i;
            }
            break;
        } else if (point.x() > t_end) {
            break;
        }
    }
}

void getPointsInBetween(const point2dList & plist, double t_start, double t_end, point2dList & outList, bool inclusive)
{
    outList.clear();
    if (plist.size() <= 0)
        return;

    pt2idx idx_start = -1, idx_end = -1;

    getPointIndexBetween(plist, t_start, t_end, inclusive, &idx_start, &idx_end);
    if (idx_start < 0 || idx_end < 0) {
        return;
    }
    for (int i = idx_start; i <= idx_end; i++) {
        outList.push_back(plist[i]);
    }
}

double getYMaxBetween(const point2dList & plist, double t_start, double t_end, bool inclusive)
{
    double maxval = 0;
    if (plist.size() <= 0)
        return maxval;

    pt2idx idx_start = -1, idx_end = -1;

    getPointIndexBetween(plist, t_start, t_end, inclusive, &idx_start, &idx_end);
    if (idx_start < 0 || idx_end < 0) {
        return maxval;
    }
    for (int i = idx_start; i <= idx_end; i++) {
        if (maxval < plist[i].y())
            maxval = plist[i].y();
    }
    return maxval;
}

double getYSumBetween(const point2dList & plist, double t_start, double t_end, bool inclusive)
{
    double sum = 0;
    if (plist.size() <= 0)
        return sum;

    pt2idx idx_start = -1, idx_end = -1;

    getPointIndexBetween(plist, t_start, t_end, inclusive, &idx_start, &idx_end);
    if (idx_start < 0 || idx_end < 0) {
        return sum;
    }
    for (int i = idx_start; i <= idx_end; i++) {
        sum += plist[i].y();
    }
    return sum;
}

double getYSumAll(const point2dList & plist)
{
    double sum = 0;
    for (unsigned int i = 0; i < plist.size(); i++) {
        sum += plist[i].y();
    }
    return sum;
}

pt2idx GetIndexLessOrEqual(const point2dList & plist, double xloc, bool outOfBoundsOnRightReturnNegativeTwo) {
    if (plist.size() <= 0)
        return OUT_OF_BOUNDS_ON_LEFT;
    point2d ploc(xloc,0);
    point2dList::const_iterator itr = std::lower_bound(plist.begin(), plist.end(), ploc, point2d_less_x);
    //if out of bounds on the right
    if (itr == plist.end()) {
        if (outOfBoundsOnRightReturnNegativeTwo)
            return OUT_OF_BOUNDS_ON_RIGHT;
        else
            return static_cast<pt2idx>(plist.size()) - 1;
    }

    //Question: why lower bound is returning itr->x == xloc
    //Answer: http://www.cplusplus.com/reference/algorithm/lower_bound/
    //   "Returns an iterator pointing to the first element in the range [first,last) which does not compare less than val."
    //   This means xloc < val is true since xlock == val
    if (itr->x() == xloc) {
        return itr - plist.begin();
    }

    //if out of bounds on the left
    if (itr == plist.begin()) {
        return OUT_OF_BOUNDS_ON_LEFT;
    }
    return itr - plist.begin() - 1;
}

inline point2dList::const_iterator findGreaterOrEqual(const point2dList &plist, double xloc, pt2idx searchStartIdx) {
    point2dList::const_iterator first;

    if (searchStartIdx < 0 || searchStartIdx >= (int)plist.size()) {
        searchStartIdx = 0;
    }

    first = plist.begin() + searchStartIdx;

    point2dList::const_iterator last = plist.end();
    while (first != last) {
        if (first->x() >= xloc) {
            return first;
        }
        ++first;
    }
    return last;
}

pt2idx GetIndexLessThanValLinear(const point2dList & plist, double xloc, const pt2idx searchStartIdx) {
    if (plist.empty()) {
        return -1;
    }

    point2dList::const_iterator itr = findGreaterOrEqual(plist, xloc, searchStartIdx);

    // if out of bounds on the right
    if (itr == plist.end()) {
        return static_cast<pt2idx>(plist.size()) - 1;
    }

    // if out of bounds on the left
    if (itr == plist.begin() && itr->x() != xloc) {
        return -1;
    }

    // if it is exactly the point on the very left
    if (itr == plist.begin() && itr->x()== xloc) {
        return -1;
    }

    return itr - plist.begin() - 1;
}

static double
get_abs_distance_x(const point2dList & plist, pt2idx idx, double xloc) {
    if (idx < 0 || idx >= int(plist.size()))
        return FLT_MAX;
    double dist = plist[idx].x() - xloc;
    if (dist < 0)
        dist = -dist;
    return dist;
}

pt2idx
findClosestIndex(const point2dList & plist, double xloc) {
    pt2idx idx = GetIndexLessOrEqual(plist, xloc, true);

    //consider out of bounds issue
    if (plist.size() > 0 && idx < 0) {
        if (idx == -1){
            return 0;
        } else {
            return static_cast<pt2idx>(plist.size()) - 1;
        }
    }

    //if out of bounds
    if (idx < 0)
        return -1;
    if (idx >= int(plist.size()))
        return int(plist.size()) - 1;

    pt2idx idx_next = idx+1;
    if (idx_next >= int(plist.size()))
        return idx;

    double dist_now = get_abs_distance_x(plist, idx, xloc);
    double dist_next = get_abs_distance_x(plist, idx_next, xloc);
    if (dist_now < dist_next)
        return idx;
    return idx_next;
}

std::vector<pt2idx>
findClosestIndexList(const point2dList & plist, const std::vector<double> & xlocList) {
    std::vector<pt2idx> ilist;
    for (unsigned int i = 0; i < xlocList.size(); i++) {
        pt2idx idx = findClosestIndex(plist, xlocList[i]);
        ilist.push_back(idx);
    }
    return ilist;
}

double distanceToClosestPoint(const point2dList & plist, double xloc, pt2idx * idx) {
    int index = findClosestIndex(plist, xloc);
    if (index < 0) {
        if (idx) *idx = -1;
        return -1.0;
    }
    double dist = get_abs_distance_x(plist, index, xloc);

    if (idx) *idx = index;
    return dist;
}

pt2idx PlotBase::getIndexLessOrEqual(double xloc, bool outOfBoundsOnRightReturnNegativeTwo) const {
    return GetIndexLessOrEqual(m_pointList, xloc, outOfBoundsOnRightReturnNegativeTwo);
}

pt2idx PlotBase::getIndexLessThanValLinear(double xloc, const pt2idx searchStartIdx) const {
    return GetIndexLessThanValLinear(m_pointList, xloc, searchStartIdx);
}

double
PlotBase::evaluate(double xloc, int interpolate, int use_boundary_value) const
{
    point2d ploc(xloc,0);
    point2d point;
    if (m_pointList.size() <= 0) {
        return 0;
    }

    //check bounds
    point = m_pointList[0];
    if (xloc < point.x()) {
        DEBUG_WARNING_LIMIT(cout << "PlotBase::evaluate xloc < first point.x()=" << xloc << "," << point.x() << endl, 5);
        return use_boundary_value ? point.y() : 0;
    }
    point = m_pointList[m_pointList.size()-1];
    if (point.x() < xloc) {
        DEBUG_WARNING_LIMIT(cout << "PlotBase::evaluate xloc > last point.x()=" << xloc << "," << point.x() << endl, 5);
        return use_boundary_value ? point.y() : 0;
    }

    //find the first instance xloc crossing
    if (m_isSortedAscendingX) {
        pt2idx idx = getIndexLessOrEqual(xloc);
        //if out of bounds on the right
        if (idx < 0) {
            return 0;
        }
        if (idx < int(m_pointList.size())-1) {
            if (interpolate) {
                return interpolate_at(m_pointList[idx], m_pointList[idx+1], xloc);
            } else {
                double aa = diff_double(m_pointList[idx].x(), xloc);
                double bb = diff_double(m_pointList[idx+1].x(), xloc);
                if (aa < bb) {
                    return m_pointList[idx].y();
                }
                else {
                    return m_pointList[idx+1].y();
                }
            }
        } else {
            return m_pointList[idx].y();
        }
    } else {
        for (unsigned int i = 0; i < m_pointList.size(); i++) {
            const point2d & point = m_pointList[i];
            if (xloc <= point.x() ) {  //first crossing of xloc
                if (i == 0) {
                    return point.y();
                }
                if (interpolate) {
                    return interpolate_at(m_pointList[i-1], point, xloc);
                }
                double aa = diff_double(m_pointList[i-1].x(), xloc);
                double bb = diff_double(m_pointList[i].x(), xloc);
                if (aa < bb) {
                    return m_pointList[i-1].y();
                }
                else {
                    return m_pointList[i].y();
                }
            }
        }
    }
    return 0;
}


void PlotBase::evaluate_slow(const std::vector<double> &xs, std::vector<double> * values)
{
    static int interpolate = 1, use_boundary_value = 0;
    values->resize(xs.size());

    int idx = 0;
    for (double x : xs) {
        (*values)[idx] = evaluate(x, interpolate, use_boundary_value);
        idx++;
    }
}

static pt2idx getLastDuplicateXIndex(const point2dList &list, pt2idx point_idx) {
    pt2idx point_after_idx = point_idx + 1;
    if (point_after_idx >= (int)list.size()) {
       return point_idx;
    }

    double x_val = list[point_idx].x();
    double x_val_after = list[point_after_idx].x();

    while (x_val == x_val_after) {
        point_idx++;
        point_after_idx = point_idx + 1;

        if (point_after_idx >= (int)list.size()) {
            break;
        }

        x_val = list[point_idx].x();
        x_val_after = list[point_after_idx].x();
    }

    return point_idx;
}

//First version. Too slow to be useful.
void PlotBase::evaluate_linear_old_v1(const std::vector<double> &sortedXs, std::vector<double> * values) const
{
    pt2idx p_index = 0;
    pt2idx p_index_plus = 0;
    values->reserve(sortedXs.size());

    double left = m_pointList.front().x();
    double right = m_pointList.back().x();
    size_t sortedXSize = sortedXs.size();

    for (unsigned int i = 0; i < sortedXSize; i++) {
        double evaluated_value = 0;

        const double & xval = sortedXs[i];

        if (xval >= left && xval <= right) {
            if (i == 0) {
                p_index = getIndexLessOrEqual(xval);
            } else {
                if( p_index_plus < (int)m_pointList.size()) {
                    p_index = getIndexLessThanValLinear(xval, p_index);
                }
            }

            p_index_plus = p_index + 1;

            if (p_index >= 0 && p_index_plus < (int)m_pointList.size()) {
                p_index = getLastDuplicateXIndex(m_pointList, p_index);
                p_index_plus = getLastDuplicateXIndex(m_pointList, p_index_plus);

                const point2d & leftp = m_pointList[p_index];
                const point2d & rightp = m_pointList[p_index_plus];

                evaluated_value = interpolate_at(leftp, rightp, xval);
            }
            if (p_index < 0) {
                p_index = 0;
            }
        }
        values->push_back(evaluated_value);
    }
}


//Second version, faster than _v1
void PlotBase::evaluate_linear(const std::vector<double> &sortedXs, std::vector<double> * values) const
{
    pt2idx p_index = OUT_OF_BOUNDS_ON_RIGHT;
    pt2idx p_index_right = OUT_OF_BOUNDS_ON_RIGHT;

    if (sortedXs.size() <= 0) {
        warningCoreMini() << "Warning: sortedXs is empty.";
        values->clear();
        return;
    }
    if (m_pointList.size() <= 0) {
        DEBUG_WARNING_LIMIT(warningCoreMini() << "Warning: m_pointList is empty.",5);
        values->clear();
        values->resize(sortedXs.size(), 0);
        return;
    }

    values->resize(sortedXs.size());

    double first = m_pointList.front().x();
    double last = m_pointList.back().x();
    size_t sortedXsSize = sortedXs.size();

    size_t current_xIndex = 0;
    p_index = getIndexLessOrEqual(sortedXs[current_xIndex]);

    // out of bounds on left
    if (p_index == OUT_OF_BOUNDS_ON_LEFT) {
        p_index = 0;
    } // out of bounds on right
    else if (p_index == OUT_OF_BOUNDS_ON_RIGHT) {
        // when x is greater than last point
        while (current_xIndex < sortedXsSize) {
            (*values)[current_xIndex] = 0;
            current_xIndex++;
        }
        return;
    }
    // unspecified error
    else if (p_index < 0) {
        warningCoreMini() << "Error: p_index was an invalid negative value.";
        return;
    }

    p_index_right = p_index + 1;

    // when x is less than point on right
    while (current_xIndex < sortedXsSize && sortedXs[current_xIndex] < first) {
        (*values)[current_xIndex] = 0;
        current_xIndex++;
    }

    // when x is between points
    ptrdiff_t pointListSize = m_pointList.size();
    while(p_index_right < pointListSize && current_xIndex < sortedXsSize && sortedXs[current_xIndex] <= last) {
        if (sortedXs[current_xIndex] > m_pointList[p_index_right].x()) {
            p_index_right++;
            m_pointList[p_index_right].x();
        } else {
            p_index = p_index_right - 1;
            (*values)[current_xIndex] = interpolate_at(m_pointList[p_index], m_pointList[p_index_right], sortedXs[current_xIndex]);
            current_xIndex++;
        }
    }

    // when x is greater than last point
    while (current_xIndex < sortedXsSize) {
        (*values)[current_xIndex] = 0;
        current_xIndex++;
    }
}

void PlotBase::evaluateList(point2dList & plist) const {
    for (unsigned int i = 0; i < plist.size(); i++) {
        double yval = evaluate(plist[i].x());
        plist[i].ry() = yval;
    }
}

#ifdef VALIDATE_BINARY_SEARCH_AREA
static double
computeAreaTrapezoid(const std::vector<point2d> & points)
{
    if (points.size() <= 0)
        return 0;
    double sum = 0;
    double area = 0;
    int i, size = int(points.size()) - 1;
    for (i = 0; i < size; i++) {
        area = computeAreaTrapezoid(points[i], points[i+1]);
        sum += area;
    }
    return sum;
}
#endif

double
PlotBase::computeAreaNoUnitConversion(double t_start, double t_end, bool useBinarySearch, ComputeAreaMethod method) const {
    double aa;
    aa = computeAreaFast(t_start, t_end, method);

#ifdef VALIDATE_BINARY_SEARCH_AREA
        debugCoreMini() << ".";
        vector<point2d> points = getAreaPointList(t_start, t_end, useBinarySearch);
        double bb = pmi::computeAreaTrapezoid(points);
        if (!is_close(aa,bb,1e-39)) {
            debugCoreMini() << "plotbase area different: " << aa << "," << bb << aa-bb;
        }
#else
    Q_UNUSED(useBinarySearch);
#endif

    return aa;
}

double
PlotBase::getYSum(double t_start, double t_end, bool inclusive) const {
    return getYSumBetween(m_pointList, t_start, t_end, inclusive);
}

double
PlotBase::computeAreaFast(double t_start, double t_end, ComputeAreaMethod method) const {
    double totalArea = 0;
    point2d start, end;

    if (m_pointList.size() <= 0 || t_start == t_end)
        return totalArea;

    //force t_start <= t_end
    if (t_end < t_start) {
        std::swap(t_end,t_start);
    }
    if (!m_isSortedAscendingX) {
        debugCoreMini() << "warning, the point list was not sorted (or not guaranteed to be sorted); now sorting.";
        PlotBase * ptr = (PlotBase*) this;  //avoid compile error; poor programming, but will work until we fully remove non-sorted (and slow) area computation.
        ptr->sortPointListByX();
    }

    //All possible intersections
    //< > (      )
    //<   (  >   )
    //    ( <  > )
    //    (  <   )  >
    //    (      )  <   >
    //<   (      )  >

    //check to seee if the interval has any intersections; return zero if no intersection.
    //<  > (    )
    //     (    ) <   >
    if (t_start >= m_pointList.back().x()) { //If start is to the right of data, return zero.
        return totalArea;
    } else if (t_end <= m_pointList.front().x()) { //If end is to the left of data, return zero.
        return totalArea;
    }

    //Now we have four intersection cases:
    //<  (  >  )
    //   ( < > )
    //   (  <  )  >
    //<  (     )  >
    //

    //For the above cases, we are going to introduce start point and end point
    //These will either clamp to the either boundary or get evalulated inside the intersection point.
    if (t_start < m_pointList.front().x()) {
        start = m_pointList.front();
    }
    else {
        start.rx() = t_start;
        start.ry() = evaluate(t_start);
    }
    if (t_end > m_pointList.back().x()) {
        end = m_pointList.back();
    } else {
        end.rx() = t_end;
        end.ry() = evaluate(t_end);
    }

    pt2idx idx = getIndexLessOrEqual(t_start);
    const int start_idx = std::max(0, idx);
    int first_index = -1;
    int last_index = -1;
    //Find the first and last index within the given start and end positions
    for (int i = start_idx; i < int(m_pointList.size()); i++) {
        const point2d & point = m_pointList[i];
        if (start.x() < point.x() && point.x() < end.x() ) {
            if (first_index == -1) {
                first_index = i;
            }
            last_index = i;
        } else if (point.x() > end.x())
            break;
    }

    //If no bars between the two, then these are points very close together, in-between a single interval.
    if (first_index == -1) {
        totalArea = computeAreaTrapezoid(start,end,method);
    }
    else {
        for (int i = first_index; i < last_index; i++) {
            totalArea += computeAreaTrapezoid(m_pointList[i], m_pointList[i + 1], method);
        }
        totalArea += computeAreaTrapezoid(start, m_pointList[first_index], method);
        totalArea += computeAreaTrapezoid(m_pointList[last_index], end, method);
    }

    if (method == ComputeAreaMethod_KeepNegativeAreas_ButClampToZeroAfterwards) {
        if (totalArea < 0) {
            totalArea = 0;
        }
    }

    return totalArea;
}

double
PlotBase::computeAreaSecondsUnit(double t_start, double t_end, ComputeAreaMethod method) const {
    return computeAreaNoUnitConversion(t_start, t_end, true, method)*60;
}

bool PlotBase::getMinMaxPoints_slow(double t_start, double t_end, point2d * xminPoint, point2d * xmaxPoint) const {
    bool valid = getMinMaxPoints(t_start, t_end, xminPoint, xmaxPoint);
    return valid;
}

bool PlotBase::getMinMaxPointsIndex(pt2idx i_start, pt2idx i_end, point2d & minPoint, point2d & maxPoint) const {
    //double max_time = t_start, max_inten = 0;
    if (i_start < 0 || i_end < i_start || i_end >= int(m_pointList.size())) {
        cerr << "Improper index in getMinMaxPointsIndex(), i_start " << i_start << " i_end " << i_end << endl;
        return false;
    }

    maxPoint = m_pointList[i_start];
    minPoint = m_pointList[i_start];

    for (int i = i_start+1; i <= i_end; i++) {
        if (m_pointList[i].y() > maxPoint.y()) {
            maxPoint = m_pointList[i];
        }
        if (m_pointList[i].y() < minPoint.y()) {
            minPoint = m_pointList[i];
        }
    }
    return true;
}

//TODO(2015-01-25): This is for choosing apex for XIC maximum.
//If no maximum exists, what to do?  The current implementation
//maybe choose one that's not near the boundary.  Consider choosing the middle in such
//situation?  Write a new function, or pass in option.
point2d
PlotBase::getMaxTimeIntensity(double t_start, double t_end) const {
    point2d minPoint, maxPoint;
    bool valid = getMinMaxPoints(t_start, t_end, &minPoint, &maxPoint);
    return maxPoint;
}

point2d
PlotBase::getMinTimeIntensity(double t_start, double t_end) const {
    point2d minPoint, maxPoint;
    bool valid = getMinMaxPoints(t_start, t_end, &minPoint, &maxPoint);
    return minPoint;
}

point2d
getMaxPoint(const point2dList & pointList) {
    point2d p(0, 0);
    for (unsigned int i = 0; i < pointList.size(); i++) {
        if (i == 0) {
            p = pointList[i];
        }
        if (pointList[i].y() > p.y()) {
            p = pointList[i];
        }
    }
    return p;
}

point2d
getMinPoint(const point2dList & pointList) {
    point2d p(0, 0);
    for (unsigned int i = 0; i < pointList.size(); i++) {
        if (i == 0) {
            p = pointList[i];
        }
        if (pointList[i].y() < p.y()) {
            p = pointList[i];
        }
    }
    return p;
}

point2d
PlotBase::getMaxPoint() const {
    return pmi::getMaxPoint(m_pointList);
}

point2d
PlotBase::getMinPoint() const {
    return pmi::getMinPoint(m_pointList);
}

int
PlotBase::getMaxTimeIntensityIndex(const std::vector<double> & timeList) const {
    double maxValue = -1;
    int maxIndex = -1;
    for (unsigned int i = 0; i < timeList.size(); i++) {
        double val = evaluate(timeList[i], 0);
        if (val > maxValue) {
            maxValue = val;
            maxIndex = i;
        }
    }
    return maxIndex;
}

void SortPointListByX(point2dList & plist, bool doAscending)
{
    if (doAscending) {
        sort(plist.begin(), plist.end(), point2d_less_x);
    } else {
        sort(plist.rbegin(), plist.rend(), point2d_less_x);
    }
}

void PlotBase::sortPointListByX(bool doAscending) {
    SortPointListByX(m_pointList, doAscending);
    m_isSortedAscendingX = doAscending;
}

void PlotBase::sortPointListByY(bool doAscending) {
    if (doAscending) {
        sort(m_pointList.begin(), m_pointList.end(), point2d_less_y);
    } else {
        sort(m_pointList.rbegin(), m_pointList.rend(), point2d_less_y);
    }
    m_isSortedAscendingX = false;
}

void PlotBase::getXBound(double * t1, double *t2) const {
    *t1 = *t2 = 0;
    if (m_pointList.size() > 0) {
        *t1 = *t2 = m_pointList[0].x();
    }
    for (unsigned int i = 1; i < m_pointList.size(); i++) {
        double x = m_pointList[i].x();
        if (x < *t1)
            *t1 = x;
        if (x > *t2)
            *t2 = x;
    }
}

void PlotBase::getMinMaxIndexList(std::vector<pt2idx> & idxList, int start_index, int end_index) const {
    idxList.clear();
    std::vector<pt2idx> minList, maxList;
    //Let's call the min/max functions directly that to make sure the boundaries (first & last) are considered properly.
    //Merged result should be such that min/max values are alternating, always.
    getMinIndexList(minList, start_index, end_index);
    getMaxIndexList(maxList, start_index, end_index);
    idxList.resize(minList.size()+maxList.size());
    std::merge(minList.begin(), minList.end(), maxList.begin(), maxList.end(), idxList.begin());
}

void PlotBase::getMaxIndexList(std::vector<pt2idx> & idxList, int start_index, int end_index) const {
    idxList.clear();
    double a, b, c;
    const pt2idx pointListSize = static_cast<pt2idx>(m_pointList.size());

    if (start_index < 0 || end_index < 0) {
        start_index = 0;
        end_index = pointListSize - 1;
    }

    if (start_index == 0 && m_pointList.size() > 1) { //first point, requires two points for extrema.
        b = m_pointList[0].y();
        c = m_pointList[1].y();
        if (b > c) {
            idxList.push_back(0);
        }
    }
    for (pt2idx i = start_index + 1; i < end_index; i++) {
        a = m_pointList[i-1].y();
        b = m_pointList[i].y();
        c = m_pointList[i+1].y();
        if (a < b && b > c) {
            idxList.push_back(i);
        } else if (a == b && b > c) {
            //This is a maxima or just a slant? E.g.
            //   ___       \___
            //  /   \  or      \
            //Find the previous non-equavalent value to determine extrema
            for (pt2idx j = i - 2; j >= 0; j--) {
                a = m_pointList[j].y();
                if (a < b) {
                    idxList.push_back(i);
                    break;
                } else if (a > b) {
                    break;
                }
            }
        }
    }
    // last point, requires two points for extrema.
    if (m_pointList.size() > 1
        && (end_index == pointListSize - 1)) {
        a = m_pointList[m_pointList.size() - 2].y();
        b = m_pointList[m_pointList.size() - 1].y();
        if (b > a) {
            idxList.push_back(pointListSize - 1);
        } else if (b == a) {
            // Find the previous non-equavalent value to determine extrema
            for (int j = pointListSize - 3; j >= 0; j--) {
                a = m_pointList[j].y();
                if (a < b) {
                    idxList.push_back(pointListSize - 1);
                    break;
                } else if (a > b) {
                    break;
                }
            }
        }
    }
}

double PlotBase::computeSumIntensities(double startMz, double endMz) const {

    pt2idx start_index = GetIndexLessOrEqual(m_pointList, startMz, false);
    pt2idx end_index = GetIndexLessOrEqual(m_pointList, endMz, false);

    start_index = start_index < 0 ? 0 : start_index;
    end_index = end_index < 0 ? 0 : end_index;

    double sum_points_y = 0;
    for (pt2idx pl_index = start_index; pl_index <= end_index; pl_index++) {
        const point2d point = m_pointList[pl_index];
        double point_y = point.y();
        sum_points_y += point_y;
    }
    return sum_points_y;
}

void PlotBase::getMinIndexList(std::vector<pt2idx> &idxList, int start_index, int end_index) const
{
    idxList.clear();
    double a, b, c;
    const pt2idx pointListSize = static_cast<pt2idx>(m_pointList.size());

    if (start_index < 0 || end_index < 0) {
        start_index = 0;
        end_index = pointListSize - 1;
    }

    if (start_index == 0 && m_pointList.size() > 1) { //first point, requires two points for extrema.
        b = m_pointList[0].y();
        c = m_pointList[1].y();
        if (b < c) {
            idxList.push_back(0);
        }
    }
    for (int i = start_index + 1; i < end_index; i++) {
        a = m_pointList[i - 1].y();
        b = m_pointList[i].y();
        c = m_pointList[i + 1].y();
        if (a > b && b < c) {
            idxList.push_back(i);
        } else if (a == b && b < c) {
            // This is a minima or just a slant? E.g.
            //  \___/      \___
            //         or      \
            //find the previous non-equavalent value to determine extrema
            for (int j = i - 2; j >= 0; j--) {
                a = m_pointList[j].y();
                if (a > b) {
                    idxList.push_back(i);
                    break;
                } else if (a < b) {
                    break;
                }
            }
        }
    }
    // last point, requires two points for extrema.
    if (m_pointList.size() > 1 && (end_index == pointListSize - 1)) {
        a = m_pointList[m_pointList.size() - 2].y();
        b = m_pointList[m_pointList.size() - 1].y();
        if (a > b) {
            idxList.push_back(pointListSize - 1);
        } else if (a == b) {
            // find the previous non-equavalent value to determine extrema
            for (int j = pointListSize - 3; j >= 0; j--) {
                a = m_pointList[j].y();
                if (a > b) {
                    idxList.push_back(pointListSize - 1);
                    break;
                } else if (a < b) {
                    break;
                }
            }
        }
    }
}

Err PlotBase::makePointsFromIndex(const point2dIdxList & idxList, point2dList & points) const {
    Err e = kNoErr;

    points.clear();
    for (size_t i = 0; i < idxList.size(); i++) {
        const pt2idx index = idxList[i];
        if (index < 0 || index >= static_cast<pt2idx>(m_pointList.size())) {
            e = kBadParameterError; eee;
        }
        points.push_back(m_pointList[index]);
    }

error:
    return e;
}



/*
Copyright (c) 2009, Nathanael C. Yoder
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the distribution

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.*/

//http://www.mathworks.com/matlabcentral/fileexchange/25500-peakfinder
//Find peaks that are larger than other peaks by tol amount; over the whole domain.
Err PlotBase::findPeaksIndex(double sel, FindPeakOption findMax, point2dIdxList & idxList, int start_index, int end_index) const {
    Err e = kNoErr;
    point2dIdxList ilist;
    point2dList plist;

    pt2idx tempLoc = -1;
    double tempMag,leftMin,minMag;
    bool foundPeak;

    //loop through critical points
    getMinMaxIndexList(ilist, start_index, end_index);
    e = makePointsFromIndex(ilist,plist); eee;

    //the algorithm always looks for max. if search for min, invert values
    if (findMax == FindMin) { //find min
        scale_height(plist, -1);
    }

    minMag = point2dMin(plist);

    tempMag = minMag;
    foundPeak = false;
    leftMin = minMag;

    for (int i = 0; i < int(ilist.size())-1; i++) {
        const point2d & p = plist[i];
        // Reset peak finding if we had a peak and the next peak is bigger
        //   than the last or the left min was small enough to reset.
        if (foundPeak) {
            tempMag = minMag;
            foundPeak = false;
        }
        //Found new peak that was lager than temp mag and selectivity larger
        //   than the minimum to its left.
        if (p.y() > tempMag && p.y() > (leftMin + sel)) {
            tempLoc = ilist[i];
            tempMag = p.y();
            continue;
        }

        //Move onto the valley
        //Come down at least sel from peak
        if (!foundPeak && tempMag > sel + p.y()) {
            foundPeak = true; // We have found a peak
            leftMin = p.y();
            idxList.push_back(tempLoc);
        }
        else if (p.y() < leftMin) { //New left minima
            leftMin = p.y();
        }
    }

    // Check end point
    if (ilist.size() > 1) {
        const size_t i = ilist.size()-1;
        const point2d & p = plist[i];
        if (p.y() > tempMag && p.y() > (leftMin + sel)) {
            idxList.push_back(ilist[i]);
        }
        else if (!foundPeak && tempMag > minMag) { //Check if we still need to add the last point
            idxList.push_back(tempLoc);
        }
    }

error:
    return e;
}

Err
PlotBase::findPeaksRelativeTolerance(FindPeakOption findMax, point2dIdxList & apexIdxList, double relativeTolerance, int kth_max) const {
    Err e = kNoErr;

    double ymax = this->getNthCriticalMaximaValue(kth_max);
    double peak_tol_max = ymax * relativeTolerance;
    e = findPeaksIndex(peak_tol_max, findMax, apexIdxList); eee;

error:
    return e;
}

/*
Err PlotBase::makePeaks(const ConstructPeaksOptions & options, point2dIdxList & startIdxList, point2dIdxList & endIdxList, point2dIdxList & apexIdxList, QString * tempFolderPtr) const {
    Err e = kNoErr;

    //use maximum value to set tolerances; need better algorithm with useful user input values.
    double ymax = this->getNthCriticalMaximaValue(options.maxCriticalPointsKth);
    double peak_tol_max = ymax * options.peakToleranceScaleForMax;
    double peak_tol_min = ymax * options.peakToleranceScaleForMin;

    debugCoreMini() << "ymax " << ymax;
    debugCoreMini() << "tolmax" << options.peakToleranceScaleForMax;
    debugCoreMini() << "tolmin" << options.peakToleranceScaleForMin;

    point2dIdxList maxIdxList, minIdxList;

    startIdxList.clear();
    endIdxList.clear();
    apexIdxList.clear();
    if (m_pointList.size() <= 0)
        return e;

    //get critical max/min points
    e = findPeaksIndex(peak_tol_max, kFindMax, maxIdxList); eee;
    e = findPeaksIndex(peak_tol_min, kFindMin, minIdxList); eee;

    if (1) {
        static int count = 0;
        count++;
        point2dList minPList, maxPList; //for debugging
        this->makePointsFromIndex(maxIdxList, maxPList);
        this->makePointsFromIndex(minIdxList, minPList);
        QString tmpFolder;
        if (tempFolderPtr)
            tmpFolder = *tempFolderPtr;
        QString allPointsFilename = QString("all_points%1.csv").arg(count);
        QString maxPointsFilename = QString("max_points%1_tol_%2.csv").arg(count).arg(peak_tol_max, 0, 'f', 4);
        QString minPointsFilename = QString("min_points%1_tol_%2.csv").arg(count).arg(peak_tol_min, 0, 'f', 4);

        pmi::saveDataToFile(joinPaths(tmpFolder,maxPointsFilename), maxPList);
        pmi::saveDataToFile(joinPaths(tmpFolder,minPointsFilename), minPList);
        pmi::saveDataToFile(joinPaths(tmpFolder,allPointsFilename), m_pointList);
    }


    if (1) {
        point2dIdxList maxIdxList, minIdxList;
        point2dList minPList, maxPList; //for debugging
        QString tmpFolder;
        if (tempFolderPtr)
            tmpFolder = *tempFolderPtr;
        double tol = 1;
        for (int i = 0; i < 5; i++) {
            tol *= 0.1;
            e = findPeaksIndex(tol, kFindMax, maxIdxList); eee;
            e = findPeaksIndex(tol, kFindMin, minIdxList); eee;

            this->makePointsFromIndex(maxIdxList, maxPList);
            this->makePointsFromIndex(minIdxList, minPList);
            QString maxPointsFilename = QString("max_points_tol_%2.csv").arg(tol, 0, 'f', 6);
            QString minPointsFilename = QString("min_points_tol_%2.csv").arg(tol, 0, 'f', 6);

            pmi::saveDataToFile(joinPaths(tmpFolder,maxPointsFilename), maxPList);
            pmi::saveDataToFile(joinPaths(tmpFolder,minPointsFilename), minPList);
        }
    }

    //for each max point, find the most adjecent min point and construct the peak_interval
    for (unsigned int i = 0; i < maxIdxList.size(); i++) {
        pt2idx max_idx = maxIdxList[i];
        //double idx_now_time = m_pointList[max_idx].x();

        point2dIdxList::const_iterator itr_start = lower_bound(minIdxList.begin(), minIdxList.end(), max_idx);
        point2dIdxList::const_iterator itr_end = upper_bound(minIdxList.begin(), minIdxList.end(), max_idx);
        if (itr_start != minIdxList.end() && itr_end != minIdxList.end() && itr_start != minIdxList.begin()) {
            pt2idx idx_start = *(--itr_start);
            pt2idx idx_end = *itr_end;
            //Check to see if idx_start & idx_end already exists
            //We want to do this because many max peaks may have the same minimal peaks / interval
            if (startIdxList.size() > 0 && startIdxList.back() == idx_start && endIdxList.back() == idx_end) {
                //If the start & end interval already exists, consider changing the max index for this interval.
                if (m_pointList[apexIdxList.back()].y() < m_pointList[max_idx].y())
                    apexIdxList.back() = max_idx;
            } else {
                //This sure the peaks and created end-to-end; no gaps between peaks.
                if (endIdxList.size() > 0) {
                    idx_start = endIdxList.back();
                }
                //Create the peak
                startIdxList.push_back(idx_start);
                endIdxList.push_back(idx_end);
                apexIdxList.push_back(max_idx);
            }
        }
    }

error:
    return e;
}
*/

Err PlotBase::getDataFromFile(const QString & filename, bool doSort, bool outputMessage) {
    Err e = kNoErr;

    QFile file(filename);
    if(!file.open(QIODevice::ReadOnly)) {
        e = kFileOpenError; ree;
        //QMessageBox::information(0, "error", file.errorString());
    }

    m_pointList.clear();

    QTextStream in(&file);

    int line_count = 0;
    point2d obj;
    while(!in.atEnd()) {
        line_count++;
        QString line;
        //The in.readLine() doesn't work if the file only contains "\r", for Windows-only text format (some old machines still write this way I guess)
        //We will emulate readLine() by reading one character at a time.
        while(!in.atEnd()) {
            QString oneStr = in.read(1);
            if (oneStr.endsWith("\n") || oneStr.endsWith("\r")) {
                line = line.trimmed();
                break;
            }
            line += oneStr;
        }
        //QString line = in.readLine().trimmed();
        QStringList fields;
        if (line.contains(",")) {
            fields = line.split(",", QString::SkipEmptyParts);
        } else if (line.contains("\t")){
            fields = line.split("\t", QString::SkipEmptyParts);
        } else if (line.contains(" ")) {
            fields = line.split(" ", QString::SkipEmptyParts);
        }
        if (fields.size() == 2) {
            bool ok=false, ok2=false;
            obj.rx() = fields[0].toDouble(&ok);
            obj.ry() = fields[1].toDouble(&ok2);
            if (ok && ok2)
                m_pointList.push_back(obj);
            else {
                if (line_count > 1 && outputMessage)  //First line might be header info; we can ignore that line.
                {
                    DEBUG_WARNING_LIMIT(cerr << "error reading at line " << line_count << ". Content=" << line.toStdString() << " file:" << filename.toStdString() << endl, 20);
                }
            }
        } else {
            if (line_count > 1 && outputMessage) {
                DEBUG_WARNING_LIMIT( cerr << "error reading at line " << line_count << ". Content=" << line.toStdString() << " file:" << filename.toStdString() << endl, 20);
            }
        }
    }
    if (outputMessage) {
        cout << "Read complete. Plot data length =" << m_pointList.size() << endl;
    }

    if (doSort) {
        sortPointListByX();
        m_isSortedAscendingX = true;
    } else {
        m_isSortedAscendingX = isSortedAscendingX();
    }

//error:
    if (file.isOpen()) file.close();
    return e;
}

Err saveDataToFile(const QString & filename, const point2dList & plist) {
    Err e = kNoErr;
    QFile file(filename);
    if(!file.open(QIODevice::WriteOnly)) {
        e = kFileOpenError; ree;
    }

    QTextStream out(&file);
    out.setRealNumberPrecision(15);
    for (unsigned int i = 0; i < plist.size(); i++) {
        out << plist[i].x() << "," << plist[i].y() << endl;
    }
//error:
    return e;
}

Err PlotBase::saveDataToFile(const QString & filename) const {
    return pmi::saveDataToFile(filename, m_pointList);
}

bool isSortedAscendingX(const point2dList & list) {
    for (unsigned int i = 1; i < list.size(); i++) {
        if (list[i].x() < list[i-1].x())
            return false;
    }
    return true;
}

bool PlotBase::isSortedAscendingX() const {
    return pmi::isSortedAscendingX(m_pointList);
}

double
PlotBase::getNthCriticalMinimaValue(int nth) const {
    point2dIdxList idxList;
    point2dList pList;
    getMinIndexList(idxList);
    if (idxList.size() <= 0) {
        return 0;  //should throw error or invalid QVariant.
    }

    makePointsFromIndex(idxList, pList);
    sort(pList.begin(), pList.end(), point2d_less_y);
    nth = std::min(nth, static_cast<int>(pList.size()) - 1);
    return pList[nth].y();
}

double
PlotBase::getNthCriticalMaximaValue(int nth) const {
    point2dIdxList idxList;
    point2dList pList;
    getMaxIndexList(idxList);
    if (idxList.size() <= 0) {
        return 0;  //should throw error or invalid QVariant.
    }

    makePointsFromIndex(idxList, pList);
    sort(pList.rbegin(), pList.rend(), point2d_less_y);
    nth = std::min(nth, static_cast<int>(pList.size()) - 1);
    return pList[nth].y();
}

Err PlotBase::makeResampledPlot(double samplingInterval, PlotBase & outPlot) const {
    if (samplingInterval <= 0)
        return kBadParameterError;

    point2d p;
    double t1,t2,xwidth;
    outPlot.clear();
    getXBound(&t1, &t2);
    xwidth = t2-t1;
    int samplesNum = xwidth / samplingInterval + 1;
    for (int i = 0; i < samplesNum; i++) {
        p.rx() = t1 + samplingInterval * i;
        p.ry() = evaluate(p.x(), 1);
        outPlot.addPoint(p);
    }

    return kNoErr;
}


Err PlotBase::makeResampledPlotTargetSize(int target_number_of_points, PlotBase & outPlot) const {
    Err e = kNoErr;
    if (this->getPointListSize() < target_number_of_points) {
        outPlot = *this;
        return kNoErr;
    }

    double t1, t2, xwidth;
    getXBound(&t1, &t2);
    xwidth = t2 - t1;
    double samplingInterval = xwidth / target_number_of_points;
    e = makeResampledPlot(samplingInterval, outPlot); eee;

error:
    return e;
}

Err PlotBase::averageSampleWidth(double * averageWidth) {
    Err e = kNoErr;
    double t1,t2;
    double domainWidth = 0;
    *averageWidth=0;
    if (m_pointList.size() <= 1) {
        e = kBadParameterError; eee;
    }
    getXBound(&t1, &t2);
    domainWidth = t2-t1;
    if (domainWidth < 0) {
        e = kBadParameterError; eee;
    }

    *averageWidth = domainWidth/(m_pointList.size()-1);

error:
    return e;
}

Err PlotBase::makeResampledPlotMaxPoints(int maxNumberOfPoints, point2dList & outPoints) const {
    Err e = kNoErr;
    point2d p;
    double t1,t2,xwidth,samplingInterval;

    outPoints.clear();
    //let's make sure we have two points
    if (maxNumberOfPoints <= 1) {
        e = kBadParameterError; eee;
    }

    getXBound(&t1, &t2);
    xwidth = t2-t1;
    if (xwidth < 0) {
        e = kBadParameterError; eee;
    }
    samplingInterval = xwidth/(maxNumberOfPoints-1);
    for (int i = 0; i < maxNumberOfPoints; i++) {
        p.rx() = t1 + samplingInterval * i;
        p.ry() = evaluate(p.x(), 1);
        outPoints.push_back(p);
    }

error:
    return e;
}

/*! \brief Checks that a and b are almost equal (within tolerance). Tolerance is relative. */
static inline bool fuzzyCompare(double a, double b, double relativeTolerance)
{
    // This is a very small number in terms of minutes (used here)
    const double verySmallNumber = 0.00001;
    if (fsame(a, b, verySmallNumber)) {
        return true;
    }

    double diff = std::abs(b - a);    
    double maximumAbs = std::max(std::abs(a), std::abs(b));

    return (diff / maximumAbs) <= relativeTolerance;
}

/*! \brief Recursively splits in two and checks if the two segments are within tolerance. */
static bool isGloballyUniform(const PlotBase &plot, int startIndex, int endIndex,
                              double negligibleRatio)
{
    int size = endIndex - startIndex;
    if (size < 4) {
        return true;
    }

    // split to two equal segments
    int halfSize = size / 2;
    int segmentAStart = startIndex;
    int segmentAEnd = segmentAStart + halfSize;
    int segmentBStart = segmentAEnd;
    int segmentBEnd = segmentBStart + halfSize;

    // calculate duration of first and second segment and make sure they are almost equal
    double segmentADuration
        = plot.getPointList()[segmentAEnd - 1].x() - plot.getPointList()[segmentAStart].x();
    double segmentBDuration
        = plot.getPointList()[segmentBEnd - 1].x() - plot.getPointList()[segmentBStart].x();
    if (!fuzzyCompare(segmentADuration, segmentBDuration, negligibleRatio / halfSize)) {
        return false;
    }

    // recurse
    if (!isGloballyUniform(plot, segmentAStart, segmentAEnd, negligibleRatio)) {
        return false;
    }
    if (!isGloballyUniform(plot, segmentBStart, segmentBEnd, negligibleRatio)) {
        return false;
    }

    return true;
}

bool PlotBase::isUniform(double negligibleRatio) const
{
    if (getPointListSize() < 3) {
        return true;
    }

    // Check small errors don't stack up.
    if (!isGloballyUniform(*this, 0, getPointListSize(), negligibleRatio)) {
        return false;
    }

    // Make sure that all durations between samples are within tolerance
    double minimumDuration = m_pointList[1].x() - m_pointList[0].x();
    double maximumDuration = minimumDuration;
    for (int i = 1; i < getPointListSize(); i++) {
        double duration = m_pointList[i].x() - m_pointList[i - 1].x();
        minimumDuration = std::min(minimumDuration, duration);
        maximumDuration = std::max(maximumDuration, duration);
    }
    return fuzzyCompare(minimumDuration, maximumDuration, negligibleRatio);
}

//Use very small fudge value since we are using double
static const double internval_fudge = 1e-200;

//This function assumes orderd point list
void PlotBase::findIntersectionPoints(const line2d & xquery_line,
                                      point2dList & intersectPointList, point2dIdxList & intersectStartIdxList,
                                      long flags, std::vector<long> * intersectFlagList) const {
    intersectPointList.clear();
    intersectStartIdxList.clear();
    if (intersectFlagList) intersectFlagList->clear();

    const pt2idx pointListSize = static_cast<pt2idx>(m_pointList.size());

    if (pointListSize <= 0) {
        return;
    }

    //enforce a.x < b.x
    line2d query_line = xquery_line;
    if (xquery_line.a.x() > xquery_line.b.x()) {
        query_line.a = xquery_line.b;
        query_line.b = xquery_line.a;
    }

    //Find optimal start and end index positions
    pt2idx start_idx = getIndexLessOrEqual(query_line.a.x());
    pt2idx end_idx = getIndexLessOrEqual(query_line.b.x()) + 1;

    if (start_idx < 0) {
        //left side not found because:
        //1)on left of content
        if (query_line.a.x() < m_pointList.front().x()) {
            start_idx = 0;
        }
        //2)on right of content
        else if (query_line.a.x() > m_pointList.back().x()) {
            start_idx = pointListSize - 1;
        }
        else {
            debugCoreMini() << "start_idx not found for query_line: " << query_line.a << "," << query_line.b;
            start_idx = 0;
        }
    }

    if (end_idx < 0) {
        //right side not found because:
        //1) on left of content
        if (query_line.b.x() < m_pointList.front().x()) {
            end_idx = 0;
        }
        //2) on right of content
        else if (query_line.b.x() > m_pointList.back().x()) {
            end_idx = pointListSize - 1;
        } else {
            debugCoreMini() << "end_idx not found for query_line: " << query_line.b << "," << query_line.b;
            end_idx = pointListSize - 1;
        }
    }

    point2d intersect_p;

    //Note: end_idx is used below as open boundary (not i<=end_idx); and so needs to be one less than length of array.
    //Enforce the boundary here.
    if (end_idx >= int(m_pointList.size())) {
        //debugCoreMini() << "warning, end_idx " << end_idx  << " was out of bounds; but now fixed to " << m_pointList.size()-1;
        end_idx = pointListSize - 1;
    }

    QLineF queryLine(query_line.a, query_line.b);

    for (pt2idx i = start_idx; i < end_idx; i++) {
        QLineF line2(m_pointList[i], m_pointList[i+1]);
        if (line2 == queryLine) {  //If exactly same
            if (flags & kIntersectionFlagEndPoints) {
                intersectPointList.push_back(line2.p1());
                intersectStartIdxList.push_back(i);
                if (intersectFlagList) intersectFlagList->push_back(kIntersectionFlagEndPoints);

                if (i == (end_idx-1)) {
                    intersectPointList.push_back(line2.p2());
                    intersectStartIdxList.push_back(i);
                    if (intersectFlagList) intersectFlagList->push_back(kIntersectionFlagEndPoints);
                }
            }
            continue;
        }
        QLineF::IntersectType it_type = queryLine.intersect(line2, &intersect_p);

        //TODO: If parallel and intersects, do what?

        if (it_type == QLineF::BoundedIntersection) {
            //if (is_between(line2.a.x(), line2.b.x(), intersect_p.x(), internval_fudge)) {
            if (is_between(query_line.a.x(), query_line.b.x(), intersect_p.x(), 0)) {
                if (flags & kIntersectionFlagMidPoints) {
                    intersectPointList.push_back(intersect_p);
                    intersectStartIdxList.push_back(i);
                    if (intersectFlagList) intersectFlagList->push_back(kIntersectionFlagMidPoints);
                }
            } else {
                if (flags & kIntersectionFlagEndPoints) {
                    if (query_line.matchesEndPoint(line2.p1())) {
                        debugCoreMini() << "warning, queryLine.intersect touches and end point, but numerically not caught; now adding to intersect list." << endl;
                        intersectPointList.push_back(line2.p1());
                        intersectStartIdxList.push_back(i);
                        if (intersectFlagList) intersectFlagList->push_back(kIntersectionFlagEndPoints);
                    }
                    //If last index, also check the last point
                    if (i == (end_idx-1)) {
                        if (query_line.matchesEndPoint(line2.p2())) {
                            //cerr << "warning, query_line touches and end point, but numerically not caught; now adding to intersect list." << endl;
                            intersectPointList.push_back(line2.p2());
                            intersectStartIdxList.push_back(i+1);
                            if (intersectFlagList) intersectFlagList->push_back(kIntersectionFlagEndPoints);
                        }
                    }
                }
            }
        } else {
            //If parallel,
            //mid point is needed for area computation and
            //end point is needed for baseline pruning
            if ((flags & kIntersectionFlagMidPoints) || (flags & kIntersectionFlagEndPoints)) {
                QVector2D v1(line2.p1() - query_line.a);
                QVector2D v2(query_line.b - query_line.a);
                v1.normalize();
                v2.normalize();

                const double dotValue = MathUtils::dotProduct(v1,v2);

                if (is_close(dotValue,1,internval_fudge)) {
                    //if line2 ontop of query_line, add both points
                    intersectPointList.push_back(line2.p1());
                    intersectStartIdxList.push_back(i);
                    if (intersectFlagList) intersectFlagList->push_back(kIntersectionFlagMidPoints|kIntersectionFlagEndPoints);
                    if (i == (end_idx-1)) {
                        intersectPointList.push_back(line2.p2());
                        intersectStartIdxList.push_back(i+1);
                        if (intersectFlagList) intersectFlagList->push_back(kIntersectionFlagMidPoints|kIntersectionFlagEndPoints);
                    }
                }
            }
        }
    }
}

/*
bool PlotBase::intersectsStick(direction dir, const point2d & shift_limit,
                const QRectF & rectBeforeMove,
                QRectF & rectAfterMove, pt2idx * index, bool *continueChecking, bool avoid_hor_cross)
{

    bool avoidedCurrItem = true;    /// If current item can be avoided successfully.
    //bool continueChecking ;  /// if need to continue checking against rest items/


    while(*index < m_pointList.size()){
        //debugCoreMini() << ".............................................";
        //debugCoreMini() << "index: " << *index;
        //debugCoreMini() << "dir = " << dir;
        point2d avoidItem = m_pointList.at(*index);

        avoidedCurrItem = intersectsStick(dir, shift_limit,avoidItem,rectBeforeMove,rectAfterMove, continueChecking, avoid_hor_cross);

        //debugCoreMini() << "continueChecking " << continueChecking;
        if (!*continueChecking){  /// if we fail to avoid current item, then we know we cannot expect to find non-colliding position
            break;
        }
        (*index)++;
    }
    return avoidedCurrItem;
}


bool PlotBase::intersectsStick(direction dir, const point2d & shift_limit,
                const point2d avoidItem, const QRectF& itemRectOrigin, QRectF& itemRectMoved, bool *continueChecking, bool avoid_hor_cross) const
{

    *continueChecking = true;

    bool avoidedCollision = true;
    int pixelMargin = 3;



    //debugCoreMini() << "Current rect" << itemRectMoved;
    //debugCoreMini() << "datapoint " << avoidItem;
    if( itemRectMoved.bottom() > avoidItem.y()
            && itemRectMoved.left() < avoidItem.x()
            && itemRectMoved.right() > avoidItem.x()
            )
    {
        avoidedCollision = false;

        double mid = (itemRectOrigin.left() + itemRectOrigin.right())/2;
        switch(dir){
        case move_Left:
        {
            if( (itemRectOrigin.right() - (avoidItem.x() - pixelMargin) < shift_limit.x()) &&
                    (! avoid_hor_cross || avoidItem.x() > mid)
                    )
            {
                 itemRectMoved.moveRight(avoidItem.x()- pixelMargin);
                 avoidedCollision = true;
                 *continueChecking = true;
            }
            break;
        }
        case move_Right:
        {
            if( ((avoidItem.x() + pixelMargin) - itemRectOrigin.left() < shift_limit.x()) &&
                    (! avoid_hor_cross || avoidItem.x() < mid))
            {
                itemRectMoved.moveLeft(avoidItem.x()+ pixelMargin);
                avoidedCollision = true;
                *continueChecking = true;
            }
            break;
        }
        case move_Up:
        {
            if( itemRectOrigin.bottom() - (avoidItem.y()- pixelMargin) < shift_limit.y())
            {
                itemRectMoved.moveBottom(avoidItem.y()- pixelMargin);
                avoidedCollision = true;
                *continueChecking = true;
            }
            break;
        }
        case move_Down:
        {
            break;
        }

        }
    }

    /// If
    /// 1) cannot solve current collision, or
    /// 2) will not intersect with further objects,
    /// no need to check anymore
    if( !avoidedCollision ||
            (dir == move_Left && itemRectMoved.left() > avoidItem.x()) ||
            (dir == move_Right && itemRectMoved.right() < avoidItem.x()) ||
            (dir == move_Up && itemRectMoved.bottom() < avoidItem.y())
            ){
        *continueChecking = false;
    }

    // *itemMoved = avoidedCollision;


    //bool moveable = !avoidItem->intersects(itemRect);// && tmpRect.top() >= 0 && tmpRect.left() >= 0 && tmpRect.right() < m_universeRect.right() && tmpRect.bottom() < m_universeRect.bottom() ;
    return avoidedCollision;
}


bool PlotBase::intersectsCurve(direction dir, const point2d & shift_limit,
                const QRectF & rectBeforeMove,
                QRectF & rectAfterMove, pt2idx * index) const
{
    return true;
}

bool PlotBase::intersectsFill(direction dir, const point2d & shift_limit,
                const QRectF & rectBeforeMove,
                QRectF & rectAfterMove, pt2idx * index) const
{
    return true;
}*/



void PlotBase::subtractBy(const point2dList & plist) {
    PlotBase tmpPlotBase;
    tmpPlotBase.setPointList(plist);

    for (unsigned int i = 0; i < m_pointList.size(); i++) {
        double val = tmpPlotBase.evaluate(m_pointList[i].x());
        m_pointList[i].ry() -= val;
    }
}

void PlotBase::addPoints(const point2dList & list) {
    m_pointList.insert(m_pointList.end(), list.begin(), list.end());
    m_isSortedAscendingX = isSortedAscendingX();  //Minor improvment: Could make this line faster by only checking list, but more stable this way.
}

void PlotBase::removePointFromIndexList(const std::vector<pt2idx> & deleteIdxList) {
    std::vector<pt2idx> list = deleteIdxList;
    std::sort(list.rbegin(), list.rend());
    for (unsigned int i = 0; i < list.size(); i++) {
        removePoint(list[i]);
    }
}

void PlotBase::getIntensityListFloat(double t1, double t2, double * actual_t1, double * actual_t2, std::vector<float> & intensityArray) {
    pt2idx start_idx = getIndexLessOrEqual(t1);
    pt2idx end_idx = getIndexLessOrEqual(t2);

    if (start_idx < 0)
        start_idx = 0;
    if (end_idx < 0)
        end_idx = static_cast<pt2idx>(m_pointList.size()) - 1;

    *actual_t1 = m_pointList[start_idx].x();
    *actual_t2 = m_pointList[end_idx].x();

    intensityArray.resize(end_idx-start_idx+1);
    int j = 0;
    for (int i = start_idx; i <= end_idx; i++) {
        intensityArray[j++] = m_pointList[i].y();
    }
}

//TODO: make this more accurate by having polynomial fit
void PlotBase::makeCentroidedPoints(point2dList * _points, CentroidAlgorithmMethod method ) const
{
    if (!_points) {
        return;
    }
    point2dList & points = *_points;
    double lowest_value = 0.0;

    points.clear();
    point2dIdxList idxList;
    getMaxIndexList(idxList);
    if (method == CentroidNaiveMaxValue) {
        makePointsFromIndex(idxList, points);
        return;
    }

    //weigthed average method
    for (int i = 0; i < (int)idxList.size(); i++) {
        int index = idxList[i];
        if (index > 0 && index < (int)(m_pointList.size()-1)) {
            const point2d & p1 = m_pointList[index-1];
            const point2d & p2 = m_pointList[index];
            const point2d & p3 = m_pointList[index+1];
            double w1 = p1.y() > 0 ? p1.y() : 0;
            double w2 = p2.y() > 0 ? p2.y() : 0;
            double w3 = p3.y() > 0 ? p3.y() : 0;

            if (method == CentroidWeightedAverage_RelativeWeight) {
                lowest_value = std::min({ w1, w2, w3 });
                w1 = w1 - lowest_value;
                w2 = w2 - lowest_value;
                w3 = w3 - lowest_value;
            }

            double denomidator =  w1 + w2 + w3;

            if (denomidator > 1e-05) {
                point2d p = p1*w1 + p2*w2 + p3*w3;
                p = p / denomidator;
                p.ry() = evaluate(p.x());
                points.push_back(p);
                //debugCoreMini() << "denom,point=" << denomidator << "," << p;
            } else {
                points.push_back(m_pointList[index]);
            }
        } else if (index >= 0 && index < (int)m_pointList.size()){
            points.push_back(m_pointList[index]);
        } else {
            debugCoreMini() << "idxList index of out bounds. i,index=" << i << "," << index;
        }
    }
}

PMI_COMMON_CORE_MINI_EXPORT Err byteArrayToPlotBase(const QByteArray & xba, const QByteArray & yba, point2dList & plist, bool sortByX, Calibration *calibration) {
    Err e = kNoErr;
    plist.clear();

    //Since we are not directly adding to the raw points (no api construction required unless it becomes computationally expensive),
    //we will first add it to the main points, copy to raw points, and then transform to main points.
    int xsize = xba.size();
    int ysize = yba.size();
    plist.reserve(xsize/sizeof(double));
    if (xsize == ysize) {  //both doubles
        double * xptr = (double*) xba.constData();
        double * yptr = (double*) yba.constData();
        int size = xsize/sizeof(double);
        for (int i = 0;i < size; i++) {
            double xValue = xptr[i];
            if(calibration != NULL) {
                xValue = calibration->calibrate(xValue);
            }
            plist.push_back(point2d(xValue, yptr[i]));
        }
    } else if (xsize == 2*ysize) { //x-double, y-float
        double * xptr = (double*) xba.constData();
        float * yptr = (float*) yba.constData();
        int size = xsize/sizeof(double);
        for (int i = 0; i < size; i++) {
            double xValue = xptr[i];
            if(calibration != NULL) {
                xValue = calibration->calibrate(xValue);
            }
            plist.push_back(point2d(xValue, yptr[i]));
        }
    } else { //unknown
        e = kSQLiteMissingContentError; eee;
    }
    if (sortByX) {
        SortPointListByX(plist, true);
    }

error:
    return e;
}

PMI_COMMON_CORE_MINI_EXPORT Err byteArrayToPlotBase(const QByteArray & xba, const QByteArray & yba, PlotBase * plot, bool sortByX, Calibration *calibration) {
    Err e = kNoErr;
    plot->clear();
    point2dList & plist = plot->getPointList();
    e = byteArrayToPlotBase(xba,yba,plist,sortByX,calibration); eee;
error:
    return e;
}

PMI_COMMON_CORE_MINI_EXPORT Err byteArrayToPlotBase(const QByteArray & xyba, PlotBase * plot, bool sortByX, Calibration *calibration) {
    Err e = kNoErr;
    plot->clear();

    point2dList & plist = plot->getPointList();
    plist.clear();

    int listSize = xyba.size()/(sizeof(double) * 2); // half of the array size, because it contains x and y coordinates
    plist.reserve(listSize);
    double * xyptr = (double*) xyba.constData();

    for (int i = 0; i < listSize; i++) {
        double xValue = xyptr[i*2];
        if(calibration != NULL) {
            xValue = calibration->calibrate(xValue);
        }
        plist.push_back(point2d(xValue, xyptr[i*2+1]));
    }

    if (sortByX) {
        SortPointListByX(plist, true);
    }

//error:
    return e;
}

PMI_COMMON_CORE_MINI_EXPORT Err byteArrayToDoubleVector(const QByteArray & xba, std::vector<double> & list, bool sortByX ) {
    Err e = kNoErr;
    list.clear();

    //Since we are not directly adding to the raw points (no api construction required unless it becomes computationally expensive),
    //we will first add it to the main points, copy to raw points, and then transform to main points.
    int xsize = xba.size();
    double * xptr = (double*) xba.constData();
    int size = xsize/sizeof(double);
    for (int i = 0;i < size; i++) {
        list.push_back(xptr[i]);
    }

    if (sortByX) {
        sort(list.begin(), list.end());
    }

//error:
    return e;
}

PMI_COMMON_CORE_MINI_EXPORT Err byteArrayToFloatVector(const QByteArray & xba, std::vector<float> & list, bool sortByX) {
    Err e = kNoErr;
    list.clear();

    //Since we are not directly adding to the raw points (no api construction required unless it becomes computationally expensive),
    //we will first add it to the main points, copy to raw points, and then transform to main points.
    int xsize = xba.size();
    float * xptr = (float*) xba.constData();
    int size = xsize/sizeof(float);
    for (int i = 0;i < size; i++) {
        list.push_back(xptr[i]);
    }
    if (sortByX) {
        sort(list.begin(), list.end());
    }

//error:
    return e;
}

//get the minima closest to half point
PMI_COMMON_CORE_MINI_EXPORT Err getBestCriticalPositions(const PlotBase & plot, PlotBase::FindPeakOption critial, double startTime, double endTime, pt2idx * mostMidPointIdx, pt2idx * mostValuePointIdx) {
    Err e = kNoErr;
    point2dIdxList critial_idxs;
    point2dList critial_points;
    pt2idx idx1 = plot.getIndexLessOrEqual(startTime);
    pt2idx idx2 = plot.getIndexLessOrEqual(endTime);
    point2d mostValuePt;

    *mostMidPointIdx = -1;
    *mostValuePointIdx = -1;

    //TODO: TIC is noisy; need to smooth it first.
    if (critial == PlotBase::FindMin) {
        plot.getMinIndexList(critial_idxs, idx1, idx2);
    } else {
        plot.getMaxIndexList(critial_idxs, idx1, idx2);
    }
    plot.makePointsFromIndex(critial_idxs, critial_points);

    //find the most middle critical point
    double halfTime = (startTime + endTime)*0.5;
    double bestDiff = endTime - halfTime;
    for (size_t i = 0; i < critial_points.size(); i++) {
        const double currDiff = std::abs(critial_points[i].x() - halfTime);
        if (currDiff < bestDiff ) {
            bestDiff = currDiff;
            *mostMidPointIdx = critial_idxs[i];
        }

        if (i == 0) {
            *mostValuePointIdx = critial_idxs[i];
            mostValuePt = critial_points[i];
        } else {
            if (critial == PlotBase::FindMin) {
                if (mostValuePt.y() > critial_points[i].y()) {
                    *mostValuePointIdx = critial_idxs[i];
                    mostValuePt = critial_points[i];
                }
            } else {
                if (mostValuePt.y() < critial_points[i].y()) {
                    *mostValuePointIdx = critial_idxs[i];
                    mostValuePt = critial_points[i];
                }
            }
        }
    }

    //If nothing found, set as middle of start and end; let the caller know that no minima has been found.
    if (*mostMidPointIdx < 0) {
        if (critial_idxs.size() > 0) {
            const size_t middle_index = critial_idxs.size()/2;
            *mostMidPointIdx = critial_idxs[middle_index];
        } else {
            *mostMidPointIdx = (idx1+idx2)/2;
        }
    }

    if (*mostValuePointIdx < 0) {
        if (critial_idxs.size() > 0) {
            const size_t middle_index = critial_idxs.size()/2;
            *mostValuePointIdx = critial_idxs[middle_index];
        } else {
            *mostValuePointIdx = (idx1+idx2)/2;
        }
    }

    return e;
}

//get the minima closest to half point
PMI_COMMON_CORE_MINI_EXPORT Err getBestMinSplitPosition(const PlotBase & plot, double startTime, double endTime, bool preferMidPointInsteadOfMaxPoint, point2d & minPoint) {
    pt2idx idx_most_middle = -1, idx_most_value=-1;
    Err e = getBestCriticalPositions(plot, PlotBase::FindMin, startTime, endTime, &idx_most_middle, &idx_most_value);
    if (e == kNoErr) {
        if (preferMidPointInsteadOfMaxPoint && idx_most_middle >= 0) {
            minPoint = plot.getPointList()[idx_most_middle];
        } else if (idx_most_value >= 0){
            minPoint = plot.getPointList()[idx_most_value];
        } else {
            return kBadParameterError;
        }
    }
    return e;
}

//get the maxima by value
PMI_COMMON_CORE_MINI_EXPORT Err getBestMaxPositionByValue(const PlotBase & plot, double startTime, double endTime, bool preferMidPointInsteadOfMaxPoint, point2d & maxPoint) {
    pt2idx idx_most_middle = -1, idx_most_value=-1;
    Err e = getBestCriticalPositions(plot, PlotBase::FindMax, startTime, endTime, &idx_most_middle, &idx_most_value);
    if (e == kNoErr) {
        if (preferMidPointInsteadOfMaxPoint && idx_most_middle >= 0) {
            maxPoint = plot.getPointList()[idx_most_middle];
        } else if (idx_most_value >= 0) {
            maxPoint = plot.getPointList()[idx_most_value];
        } else {
            return kBadParameterError;
        }
    }
    return e;
}

PMI_COMMON_CORE_MINI_EXPORT void pointsToByteArray_DoubleFloat_careful(const point2dList & plist,
                                   std::vector<double> & xlist, std::vector<float> & ylist,
                                   QByteArray & xba, QByteArray & yba)
{
    convertPlotToDoubleFloatArray(plist, xlist, ylist);

    toByteArrayReference(xlist, xba);
    toByteArrayReference(ylist, yba);
}

PMI_COMMON_CORE_MINI_EXPORT void pointsToByteArray_DoubleDouble_careful(const point2dList & plist,
    std::vector<double> & xlist, std::vector<double> & ylist,
    QByteArray & xba, QByteArray & yba)
{
    convertPlotToDoubleDoubleArray(plist, xlist, ylist);

    toByteArrayReference(xlist, xba);
    toByteArrayReference(ylist, yba);
}

PMI_COMMON_CORE_MINI_EXPORT void pointsToByteArray_Double_careful(const point2dList & plist,
    std::vector<double> & xylist,
    QByteArray & xyba)
{
    convertPlotToDoubleArray(plist, xylist);

    toByteArrayReference(xylist, xyba);
}

PMI_COMMON_CORE_MINI_EXPORT void removeLowIntensePeaks(point2dList & plist, int max_points) {
    if (int(plist.size()) <= max_points)
        return;

    sort(plist.rbegin(), plist.rend(), point2d_less_y);
    plist.resize(max_points);
    sort(plist.begin(), plist.end(), point2d_less_x);
}

PMI_COMMON_CORE_MINI_EXPORT int removeZeroOrNegativeIntensePeaks(point2dList & plist)
{
    int count_removed = 0;
    point2dList tmplist;
    tmplist.reserve(plist.size());
    for (unsigned int i = 0; i < plist.size(); i++) {
        if (plist[i].y() > 1e-20) {
            tmplist.push_back(plist[i]);
        } else {
            count_removed++;
        }
    }
    swap(plist, tmplist);
    return count_removed;
}

PMI_COMMON_CORE_MINI_EXPORT int removeAllButKIntensePoint(point2dList &plist, int numberOfPointsK)
{
    if (numberOfPointsK < 0 || static_cast<int>(plist.size()) <= numberOfPointsK) {
        return 0;
    }
    point2dList tmplist;
    tmplist.reserve(numberOfPointsK);
    qSort(plist.rbegin(), plist.rend(), point2d_less_y);
    for (int i = 0; i < numberOfPointsK; i++) {
        tmplist.push_back(plist[i]);
    }
    qSort(tmplist.begin(), tmplist.end(), point2d_less_x);
    swap(plist, tmplist);

    int count_removed = static_cast<int>(tmplist.size()) - numberOfPointsK;
    return count_removed;
}

PMI_COMMON_CORE_MINI_EXPORT Err get_best_observed(const point2dList & plist, double calcMr, int charge, const double * ppm_tolerance,
                double * calcMz_iso, point2d * obsMz_iso, double * best_observed_mono_mz) {
    Err e = kNoErr;

    int iso_number = mostIntenseIsotope(calcMr);
    double calcMr_iso = calcMr + (iso_number) * kAverageIsotopeDelta;
    *calcMz_iso = mz_from_mr(calcMr_iso, charge);
    obsMz_iso->rx() = *calcMz_iso; //make it have invalid value
    obsMz_iso->ry() = 0;
    pt2idx idx = findClosestIndex(plist, *calcMz_iso);
    if (idx < 0 || idx >= int(plist.size())) {
        //debugCoreMini() << "Closest point not found for calcMr,charge,obsMz_iso=" << calcMr << "," << charge << "," << *obsMz_iso;
        e = kError; eee;
    } else {
        *obsMz_iso = plist[idx];
        //get best mono based on highest intense peak, since mono may not exists
        *best_observed_mono_mz = obsMz_iso->x() - iso_number * kAverageIsotopeDelta/charge;
        if (ppm_tolerance) {
            double calcMz = mz_from_mr(calcMr, charge);
            double ppm_diff = compute_ppm_error(*best_observed_mono_mz, calcMz, true);
            if (ppm_diff > *ppm_tolerance) {
                /*
                debugCoreMini() << "Could not find observed m/z within the given tolerance.  obsMz,calcMz,ppm_tolerance,ppm_diff="
                         << *best_observed_mono_mz << "," << calcMz << "," << *ppm_tolerance << "," << ppm_diff;
                */
                return kError;  //no need to catch error exception via ecatch.
            }
        }
    }

error:
    return e;
}

// good fudge to use = 1e-6
PMI_COMMON_CORE_MINI_EXPORT bool isSimilar_pointbypoint(const PlotBase & plota, const PlotBase & plotb, double fudge)
{
    const point2dList & pa = plota.getPointList();
    const point2dList & pb = plotb.getPointList();

    if (pa.size() != pb.size())
        return false;

    /*
    point2d a, b;
    point2d diff = a - b;
    double diff_dot = QPointF::dotProduct(diff, diff);
    */

    bool allPointsWithinFudge = true;
    for (int i = 0; i < plota.getPointListSize(); i++) {
        const double diffx = std::abs(plota.getPoint(i).x() - plotb.getPoint(i).x());
        const double diffy = std::abs(plota.getPoint(i).y() - plotb.getPoint(i).y());
        if (diffx > fudge || diffy > fudge) {
            allPointsWithinFudge = false;
            break;
        }
    }

    return allPointsWithinFudge;
}

PMI_COMMON_CORE_MINI_EXPORT void reduceDensityInX(const point2dList & plist, double x_scaling, point2dList * outList)
{
    QMap<int, point2d> i_p;  //interval x and most intense point

    outList->clear();
    if (x_scaling <= 0) {
        debugCoreMini() << "x_interval is improper, return empty list";
        return;
    }
    for (const point2d p : plist) {
        int x_scaled = (int)(p.x() * x_scaling);
        auto itr = i_p.find(x_scaled);
        if (itr == i_p.end()) {
            i_p[x_scaled] = p;
        } else {
            if (itr->y() < p.y()) {
                *itr = p;
            }
        }
    }

    outList->reserve(i_p.size());
    for (auto itr = i_p.begin(); itr != i_p.end(); ++itr) {
        outList->push_back(*itr);
    }
}

PMI_COMMON_CORE_MINI_EXPORT PlotBase::ComputeAreaMethod getDeamQuantMethod()
{
    //method == ComputeAreaMethod_KeepNegativeAreas_ButClampToZeroAfterwards
    PlotBase::ComputeAreaMethod method = PlotBase::ComputeAreaMethod_IgonoreNegativeAreas;
    return method;
}


_PMI_END

// #define DO_PMI_UNIT_TESTS

#ifdef DO_PMI_UNIT_TESTS
class UnitTestPlotBase {
public:
    UnitTestPlotBase() {
        pmi::PlotBase plotSignal;
        pmi::PlotBase plotBaseline;
        for (double x = -10; x < 10; x += 0.01) {
            point2d p(x,0);
            p.ry() = (x - 10)*(x-2);
            point2d b(x,0);
            b.ry() = -12*x;
            plotSignal.addPoint(p);
            plotBaseline.addPoint(b);
        }
        point2d beforeMinPoint = plotSignal.getMinPoint();
        plotSignal.subtractBy(plotBaseline.getPointList());
        point2d afterMinPoint = plotSignal.getMinPoint();
        if (!pmi::is_close(afterMinPoint.x(), 0, 0.0000001)) {
            debugCoreMini() << "UnitTestFail:";
        } else {
            debugCoreMini() << "UnitTest:UnitTestPlotBase Sucess:";
        }
        debugCoreMini() << "BeforeMinPoint" << beforeMinPoint;
        debugCoreMini() << "AfterMinPoint" << afterMinPoint;
    }
};

static UnitTestPlotBase obj;

class foobar_PlotAreaTest {
public:
    foobar_PlotAreaTest() {
        pmi::PlotBase plot;
        plot.addPoint(point2d(2,1));
        plot.addPoint(point2d(3,1));
        plot.addPoint(point2d(4,1));
        plot.addPoint(point2d(5,1));

        double a = plot.computeAreaFast(-1,1);
        double b = plot.computeAreaFast(1,2);
        double c = plot.computeAreaFast(5,6);
        double d = plot.computeAreaFast(6,7);

        //double a = plot.computeAreaFast(1,6);
        //double b = plot.computeAreaFast(1,2.5);
        //double c = plot.computeAreaFast(2,6);
        //double d = plot.computeAreaFast(2.5,4.5);
        //double e = plot.computeAreaFast(4.5,6);
    }
};

foobar_PlotAreaTest sdfsdfioj;

class ComputeAreaFastTest : public pmi::PlotBase {
private:
    bool m_verbosePrinting = true;
    ComputeAreaMethod m_method = ComputeAreaMethod::ComputeAreaMethod_KeepNegativeAreas;

    enum TestStatus {
        Failure,
        Success,
        Error
    };

    struct TestingRectangle {
        double x_start=0;
        double x_end=0;
        double y=0;

    };

    // All possible intersections
    // 1. < > (      )
    TestStatus allBeforePositiveRectangleTest();
    TestStatus allBeforeNegativeRectangleTest();

    // 2. <   (  >   )
    TestStatus partBeforePositiveRectangleTest();
    TestStatus partBeforeNegativeRectangleTest();


    // 3.     ( <  > )
    TestStatus allInsidePositiveRectangleTest();
    TestStatus allInsideNegativeRectangleTest();


    // 4.     (  <   )  >
    TestStatus partAfterPositiveRectangleTest();
    TestStatus partAfterNegativeRectangleTest();

    // 5.     (      ) < >
    TestStatus allAfterPositiveRectangleTest();
    TestStatus allAftereNegativeRectangleTest();

    // 6. <   (      )  >
    TestStatus enclosingPositiveRectangleTest();
    TestStatus enclosingNegativeRectangleTest();

    TestStatus rectangleTest(double rect_x_start, double rect_x_end, double rect_y, double test_start, double test_end, double expected);

    TestStatus rectangleTest(TestingRectangle rect, double test_start, double test_end, double expected);

    TestStatus test(double start, double end, double expected);

    TestStatus validateTest(double obtained, double expected);

    void printTestResult(TestStatus status, QString testName);

    void runAllTests();

    void setPointListToRectangle(double x_start, double x_end, double y_value);
public:
    ComputeAreaFastTest();
};

ComputeAreaFastTest::ComputeAreaFastTest() {
    runAllTests();
};

void ComputeAreaFastTest::printTestResult(TestStatus status, QString testName) {
    QString resultString = "Error";
    switch (status) {
    case TestStatus::Success:
        resultString = "Success";
        break;
    case TestStatus::Failure:
        resultString = "Failure";
        break;
    default:
        resultString = "Error";
        break;
    }
    debugCoreMini().nospace() << "<TestResult>" << testName << "," << resultString << "</TestResult>\n";
};

void ComputeAreaFastTest::runAllTests() {
    debugCoreMini() << "Running all tests for PlotBase::computeAreaFast:";

    // :::::::::: Positive Rectangle tests ::::::::::
    printTestResult(allBeforePositiveRectangleTest(), "AllBeforePositiveRectangleTest");
    printTestResult(partBeforePositiveRectangleTest(), "PartBeforePositiveRectangleTest");
    printTestResult(allInsidePositiveRectangleTest(), "AllInsidePositiveRectangleTest");
    printTestResult(partAfterPositiveRectangleTest(), "PartAfterPositiveRectangleTest");
    printTestResult(allAfterPositiveRectangleTest(), "AllAfterPositiveRectangleTest");
    printTestResult(enclosingPositiveRectangleTest(), "EnclosingPositiveRectangleTest");

    // :::::::::: Negative Y Rectangle tests ::::::::::
    TestStatus status = TestStatus::Error;

    // Create negative rectangle, Area of rectangle = -1000
    TestingRectangle rect;
    rect.x_start = 100;
    rect.x_end = 200;
    rect.y = -10;

    // keep negative areas
    m_method = ComputeAreaMethod::ComputeAreaMethod_KeepNegativeAreas;
    status = rectangleTest(rect, 0, 50, 0);
    printTestResult(status, "AllBeforeNegativeRectangleKeepNegativeTest");

    status = rectangleTest(rect, 0, 150, -500);
    printTestResult(status, "PartBeforeNegativeRectangleKeepNegativeTest");

    status = rectangleTest(rect, 125, 175, -500);
    printTestResult(status, "AllInsideNegativeRectangleKeepNegativeTest");

    status = rectangleTest(rect, 150, 250, -500);
    printTestResult(status, "PartAfterNegativeRectangleKeepNegativeTest");

    status = rectangleTest(rect, 250, 300, 0);
    printTestResult(status, "AllAfterNegativeRectangleKeepNegativeTest");

    status = rectangleTest(rect, 50, 250, -1000);
    printTestResult(status, "EnclosingNegativeRectangleKeepNegativeTest");

    // ignore negative areas
    m_method = ComputeAreaMethod::ComputeAreaMethod_IgonoreNegativeAreas;
    status = rectangleTest(rect, 0, 50, 0);
    printTestResult(status, "AllBeforeNegativeRectangleIgnoreNegativeTest");

    status = rectangleTest(rect, 0, 150, 0);
    printTestResult(status, "PartBeforeNegativeRectangleIgnoreNegativeTest");

    status = rectangleTest(rect, 125, 175, 0);
    printTestResult(status, "AllInsideNegativeRectangleIgnoreNegativeTest");

    status = rectangleTest(rect, 150, 250, 0);
    printTestResult(status, "PartAfterNegativeRectangleIgnoreNegativeTest");

    status = rectangleTest(rect, 250, 300, 0);
    printTestResult(status, "AllAfterNegativeRectangleIgnoreNegativeTest");

    status = rectangleTest(rect, 50, 250, 0);
    printTestResult(status, "EnclosingNegativeRectangleIgnoreNegativeTest");

    // keep negative areas but clamp
    m_method = ComputeAreaMethod::ComputeAreaMethod_KeepNegativeAreas_ButClampToZeroAfterwards;
    status = rectangleTest(rect, 0, 50, 0);
    printTestResult(status, "AllBeforeNegativeRectangleIgnoreNegativeClampTest");

    status = rectangleTest(rect, 0, 150, 0);
    printTestResult(status, "PartBeforeNegativeRectangleIgnoreNegativeClampTest");

    status = rectangleTest(rect, 125, 175, 0);
    printTestResult(status, "AllInsideNegativeRectangleIgnoreNegativeClampTest");

    status = rectangleTest(rect, 150, 250, 0);
    printTestResult(status, "PartAfterNegativeRectangleIgnoreNegativeClampTest");

    status = rectangleTest(rect, 250, 300, 0);
    printTestResult(status, "AllAfterNegativeRectangleIgnoreNegativeClampTest");

    status = rectangleTest(rect, 50, 250, 0);
    printTestResult(status, "EnclosingNegativeRectangleIgnoreNegativeClampTest");

    // :::::::::: Negative Everything Rectangle Tests ::::::::::
    rect.x_start = -100;
    rect.x_end = -200;
    rect.y = -10;

    // keep negative areas
    m_method = ComputeAreaMethod::ComputeAreaMethod_KeepNegativeAreas;
    status = rectangleTest(rect, -300, -250, 0);
    printTestResult(status, "AllBeforeAllNegativeRectangleKeepNegativeAreasTest");

    status = rectangleTest(rect, -250, -150, 0);
    printTestResult(status, "PartBeforeAllNegativeRectangleKeepNegativeAreasTest");

    status = rectangleTest(rect, -175, -125, 0);
    printTestResult(status, "AllInsideAllNegativeRectangleKeepNegativeAreasTest");

    status = rectangleTest(rect, -150, -50, 0);
    printTestResult(status, "PartAfterAllNegativeRectangleKeepNegativeAreasTest");

    status = rectangleTest(rect, -50, 0, 0);
    printTestResult(status, "AllAfterAllNegativeRectangleKeepNegativeAreasTest");

    status = rectangleTest(rect, -250, -50, 0);
    printTestResult(status, "EnclosingAllNegativeRectangleKeepNegativeAreasTest");

    // ignore negative areas
    m_method = ComputeAreaMethod::ComputeAreaMethod_IgonoreNegativeAreas;
    status = rectangleTest(rect, -300, -250, 0);
    printTestResult(status, "AllBeforeAllNegativeRectangleIgnoreNegativeAreasTest");

    status = rectangleTest(rect, -250, -150, 0);
    printTestResult(status, "PartBeforeAllNegativeRectangleIgnoreNegativeAreasTest");

    status = rectangleTest(rect, -175, -125, 0);
    printTestResult(status, "AllInsideAllNegativeRectangleIgnoreNegativeAreasTest");

    status = rectangleTest(rect, -150, -50, 0);
    printTestResult(status, "PartAfterAllNegativeRectangleIgnoreNegativeAreasTest");

    status = rectangleTest(rect, -50, 0, 0);
    printTestResult(status, "AllAfterAllNegativeRectangleIgnoreNegativeAreasTest");

    status = rectangleTest(rect, -250, -50, 0);
    printTestResult(status, "EnclosingAllNegativeRectangleIgnoreNegativeAreasTest");

    // keep negative areas but clamp
    m_method = ComputeAreaMethod::ComputeAreaMethod_KeepNegativeAreas_ButClampToZeroAfterwards;
    status = rectangleTest(rect, -300, -250, 0);
    printTestResult(status, "AllBeforeAllNegativeRectangleIgnoreNegativeAreasClampTest");

    status = rectangleTest(rect, -250, -150, 0);
    printTestResult(status, "PartBeforeAllNegativeRectangleIgnoreNegativeAreasClampTest");

    status = rectangleTest(rect, -175, -125, 0);
    printTestResult(status, "AllInsideAllNegativeRectangleIgnoreNegativeAreasClampTest");

    status = rectangleTest(rect, -150, -50, 0);
    printTestResult(status, "PartAfterAllNegativeRectangleIgnoreNegativeAreasClampTest");

    status = rectangleTest(rect, -50, 0, 0);
    printTestResult(status, "AllAfterAllNegativeRectangleIgnoreNegativeAreasClampTest");

    status = rectangleTest(rect, -250, -50, 0);
    printTestResult(status, "EnclosingAllNegativeRectangleIgnoreNegativeAreasClampTest");

    // :::::::::: Custom Tests ::::::::::
    // Positive slope, negative to positive y
    m_pointList.clear();

    // x will be from 100 to 200, inclusive
    for (int x_pos = 100; x_pos <= 200; x_pos++) {
        // y will be from -50 to 50, inclusive
        int y_pos = x_pos - 150;
        point2d point;
        point.setX(x_pos);
        point.setY(y_pos);
        m_pointList.push_back(point);
    }

    // keep negative areas
    m_method = ComputeAreaMethod::ComputeAreaMethod_KeepNegativeAreas;
    status = test(0, 50, 0);
    printTestResult(status, "AllBeforePositiveSlopeKeepNegativeAreasTest");

    status = test(50, 150, -1250);
    printTestResult(status, "PartBeforePositiveSlopKeepNegativeAreaseTest");

    status = test(125, 175, 0);
    printTestResult(status, "AllInsidePositiveSlopeKeepNegativeAreasTest");

    status = test(150, 250, 1250);
    printTestResult(status, "PartAfterPositiveSlopeKeepNegativeAreasTest");

    status = test(250, 300, 0);
    printTestResult(status, "AllAfterPositiveSlopeKeepNegativeAreasTest");

    status = test(50, 250, 0);
    printTestResult(status, "EnclosingPositiveSlopeKeepNegativeAreasTest");

    // ignore negative areas
    m_method = ComputeAreaMethod::ComputeAreaMethod_IgonoreNegativeAreas;
    status = test(0, 50, 0);
    printTestResult(status, "AllBeforePositiveSlopeIgnoreNegativeAreasTest");

    status = test(50, 150, 0);
    printTestResult(status, "PartBeforePositiveSlopeIgnoreNegativeAreasTest");

    status = test(125, 175, 312.5);
    printTestResult(status, "AllInsidePositiveSlopeIgnoreNegativeAreasTest");

    status = test(150, 250, 1250);
    printTestResult(status, "PartAfterPositiveSlopeIgnoreNegativeAreasTest");

    status = test(250, 300, 0);
    printTestResult(status, "AllAfterPositiveSlopeIgnoreNegativeAreasTest");

    status = test(50, 250, 1250);
    printTestResult(status, "EnclosingPositiveSlopeIgnoreNegativeAreasTest");

    // keep negative areas but clamp
    m_method = ComputeAreaMethod::ComputeAreaMethod_KeepNegativeAreas_ButClampToZeroAfterwards;
    status = test(0, 50, 0);
    printTestResult(status, "AllBeforePositiveSlopeKeepNegativeAreasClampTest");

    status = test(50, 150, 0);
    printTestResult(status, "PartBeforePositiveSlopKeepNegativeAreasClampTest");

    status = test(125, 175, 0);
    printTestResult(status, "AllInsidePositiveSlopeKeepNegativeAreasClampTest");

    status = test(150, 250, 1250);
    printTestResult(status, "PartAfterPositiveSlopeKeepNegativeAreasClampTest");

    status = test(250, 300, 0);
    printTestResult(status, "AllAfterPositiveSlopeKeepNegativeAreasClampTest");

    status = test(50, 250, 0);
    printTestResult(status, "EnclosingPositiveSlopeKeepNegativeAreasClampTest");

    // Positive slope, positive to negative y
    m_pointList.clear();

    m_pointList.push_back(point2d(100, -50));
    m_pointList.push_back(point2d(200, 50));

    // keep negative areas
    m_method = ComputeAreaMethod::ComputeAreaMethod_KeepNegativeAreas;
    status = test(0, 50, 0);
    printTestResult(status, "AllBeforePositiveSlopeKeepNegativeAreasTwoPointsTest");

    status = test(50, 150, -1250);
    printTestResult(status, "PartBeforePositiveSlopeKeepNegativeAreasTwoPointsTest");

    status = test(125, 175, 0);
    printTestResult(status, "AllInsidePositiveSlopeKeepNegativeAreasTwoPointsTest");

    status = test(150, 250, 1250);
    printTestResult(status, "PartAfterPositiveSlopeKeepNegativeAreasTwoPointsTest");

    status = test(250, 300, 0);
    printTestResult(status, "AllAfterPositiveSlopeKeepNegativeAreasTwoPointsTest");

    status = test(50, 250, 0);
    printTestResult(status, "EnclosingPositiveSlopeKeepNegativeAreasTwoPointsTest");

    // ignore negative areas
    m_method = ComputeAreaMethod::ComputeAreaMethod_IgonoreNegativeAreas;
    status = test(0, 50, 0);
    printTestResult(status, "AllBeforePositiveSlopeIgnoreNegativeAreasTwoPointsTest");

    status = test(50, 150, 0);
    printTestResult(status, "PartBeforePositiveSlopeIgnoreNegativeAreasTwoPointsTest");

    status = test(125, 175, 312.5);
    printTestResult(status, "AllInsidePositiveSlopeIgnoreNegativeAreasTwoPointsTest");

    status = test(150, 250, 1250);
    printTestResult(status, "PartAfterPositiveSlopeIgnoreNegativeAreasTwoPointsTest");

    status = test(250, 300, 0);
    printTestResult(status, "AllAfterPositiveSlopeIgnoreNegativeAreasTwoPointsTest");

    status = test(50, 250, 1250);
    printTestResult(status, "EnclosingPositiveSlopeIgnoreNegativeAreasTwoPointsTest");

    // keep negative areas but clamp
    m_method = ComputeAreaMethod::ComputeAreaMethod_KeepNegativeAreas_ButClampToZeroAfterwards;
    status = test(0, 50, 0);
    printTestResult(status, "AllBeforePositiveSlopeKeepNegativeAreasTwoPointsClampTest");

    status = test(50, 150, 0);
    printTestResult(status, "PartBeforePositiveSlopeKeepNegativeAreasTwoPointsClampTest");

    status = test(125, 175, 0);
    printTestResult(status, "AllInsidePositiveSlopeKeepNegativeAreasTwoPointsClampTest");

    status = test(150, 250, 1250);
    printTestResult(status, "PartAfterPositiveSlopeKeepNegativeAreasTwoPointsClampTest");

    status = test(250, 300, 0);
    printTestResult(status, "AllAfterPositiveSlopeKeepNegativeAreasTwoPointsClampTest");

    status = test(50, 250, 0);
    printTestResult(status, "EnclosingPositiveSlopeKeepNegativeAreasTwoPointsClampTest");

}

void ComputeAreaFastTest::setPointListToRectangle(double x_start, double x_end, double y_value) {
    m_pointList.clear();
    // y will be all y_value, x will be continuous ints from x_start to x_end
    for (int i = x_start; i <= x_end; i++) {
        point2d point;
        point.setX(i);
        point.setY(y_value);
        m_pointList.push_back(point);
    }
};

ComputeAreaFastTest::TestStatus ComputeAreaFastTest::rectangleTest(double rect_x_start, double rect_x_end, double rect_y, double test_start, double test_end, double expected) {
    setPointListToRectangle(rect_x_start, rect_x_end, rect_y);
    TestStatus status_a, status_b;
    status_a = test(test_start, test_end, expected);
    // Test putting in start/end in reverse
    status_b = test(test_end, test_start, expected);
    if (status_a == TestStatus::Success && status_b == TestStatus::Success) {
        return TestStatus::Success;
    }
    else if (status_a == TestStatus::Failure && status_b == TestStatus::Failure) {
        return TestStatus::Failure;
    }
    return TestStatus::Error;
};

ComputeAreaFastTest::TestStatus ComputeAreaFastTest::rectangleTest(TestingRectangle rect, double test_start, double test_end, double expected) {
    return rectangleTest(rect.x_start, rect.x_end, rect.y, test_start, test_end, expected);
};

ComputeAreaFastTest::TestStatus ComputeAreaFastTest::allBeforePositiveRectangleTest() {
    setPointListToRectangle(100, 200, 10);
    return test(0, 50, 0);
};

ComputeAreaFastTest::TestStatus ComputeAreaFastTest::partBeforePositiveRectangleTest() {
    setPointListToRectangle(100, 200, 10);
    return test(0, 150, 500);
};

ComputeAreaFastTest::TestStatus ComputeAreaFastTest::allInsidePositiveRectangleTest() {
    setPointListToRectangle(100, 200, 10);
    return test(110, 190, 800);
};

ComputeAreaFastTest::TestStatus ComputeAreaFastTest::partAfterPositiveRectangleTest() {
    setPointListToRectangle(100, 200, 10);
    return test(150, 300, 500);
};

ComputeAreaFastTest::TestStatus ComputeAreaFastTest::allAfterPositiveRectangleTest() {
    setPointListToRectangle(100, 200, 10);
    return test(250, 350, 0);
};

ComputeAreaFastTest::TestStatus ComputeAreaFastTest::enclosingPositiveRectangleTest() {
    setPointListToRectangle(100, 200, 10);
    return test(50, 250, 1000);
};

ComputeAreaFastTest::TestStatus ComputeAreaFastTest::test(double start, double end, double expected) {
    double returnedValue = computeAreaFast(start, end, m_method);
    double expectedValue = expected;
    return validateTest(returnedValue, expectedValue);
};

ComputeAreaFastTest::TestStatus ComputeAreaFastTest::validateTest(double obtained, double expected) {
    double epsilon = .0001;
    double difference = abs(obtained - expected);
    TestStatus returnTestStatus = TestStatus::Error;
    if (m_verbosePrinting) {
        debugCoreMini().nospace() << "<Obtained>" << obtained << "</Obtained>\n" << "<Expected>" << expected << "</Expected>";
    }
    if (difference < epsilon) {
        returnTestStatus = TestStatus::Success;
    }
    else {
        returnTestStatus = TestStatus::Failure;
    }
    return returnTestStatus;
};

static ComputeAreaFastTest testInstance;

#endif
