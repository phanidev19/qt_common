/*
 * Copyright (C) 2012-2014 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 */


#ifndef __PLOT_BASE_H__
#define __PLOT_BASE_H__

#include "Calibration.h"
#include <common_errors.h>
#include <common_types.h>
#include <vector>
#include <QVariant>

enum direction{
    move_Left = 0,
    move_Right = 1,
    move_UpLeft = 2,
    move_UpRight = 3
};

#define OUT_OF_BOUNDS_ON_LEFT -1
#define OUT_OF_BOUNDS_ON_RIGHT -2

class RowNode;

//#define VALIDATE_BINARY_SEARCH_AREA

_PMI_BEGIN

struct PMI_COMMON_CORE_MINI_EXPORT line2d
{
    point2d a,b;

    bool matchesEndPoint(const point2d & point) const {
        return point == a || point == b;
    }

    line2d() {
    }

    line2d(const point2d & aa, const point2d & bb) : a(aa), b(bb) {
    }
};

inline void
convertPlotToDoubleDoubleArray(const std::vector<point2d> & list, std::vector<double> & xlist, std::vector<double> & ylist) {
    xlist.clear();
    ylist.clear();
    for (unsigned int i = 0; i < list.size(); i++) {
        xlist.push_back(list[i].x());
        ylist.push_back(list[i].y());
    }
}

inline void
convertPlotToDoubleArray(const std::vector<point2d> & list, std::vector<double> & xylist) {
    return convertPlotToDoubleDoubleArray(list, xylist, xylist);
}

inline void
convertPlotToDoubleFloatArray(const std::vector<point2d> & list, std::vector<double> & xlist, std::vector<float> & ylist) {
    xlist.clear();
    ylist.clear();
    for (unsigned int i = 0; i < list.size(); i++) {
        xlist.push_back(list[i].x());
        ylist.push_back(list[i].y());
    }
}

#define kIntersectionFlagNone 0
#define kIntersectionFlagMidPoints 1  //not baseline points intersecting signal
#define kIntersectionFlagEndPoints 2  //baseline points touching signal
#define kIntersectionFlagAllPoints (kIntersectionFlagMidPoints | kIntersectionFlagEndPoints)

struct PMI_COMMON_CORE_MINI_EXPORT PeakInterval
{
    pt2idx istart;
    pt2idx iend;
    pt2idx iapex;

    PeakInterval() {
        istart = -1;
        iend = -1;
        iapex = -1;
    }

    PeakInterval(pt2idx t1, pt2idx t2) {
        istart = t1;
        iend = t2;
        iapex = (t1+t2)/2;
    }

    PeakInterval(pt2idx t1, pt2idx t2, pt2idx ta) {
        istart = t1;
        iend = t2;
        iapex = ta;
    }
};

/*!
 * @brief All about m_pointList. Let's make sure changing the point list is wrapped so that we can handle transformation properly in the inherited class.
 */
class PMI_COMMON_CORE_MINI_EXPORT PlotBase
{
public:
    PlotBase() : m_isSortedAscendingX(true) {
    }

    PlotBase(const point2dList &list)
        : m_pointList(list)
        , m_isSortedAscendingX(isSortedAscendingX())
    {
    }

    PlotBase(point2dList &&list): m_pointList(std::move(list))
        , m_isSortedAscendingX(isSortedAscendingX())
    {
    }

    virtual ~PlotBase() {
    }

    inline void swap(PlotBase &other) {
        m_pointList.swap(other.m_pointList);
        std::swap(m_isSortedAscendingX, other.m_isSortedAscendingX);
        std::swap(m_name, other.m_name);
    }

    //TODO: rename delta() --> averageSampleSpacing
    /*!
     * \brief delta is the x-width between two adjacent values.  This function assumes uniformly spaced values
     * \return 0 if one or less points, otherwise e.g. list[1]-list[0]
     */
    double delta() const {
        if (m_pointList.size() < 1)
            return 0;
        return (m_pointList.back().x()-m_pointList.front().x())/(m_pointList.size()-1.0);
    }

    ///////////////////////////////////
    // Access
    ///////////////////////////////////

    virtual void clear() {
        m_pointList.clear();
        m_isSortedAscendingX = true;
    }

    void addPoint(const point2d & obj, bool sortByX = false) {
        if (m_pointList.size() > 0) {
            if (obj.x() < m_pointList.back().x())
                m_isSortedAscendingX = false;
        }
        m_pointList.push_back(obj);
        if (sortByX) {
            sortPointListByX();
        }
    }

    void addPoint(double x, double y, bool sortByX = false) {
        point2d point(x,y);
        addPoint(point, sortByX);
    }

    /*!
     * \brief addPoints append points to end of list
     * \param list if not sorted and greater than the last point, this will affect the internal m_isSortedAscending list.
     */
    void addPoints(const point2dList & list);

    virtual Err removePoint(pt2idx idx, point2d * removedPoint = NULL) {
        if (idx >= 0 && idx < int(m_pointList.size())) {
            if (removedPoint)
                *removedPoint = m_pointList[idx];
            m_pointList.erase(m_pointList.begin()+idx);
        } else {
            std::cerr << "idx not found for removePoint" << std::endl;
            return kBadParameterError;
        }
        return kNoErr;
    }

    /*!
     * \brief this will attempt the exact point from the list
     * \param point to remove
     * \return Error message
     */
    pt2idx removePoint(const point2d &point) {
        pt2idx idx = findIndexExactMatch(point);
        if (idx) {
            removePoint(idx);
            return idx;
        }
        return -1;
    }

    /*!
     * \brief finds the exact point index
     * \param point to search
     * \return -1 if not found, other wise the proper index value
     */
    pt2idx findIndexExactMatch(const point2d & point) const {
        for (unsigned int i = 0; i < m_pointList.size(); i++) {
            if (m_pointList[i] == point)
                return i;
        }
        return -1;
    }

    void removePointFromIndexList(const std::vector<pt2idx> & deleteIdxList);

    const point2dList & getPointList() const {
        return m_pointList;
    }

    point2dList & getPointList() {
        return m_pointList;
    }

    bool setPoint(pt2idx idx, const point2d & obj) {
        if (idx >= 0 && idx < int(m_pointList.size())) {
            m_pointList[idx] = obj;
            return true;
        }
        std::cerr << idx << " idx not found for setPoint with size " << m_pointList.size() << std::endl;
        return false;
    }

    point2d getPoint(pt2idx idx, bool * ok = NULL) const {
        static point2d bla;
        if (idx >= 0 && idx < int(m_pointList.size())) {
            if (ok) *ok = true;
            return m_pointList[idx];
        }

        std::cerr << idx << " idx not found for getPoint with size " << m_pointList.size() << std::endl;
        if (ok) *ok = false;
        return bla;
    }

    virtual void setPointList(const point2dList & plist, Calibration *calibration = nullptr) {
        m_pointList = plist;
        if(calibration != nullptr) {
            _calibratePoints(*calibration);
        }
        m_isSortedAscendingX = isSortedAscendingX();
    }

    /*!
     * \brief Resizes pointList to \a size.
     * If \a size is higher than current, fills with default - point2d(0, 0).
     */
    void resizePointList(size_t size) {
        if (size == m_pointList.size()) {
            return;
        }
        size_t previousSize = m_pointList.size();

        point2d previousLastElement;
        if (!m_pointList.empty()) {
            previousLastElement = m_pointList.back();
        }

        m_pointList.resize(size);

        if (m_isSortedAscendingX) {
            // if we trim it is still sorted
            // otherwise if we add items that are lower that last element it is not sorted
            if (m_pointList.size() > previousSize && previousLastElement.x() > 0.0) {
                m_isSortedAscendingX = false;
            }
        } else {
            // not sorted and trimmed
            // it is possible that it was partially sorted and we trimmed unsorted part
            if (m_pointList.size() < previousSize) {
                m_isSortedAscendingX = isSortedAscendingX();
            }
        }
    }

    int getPointListSize() const {
        return static_cast<int>(m_pointList.size());
    }

    void scalePoints(const point2d & scaleP) {
        for (unsigned int i = 0; i < m_pointList.size(); i++) {
            m_pointList[i].rx() *= scaleP.x();
            m_pointList[i].ry() *= scaleP.y();
        }
    }

    /*!
     * @brief Use this to draw the area between t_start and t_end.
     * Poorly implemented.Need to apply linear interpolation. 411
     * @param t_start start position
     * @param t_end end position
     */
    point2dList getAreaPointList(double t_start, double t_end, bool includeOutOfDomainEndpoints, bool useBinarySearch) const;

    point2dList getPointsBetween(double t_start, double t_end, bool useBinarySearch) const;

    /*!
     * \brief Get points between the given the x range
     * \param t_start
     * \param t_end
     * \param useBinarySearch only use if the contained data is sorted.
     * \return
     */
    point2dList getDataPoints(double t_start, double t_end, bool useBinarySearch) const;

    /*!
     * @brief Sort the MS2 peak list by their masses.  Call this if the peak list not sorted for some reason.
     */
    void sortPointListByX(bool doAscending = true);

    void sortPointListByY(bool doAscending = true);

    ///////////////////////////////////
    // File I/O
    ///////////////////////////////////
    /*!
     * \brief getDataFromFile load comma or tab seperated points
     * \param filename e.g. file.csv
     * \param doSort force a sort after loading
     * \return
     */
    virtual Err getDataFromFile(const QString & filename, bool doSort = true, bool outputMessage = true);

    /*!
     * \brief saveDataToFile will save comma seperated x,y data
     * \param filename output filename
     * \return Err
     */
    virtual Err saveDataToFile(const QString & outFilename) const;

    ///////////////////////////////////
    // Math utils
    ///////////////////////////////////
    /*!
     * @brief get the domain of x; note: current version searches all values and thus slow
     */
    void getXBound(double * t1, double *t2) const;

    void getMinMaxIndexList(point2dIdxList & idxList, int start_index=-1, int end_index=-1) const;

    void getMinIndexList(point2dIdxList & idxList, int start_index = -1, int end_index = -1) const;

    void getMaxIndexList(point2dIdxList & idxList, int start_index = -1, int end_index = -1) const;

    Err makePointsFromIndex(const point2dIdxList & idxList, point2dList & points) const;

    /*!
     * \brief getIndexLessOrEqual finds first index such at m_pointList[index] <= xloc
     * \param xloc x-position query
     * \param outOfBoundsOnRightReturnNegativeTwo  if xloc is greater than the last position, return -1 if this is true. Otherwise, return the last index
     * \return -1 if out of bounds or valid index
     */
    pt2idx getIndexLessOrEqual(double xloc, bool outOfBoundsOnRightReturnNegativeTwo = true) const;

    pt2idx getIndexLessThanValLinear(double xloc, const pt2idx searchStartIdx = 0) const;

    /*!
     * @brief For a given x value, this will return the y value that will on the curve.
     * Poorly implemented. Need to apply linear interpolation. 411
     *
     * @param x
     * @return double
     */
    double evaluate(double x, int interpolate = 1, int use_boundary_value = 0) const;

    void evaluate_slow(const std::vector<double> &xs, std::vector<double> * values);

    // m_pointlist has multiple points with same x value, it will evaluate based on the first point with the x value
    void evaluate_linear_old_v1(const std::vector<double> &sortedXs, std::vector<double> * values) const;
    void evaluate_linear(const std::vector<double> &sortedXs, std::vector<double> * values) const;
    

    /*!
     * \brief evaluatePointList for each x position, find the y value and assign
     * \param plist the list points and point's y values to be adjusted
     */
    void evaluateList(point2dList & plist) const;

    enum ComputeAreaMethod {
         ComputeAreaMethod_KeepNegativeAreas
        ,ComputeAreaMethod_KeepNegativeAreas_ButClampToZeroAfterwards  //Use negative values, but if the final value is negative, we make it zero; this is for deamidation quant.
        ,ComputeAreaMethod_IgonoreNegativeAreas  //TODO: not used anymore; consider removing later.
    };

    /*!
     * @brief Get area between the provided interval.  This will not adjust the computed area unit.
     * @param t_start start position
     * @param t_end end position
     */
    double computeAreaNoUnitConversion(double t_start, double t_end, bool useBinarySearch, ComputeAreaMethod method) const;

    /*!
     * @brief Get area between the provided interval.  This will multiply computeAreaNoUnitConversion by 60, making
     *  it into seconds unit.
     * @param t_start start position
     * @param t_end end position
     */
    double computeAreaSecondsUnit(double t_start, double t_end, ComputeAreaMethod method) const;

    double computeAreaFast(double t_start, double t_end, ComputeAreaMethod method) const;

    /*!
     * @brief Get the time with the heighest intensity and time within the provided window. If domain not defined within interval, it returns {t_start, 0}.
     * @return point2d = {time,intensity}
     */
    point2d getMaxTimeIntensity(double t_start, double t_end) const;

    point2d getMinTimeIntensity(double t_start, double t_end) const;

    double computeSumIntensities(double startMz, double endMz) const;

    /*!
     * \brief get the maximum and minimum point within the given range
     * \param t_start
     * \param t_end
     * \param maxPoint
     * \param minPoint
     * \return true if found, false otherwise.
     */
    bool getMinMaxPoints_slow(double t_start, double t_end, point2d * minPoint, point2d * maxPoint) const;

    bool getMinMaxPointsIndex(pt2idx i_start, pt2idx i_end, point2d & maxPoint, point2d & minPoint) const;


    /*!
     * @brief Get the time with the heighest intensity and time over whole domain. If none are found, it returns {0, 0}.
     * @return point2d = {time,intensity}
     */
    point2d getMaxPoint() const;

    /*!
     * @brief Get the time with the lowest intensity and time over whole domain. If none are found, it returns {0, 0}.
     * @return point2d = {time,intensity}
     */
    point2d getMinPoint() const;

    /*!
     * @brief the the index of the best observed intense peak.
     */
    int getMaxTimeIntensityIndex(const std::vector<double> & timeList) const;

    /*!
     * \brief findIntersectionPoints finds all intersection points and index locations for the given line
     * \param line input line to test against; any intersection not within this line are ignored.
     * \param intersectPointList output list of intersecting points.
     * \param intersectStartIdxList output of intersecting index starting location. Length will be the same as @intersectPointList
     */
    void findIntersectionPoints(const line2d & line, point2dList & intersectPointList, point2dIdxList & intersectStartIdxList,
                                long flags=kIntersectionFlagAllPoints,
                                std::vector<long> * intersectFlagList = NULL) const;


    /*!
     * \brief getIntensityListFloat  get array of intensity values with the given interval.  This is used for e.g. alignment
     * \param t1 input start position
     * \param t2 input end position
     * \param actual_t1 output actual start position
     * \param actual_t2 outputactual end position
     * \param intensityArray output the intensity array
     */
    void getIntensityListFloat(double t1, double t2, double * actual_t1, double * actual_t2, std::vector<float> & intensityArray);

    ///////////////////////////////////
    // Algorithms
    ///////////////////////////////////
    enum FindPeakOption {FindMin=0, FindMax};
    Err findPeaksIndex(double tol, FindPeakOption findMax, point2dIdxList & idxList,int start_index=-1, int end_index=-1) const;

    /*!
     * \brief findPeaksRelativeTolerance wrapper around findPeaksIndex to provide @relativeTolerance as an abstracted tolerance
     * \param findMax kFindMin or kFindMax
     * \param apexIdxList output list of critical point index list
     * \param relativeTolerance noise tolerance used when finding critical point; relative to the e.g. 10th maximum height
     * \param kth_max maximum value used to compute the absolute tolerance using @relativeTolerance; we do not want to use the heighest point as that be not very interesting data.
     * \return Err
     */
    Err findPeaksRelativeTolerance(FindPeakOption findMax, point2dIdxList & apexIdxList, double relativeTolerance, int kth_max = 10) const;

    double getNthCriticalMinimaValue(int n) const;

    double getNthCriticalMaximaValue(int n) const;

    /*!
     * \brief isSortedLess
     * \return true if sorted and in ascending order
     */
    bool isSortedAscendingX() const;

    Err makeResampledPlot(double samplingInterval, PlotBase & outPlot) const;

    Err makeResampledPlotTargetSize(int target_number_of_points, PlotBase & outPlot) const;
    /*!
     * \brief generate output sample points with exactly maxNumberOfPoints
     * \param maxNumberOfPoints number of points to create
     * \param outPlot
     * \return
     */
    Err makeResampledPlotMaxPoints(int maxNumberOfPoints, point2dList & outPoints) const;

    /*! 
        \brief Checks if the plot is uniformly sampled. 
        \param negligibleRatio Maximum relative difference in sample duration.
        \return true if the plot is uniformly sampled, false otherwise.
    */
    bool isUniform(double negligibleRatio = 0.01) const;

    /*!
     * \brief offSetPointListBy Change m_pointList y values with the given plist values.  E.g. use this offset signal from baseline.
     * \param plist used to offset this signal; domain outside of plist is considered zero.
     */
    void subtractBy(const point2dList & plist);

    void applyRandomYScale(double scaleAmount);

    Err averageSampleWidth(double * averageWidth);

    /*!
     * \brief sum all the y points between the given domain region
     * \param t_start x start value
     * \param t_end x end value
     * \param inclusive for closed boundary, i.e. t_start <= x <= t_end
     * \return sum of the y values
     */
    double getYSum(double t_start, double t_end, bool inclusive) const;

    void getPoints(double t_start, double t_end, bool inclusive, point2dList & plist) const;

    /*!
     * \brief The CentroidAlgorithmMethod enum
     * CentroidNaiveMaxValue                  chooses the local maximum value -- poor, but robust
     * CentroidWeightedAverage                weight the two neighboring to find subpixel value -- based on absolute intensity -- dont use; only keeping until we verify and test
     * CentroidWeightedAverage_RelativeWeight weight the two neighboring to find subpixel value -- based on relative intensity -- best
     * TODO: Add CentroidAlgorithmQuadratic -- should be best but may lead to robustness issue with noise
     */
    enum CentroidAlgorithmMethod {
        CentroidNaiveMaxValue,
        CentroidWeightedAverage,
        CentroidWeightedAverage_RelativeWeight
    };

    /*!
     * \brief makeCentroidedPoints assumes the class contains profile data.
     * \param points is the resulting ouput centroided points
     * \param method is the centroiding algorithm
     */
    void makeCentroidedPoints(point2dList * points, CentroidAlgorithmMethod method = CentroidWeightedAverage) const;

    QString & name() {
        return m_name;
    }
    const QString & name() const {
        return m_name;
    }

    /*!
     * Make sure to call this only when certain that the points are sorted by x in ascending order.
     */
    void setIsSortedAscendingX_noErrorCheck(bool val) {
        m_isSortedAscendingX = val;
    }

protected:
    //! Let's prevent direct editing of the point list outside of this class or derived.
    point2dList & getPointListProtected() {
        return m_pointList;
    }

    /*!
     * @brief Find the min and max point within the given range. The bound point
     * is also considered with interpolation. Note that the min and max points
     * do not have to be a local min or max.
     * TODO: Consider renaming this as lowest and highest point to
     * disambiguate between critical points and non-critical points.
     * @return true if valid points are found
     */
    bool getMinMaxPoints(double t_start, double t_end, point2d * minPoint, point2d * maxPoint) const;

protected:
    point2dList m_pointList;
    bool m_isSortedAscendingX;
    QString m_name;  ///used for labeling the given plot.

private:
    void _calibratePoints(Calibration &calibration) {
        for (unsigned int i = 0; i < m_pointList.size(); i++) {
            point2d & point = m_pointList[i];
            point.setX(calibration.calibrate(point.x()));
        }
    }
};

/*!
 * \brief The PlotLinearTransform class handles linear transformation, such as x translation.
 * The idea behind this class is to hide the linear transformation as much as possible.
 * We will use the main point list (m_pointList) to contain the transformed data and cache the original data
 * as m_rawPointList.  Keep in mind that when saving data, we need to save the raw points and the translate, and discard the m_pointList.
 * Non-linear transformation (the warping) will be done outside, where its input will be two plots.
 */
class PMI_COMMON_CORE_MINI_EXPORT PlotLinearTransform : public PlotBase
{
public:
    PlotLinearTransform()
        : PlotBase(),
        m_translate(0,0),
        m_interestTimeStart(-1),
        m_interestTimeEnd(-1)
    {
    }

    PlotLinearTransform(const point2dList & list)
        : PlotBase(list),
          m_translate(0,0),
          m_interestTimeStart(-1),
          m_interestTimeEnd(-1)
    {
    }

    virtual ~PlotLinearTransform() {
    }

    virtual void clear() {
        PlotBase::clear();
        m_translate = point2d(0,0);
        m_interestTimeStart = -1;
        m_interestTimeEnd = -1;
    }

    /*!
     * \brief setTranslate translate each member point by given value; this will consider previous translate value and undo this value before hand.
     * \param new_translate
     */
    void setTranslate(const point2d & new_translate) {
        point2d delta = new_translate - m_translate;
        for (unsigned int i = 0; i < m_pointList.size(); i++) {
            const point2d & p = m_pointList[i];
            m_pointList[i] = p + delta;
        }
        m_translate = new_translate;
    }

    point2d getTranslate() const {
        return m_translate;
    }

    void makeUntranslatedPointList(point2dList & pointList) const {
        pointList.resize(m_pointList.size());
        for (unsigned int i = 0; i < m_pointList.size(); i++) {
            pointList[i] = m_pointList[i] - m_translate;
        }
    }

    void setPointListAndTranslate(const point2dList & pointList) {
        m_pointList.resize(pointList.size());
        for (unsigned int i = 0; i < pointList.size(); i++) {
            m_pointList[i] = pointList[i] + m_translate;
        }
    }

    void setPointListNoTranslate(const point2dList & pointList) {
        m_pointList = pointList;
    }

    /*!
     * \brief getInterestTimeInterval get the x-range of the time interval of interest.
     * This will first collect the true x-range and then shrink it by the interest interval.
     * \param t1 start time plus ignore time
     * \param t2 end time minus ignore time
     * \return true if interval is valid (t1 < t2), false otherwise
     */
    bool getInterestTimeInterval(double * t1, double *t2) const {
        getXBound(t1,t2);
        //if interest time not initialized, do not use
        if (!isValidInterestTime())
            return (*t1 <= *t2);

        if (*t1 < m_interestTimeStart)
            *t1 = m_interestTimeStart;
        if (*t2 > m_interestTimeEnd)
            *t2 = m_interestTimeEnd;

        return (*t1 <= *t2);
    }

    void setInterestTimeInterval(double interestStart, double interestEnd) {
        m_interestTimeStart=interestStart;
        m_interestTimeEnd=interestEnd;
    }

    bool isValidInterestTime() const {
        if (m_interestTimeStart == -1 && m_interestTimeEnd == -1)
            return false;
        return true;
    }

    QVariant getInterestTimeStart() const {
        if (isValidInterestTime())
            return m_interestTimeStart;
        return QVariant();
    }

    QVariant getInterestTimeEnd() const {
        if (isValidInterestTime())
            return m_interestTimeEnd;
        return QVariant();
    }

private:
    point2d m_translate;  //! the linear shift in x & y directions
    double m_interestTimeStart; //! E.g. ignore first few minutes; hint used to help algorithms
    double m_interestTimeEnd; //! E.g. ignore last few minutes; hint used to help algorithms
};

/*!
 * \brief GetIndexLessOrEqual returns index less than or equal to xloc
 * \param plist sort list of points
 * \param xloc the x-position wanted
 * \return the index when found and -1 if not found.
 */
PMI_COMMON_CORE_MINI_EXPORT pt2idx GetIndexLessOrEqual(const point2dList & plist, double xloc,
                                                  bool outOfBoundsOnRightReturnNegativeTwo);

/*!
 * \brief findGreaterOrEqual returns first index greater than or equal to xloc (linear search)
 * \param plist sorted list of points
 * \param xloc the x-position wanted
 * \return the index when found and plist.end() if not found.
 */
PMI_COMMON_CORE_MINI_EXPORT point2dList::const_iterator findGreaterOrEqual(const point2dList &plist, double xloc, pt2idx searchStartIdx = 0);

/*!
 * \brief getIndexLessThanValLinear returns last index that is less than xloc
 * \param plist sorted list of points
 * \param xloc the x-position wanted
 * \return the index when found and -1 if not found.
 */
PMI_COMMON_CORE_MINI_EXPORT pt2idx GetIndexLessThanValLinear(const point2dList & plist, double xloc, const pt2idx searchStartIdx = 0);

PMI_COMMON_CORE_MINI_EXPORT Err saveDataToFile(const QString & filename, const point2dList & plist);

PMI_COMMON_CORE_MINI_EXPORT pt2idx findClosestIndex(const point2dList & plist, double xloc);

PMI_COMMON_CORE_MINI_EXPORT std::vector<pt2idx> findClosestIndexList(const point2dList & plist,
                                                                const std::vector<double> & xlocList);

PMI_COMMON_CORE_MINI_EXPORT point2d getMaxPoint(const point2dList & pointList);

PMI_COMMON_CORE_MINI_EXPORT point2d getMinPoint(const point2dList & pointList);

PMI_COMMON_CORE_MINI_EXPORT double getYSumAll(const point2dList & plist);

PMI_COMMON_CORE_MINI_EXPORT Err getBestCriticalPositions(const PlotBase & plot, PlotBase::FindPeakOption critial,
                                                    double startTime, double endTime, pt2idx * mostMidPointIdx,
                                                    pt2idx * mostValuePointIdx);

PMI_COMMON_CORE_MINI_EXPORT Err getBestMinSplitPosition(const PlotBase & plot, double startTime, double endTime, bool preferMidPointInsteadOfMaxPoint,
                                                   point2d & minPoint);

PMI_COMMON_CORE_MINI_EXPORT Err getBestMaxPositionByValue(const PlotBase & plot, double startTime, double endTime, bool preferMidPointInsteadOfMaxPoint,
                                                     point2d & maxPoint);

inline void scalePoints(const point2d & scaleP, point2dList & plist) {
    for (unsigned int i = 0; i < plist.size(); i++) {
        plist[i].rx() *= scaleP.x();
        plist[i].ry() *= scaleP.y();
    }
}

PMI_COMMON_CORE_MINI_EXPORT Err byteArrayToPlotBase(const QByteArray & xyba, PlotBase * plot, bool sortByX,
                                               Calibration *calibration = NULL);
PMI_COMMON_CORE_MINI_EXPORT Err byteArrayToPlotBase(const QByteArray & xba, const QByteArray & yba,
                                               PlotBase * plot, bool sortByX, Calibration *calibration = NULL);
PMI_COMMON_CORE_MINI_EXPORT Err byteArrayToPlotBase(const QByteArray & xba, const QByteArray & yba,
                                               point2dList & plist, bool sortByX, Calibration *calibration = NULL);
PMI_COMMON_CORE_MINI_EXPORT Err byteArrayToDoubleVector(const QByteArray & xba, std::vector<double> & list,
                                                   bool sortByX);
PMI_COMMON_CORE_MINI_EXPORT Err byteArrayToFloatVector(const QByteArray &xba, std::vector<float> & list,
                                                  bool sortByX);

/*!
 * \brief pointsToByteArray_DoubleFloat
 * \param plist input
 * \param xlist output: workspace buffer required in the same space as the byte array
 * \param ylist output: workspace buffer required in the same space as the byte array
 * \param xba output: bytearray
 * \param yba output: bytearray
 */
PMI_COMMON_CORE_MINI_EXPORT void pointsToByteArray_DoubleFloat_careful(const point2dList & plist,
                                   std::vector<double> & xlist, std::vector<float> & ylist,
                                   QByteArray & xba, QByteArray & yba);

PMI_COMMON_CORE_MINI_EXPORT void pointsToByteArray_DoubleDouble_careful(const point2dList & plist,
                                   std::vector<double> & xlist, std::vector<double> & ylist,
                                   QByteArray & xba, QByteArray & yba);

PMI_COMMON_CORE_MINI_EXPORT void pointsToByteArray_Double_careful(const point2dList & plist,
                                   std::vector<double> & xylist,
                                   QByteArray & xyba);

PMI_COMMON_CORE_MINI_EXPORT bool isSortedAscendingX(const point2dList & list);

PMI_COMMON_CORE_MINI_EXPORT void getPointsInBetween(const point2dList & plist, double t_start, double t_end,
                                               point2dList & outList, bool inclusive);

PMI_COMMON_CORE_MINI_EXPORT void removeLowIntensePeaks(point2dList & plist, int max_points);

PMI_COMMON_CORE_MINI_EXPORT int removeZeroOrNegativeIntensePeaks(point2dList & plist);

PMI_COMMON_CORE_MINI_EXPORT int removeAllButKIntensePoint(point2dList &plist, int numberOfPointsK);

PMI_COMMON_CORE_MINI_EXPORT Err get_best_observed(const point2dList & plist, double calcMr,
                                             int charge, const double * ppm_tolerance,
                                             double * calcMz_iso, point2d * obsMz_iso,
                                             double * best_observed_mono_mz);

PMI_COMMON_CORE_MINI_EXPORT double getYMaxBetween(const point2dList & plist, double t_start, double t_end,
                                             bool inclusive);

///returns distance to closest point. returns -1 if plist is empty
PMI_COMMON_CORE_MINI_EXPORT double distanceToClosestPoint(const point2dList & plist, double xloc,
                                                     pt2idx * idx = NULL);

PMI_COMMON_CORE_MINI_EXPORT void SortPointListByX(point2dList & plist, bool doAscending);

PMI_COMMON_CORE_MINI_EXPORT bool isSimilar_pointbypoint(const PlotBase & a, const PlotBase & b, double fudge);

/*!
 * @brief Reduce number of points within a give x-range by picking the most intense point within each interval.  The output should be sorted, but not tested.
 * @x_scale of 1.0 will keep at most one per each x-range of 1.  Value of 0.1 will keep at most one per x-range of 10.  2 will keep at most one per x-range of 2.
 */
PMI_COMMON_CORE_MINI_EXPORT void reduceDensityInX(const point2dList & plist, double x_scale, point2dList * outList);


PMI_COMMON_CORE_MINI_EXPORT PlotBase::ComputeAreaMethod getDeamQuantMethod();

_PMI_END

Q_DECLARE_METATYPE(pmi::PlotBase) //!< Allows to store PlotBase in QVariant

#endif
