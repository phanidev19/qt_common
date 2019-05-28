/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author  : Michael Georgoulopoulos mgeorgoulopoulos@gmail.com
*/

#include "WarpElement.h"

#include <common_constants.h>
#include <MathUtils.h>

_PMI_BEGIN

MzIntensityPair::MzIntensityPair()
    :intensity(0.0)
    , mz(0.0)
{
}

WarpElement::WarpElement()
{
    // Set everything to zero
    for (int i = 0; i < maxMzIntensityPairCount; i++) {
        mzIntensityPairs[i] = MzIntensityPair();
    }
}

int WarpElement::mzIntensityPairCount() const
{
    int i = 0;
    for (i = 0; i < maxMzIntensityPairCount; i++) {
        if (mzIntensityPairs[i].isEmpty()) {
            break;
        }
    }
    return i;
}

double WarpElement::intensity() const
{
    if (isLegacy()) {
        return mzIntensityPairs[0].intensity;
    }

    double sum = 0.0;
    for (int i = 0; i < maxMzIntensityPairCount; i++) {
        if (mzIntensityPairs[i].mz == 0.0) {
            break;
        }
        sum += mzIntensityPairs[i].intensity;
    }
    return sum;
}

double WarpElement::minMz() const
{
    double ret = mzIntensityPairs[0].mz;
    for (int i = 0; i < maxMzIntensityPairCount; i++) {
        if (mzIntensityPairs[i].isEmpty()) {
            // Unused. Trailing zeroes from now on
            break;
        }
        ret = std::min(ret, mzIntensityPairs[i].mz);
    }
    return ret;
}

double WarpElement::maxMz() const
{
    double ret = mzIntensityPairs[0].mz;
    for (int i = 0; i < maxMzIntensityPairCount; i++) {
        if (mzIntensityPairs[i].isEmpty()) {
            // Unused. Trailing zeroes from now on
            break;
        }
        ret = std::max(ret, mzIntensityPairs[i].mz);
    }
    return ret;
}

double WarpElement::dominantMz() const
{
    // mz/int pairs are sorted by intensity
    return mzIntensityPairs[0].mz;
}

double WarpElement::dominantMzIntensity() const
{
    return mzIntensityPairs[0].intensity;
}

WarpElement &WarpElement::scale(double factor)
{
    if (isLegacy()) {
        mzIntensityPairs[0].intensity *= factor;
        return *this;
    }

    for (int i = 0; i < maxMzIntensityPairCount; i++) {
        if (mzIntensityPairs[i].isEmpty()) {
            // Unused. Trailing zeroes from now on.
            break;
        }
        mzIntensityPairs[i].intensity *= factor;
    }

    return *this;
}

double WarpElement::averageIntensity(const std::vector<WarpElement> &v)
{
    double average = 0.0;

    if (v.empty()) {
        return 0.0;
    }
    for (const WarpElement &i : v) {
        average += i.intensity();
    }

    average /= static_cast<double>(v.size());

    return average;
}

static inline bool withinPpm(double mzA, double mzB, double ppm)
{
    double mzMargin = std::max(mzA, mzB) * ppm * ONE_MILLIONTH;
    double diff = std::abs(mzB - mzA);
    return diff <= mzMargin;
}

WarpElement WarpElement::interpolate(const WarpElement &a, const WarpElement &b, double ppm,
    double t)
{
    if (a.isLegacy() || b.isLegacy()) {
        // default to legacy (intensity only)
        WarpElement ret;
        ret.mzIntensityPairs[0].intensity
            = MathUtils::lerp(a.mzIntensityPairs[0].intensity, b.mzIntensityPairs[0].intensity, t);
        return ret;
    }

    // otherwise, nearest neighbour
    if (t < 0.5) {
        return a;
    }

    return b;
}

double WarpElement::scoreFunction(const WarpElement &a, const WarpElement &b, double ppm)
{
    if (a.isLegacy() || b.isLegacy()) {
        // Original objective func - return diff squared
        double ret = a.mzIntensityPairs[0].intensity - b.mzIntensityPairs[0].intensity;
        ret = -ret * ret;
        return ret;
    }

    // Match all mz/int pairs from a against all from b
    int matchMapAB[maxMzIntensityPairCount];
    int matchMapBA[maxMzIntensityPairCount];

    const int UNMATCHED = -1;
    const int USED = -2;

    for (int i = 0; i < maxMzIntensityPairCount; i++) {
        matchMapAB[i] = UNMATCHED;
        matchMapBA[i] = UNMATCHED;
    }

    int pairCount = std::min(a.mzIntensityPairCount(), b.mzIntensityPairCount());

    for (int indexA = 0; indexA < pairCount; indexA++) {
        for (int indexB = 0; indexB < pairCount; indexB++) {
            if (withinPpm(a.mzIntensityPairs[indexA].mz, b.mzIntensityPairs[indexB].mz, ppm)) {
                matchMapAB[indexA] = indexB;
                matchMapBA[indexB] = indexA;
            }
        }
    }

    // sum square diffs
    double ret = 0.0;

    const double matchedWeight = 1.0;
    const double unmatchedWeight = 0.5;

    // Sum square diffs of mz/int pairs of a
    for (int i = 0; i < pairCount; i++) {
        double weight = unmatchedWeight;
        double intensity1 = a.mzIntensityPairs[i].intensity;
        double intensity2 = 0.0;

        // If we have found a match, set correct intensity and weight
        int index2 = matchMapAB[i];
        if (index2 >= 0) {
            weight = matchedWeight;
            intensity2 = b.mzIntensityPairs[index2].intensity;

            // Also remember that we've used this so we don't sum it again while examining b pairs
            // below
            matchMapBA[index2] = USED;
        }

        // Sum sq diff
        double diff = intensity2 - intensity1;
        ret -= weight * diff * diff;
    }

    // Sum square diffs of mz/int pairs of b
    for (int i = 0; i < pairCount; i++) {
        int index1 = matchMapBA[i];
        if (index1 == USED) {
            continue;
        }

        double weight = unmatchedWeight;
        double intensity2 = b.mzIntensityPairs[i].intensity;
        double intensity1 = 0.0;
        if (index1 >= 0) {
            intensity1 = a.mzIntensityPairs[index1].intensity;
            weight = matchedWeight;
        }
        double diff = intensity2 - intensity1;
        ret -= weight * diff * diff;
    }

    return ret;
}

static Err extractMaxima(const point2dList &plot, int maximaCount, double mzWindow,
    point2dList *maxima)
{
    Err e = kNoErr;

    maxima->clear();

    if (maximaCount <= 0) {
        return e;
    }

    maxima->resize(0);
    maxima->reserve(maximaCount);

    point2dList sortedPlot = plot;

    auto reverseCompareY = [](const point2d &a, const point2d &b) { return b.y() < a.y(); };
    std::sort(sortedPlot.begin(), sortedPlot.end(), reverseCompareY);

    // ignore other maxima close to the already extracted
    std::vector<double> ignoreStart;
    std::vector<double> ignoreEnd;

    // find maxima
    for (const point2d &p : sortedPlot) {
        bool ignorePoint = false;
        for (int i = 0; i < static_cast<int>(ignoreStart.size()); i++) {
            if (p.x() >= ignoreStart[i] && p.x() <= ignoreEnd[i]) {
                ignorePoint = true;
                break;
            }
        }
        if (ignorePoint) {
            continue;
        }

        maxima->push_back(p);

        if (static_cast<int>(maxima->size()) >= maximaCount) {
            break;
        }

        // ignore other maxima close to the already added
        ignoreStart.push_back(p.x() - mzWindow);
        ignoreEnd.push_back(p.x() + mzWindow);
    }

    // pad with zeroes, so that we guarantee a 'maximaCount' size list returned.
    while (static_cast<int>(maxima->size()) < maximaCount) {
        maxima->push_back(point2d(0.0, 0.0));
    }

    return e;
}

Err WarpElement::fromScan(const point2dList &scan, int mzIntensityPairCount, double mzWindow,
    WarpElement *warpElement)
{
    Err e = kNoErr;

    *warpElement = WarpElement();

    if (scan.empty()) {
        return e;
    }

    // Handle legacy
    if (mzIntensityPairCount <= 0) {
        double basePeak = std::max_element(scan.begin(), scan.end(), point2d_less_y)->y();
        *warpElement = legacyWarpElement(basePeak);
        return e;
    }

    // get n dominant mz/intensity pairs
    point2dList maxima;

    if (extractMaxima(scan, mzIntensityPairCount, mzWindow, &maxima) != kNoErr) {
        return kNoErr;
    }

    // return element
    for (int i = 0; i < mzIntensityPairCount && i < static_cast<int>(maxima.size())
        && i < WarpElement::maxMzIntensityPairCount;
        i++) {
        warpElement->mzIntensityPairs[i].mz = maxima[i].x();
        warpElement->mzIntensityPairs[i].intensity = maxima[i].y();
    }

    return e;
}

QString WarpElement::csvLabels(int mzIntensityPairCount)
{
    QStringList labels;
    for (int i = 0; i < mzIntensityPairCount; i++) {
        labels.push_back(QStringLiteral("mz %1").arg(i));
        labels.push_back(QStringLiteral("int %1").arg(i));
    }
    return labels.join(",");
}

QString WarpElement::csvValues() const
{
    QStringList values;
    int pairCount = mzIntensityPairCount();
    for (int i = 0; i < pairCount; i++) {
        values.push_back(QString::number(mzIntensityPairs[i].mz));
        values.push_back(QString::number(mzIntensityPairs[i].intensity));
    }
    return values.join(",");
}

TimedWarpElement::TimedWarpElement()
    : timeInMinutes(0.0)
{
}

TimedWarpElement::TimedWarpElement(double timeInMinutes, const WarpElement &warpElement)
{
    this->timeInMinutes = timeInMinutes;
    this->warpElement = warpElement;
}

Err TimedWarpElementList::getXBound(double *startTime, double *endTime) const
{
    Err e = kNoErr;

    Q_ASSERT(isSortedAscendingTime());
    *startTime = *endTime = 0.0;
    if (empty()) {
        return e;
    }

    *startTime = front().timeInMinutes;
    *endTime = back().timeInMinutes;

    return e;
}

WarpElement TimedWarpElementList::evaluate(double ppm, double timeInMinutes) const
{
    Q_ASSERT(isSortedAscendingTime());

    // handle trivials
    if (size() == 1) {
        return front().warpElement;
    }

    if (timeInMinutes <= front().timeInMinutes) {
        return front().warpElement;
    }

    if (timeInMinutes >= back().timeInMinutes) {
        return back().warpElement;
    }

    int start = 0;
    int end = static_cast<int>(size());

    // subdivide
    while (end - start >= 2) {
        int m = (start + end) / 2;

        double mt = (*this)[m].timeInMinutes;

        if (timeInMinutes < mt) {
            end = m;
        }
        else {
            start = m;
        }
    }

    Q_ASSERT(end == start + 1);

    // interpolate
    double startTime = (*this)[start].timeInMinutes;
    double endTime = (*this)[end].timeInMinutes;
    double segmentDuration = endTime - startTime;

    if (segmentDuration <= 0.0) {
        return (*this)[start].warpElement;
    }

    double interpolationFactor = (timeInMinutes - startTime) / segmentDuration;
    return WarpElement::interpolate((*this)[start].warpElement, (*this)[end].warpElement,
        ppm, interpolationFactor);
}

Err TimedWarpElementList::makeResampledPlotMaxPoints(int pointCount, double ppm,
    TimedWarpElementList &outPoints) const
{
    Err e = kNoErr;

    // TODO: Assert(monotonic)

    outPoints.clear();

    // handle trivials
    if (pointCount <= 0) {
        return e;
    }

    if (empty()) {
        outPoints.resize(pointCount);
        return e;
    }

    if (size() == 1) {
        outPoints.resize(pointCount, front());
        return e;
    }

    if (pointCount <= 1) {
        outPoints.resize(pointCount, front());
        return e;
    }

    double startTime = 0.0;
    double endTime = 0.0;
    e = getXBound(&startTime, &endTime); ree;

    if (endTime <= startTime) {
        outPoints.resize(pointCount, front());
        return e;
    }

    double samplingPeriod = (endTime - startTime) / static_cast<double>(pointCount - 1);

    // walk indices A and B until we sample all points
    int a = 0;
    int b = 1;
    for (int i = 0; i < pointCount; i++) {
        double time = startTime + samplingPeriod * static_cast<double>(i);

        // Move b until we overshoot time
        for (; b < static_cast<int>(size()) && (*this)[b].timeInMinutes <= time; b++);
        if (b >= static_cast<int>(size())) {
            TimedWarpElement last;
            last.timeInMinutes = time;
            last.warpElement = back().warpElement;
            outPoints.push_back(last);
            continue;
        }
        a = b - 1;

        TimedWarpElement sampleA = (*this)[a];
        TimedWarpElement sampleB = (*this)[b];

        double duration = sampleB.timeInMinutes - sampleA.timeInMinutes;
        if (duration <= 0.0) {
            outPoints.push_back(TimedWarpElement(time, sampleA.warpElement));
            continue;
        }

        double lerpFactor = (time - sampleA.timeInMinutes) / duration;
        TimedWarpElement interpolatedSample = TimedWarpElement(
            time, WarpElement::interpolate(sampleA.warpElement, sampleB.warpElement, ppm, lerpFactor));

        outPoints.push_back(interpolatedSample);
    }

    return e;
}

TimedWarpElement getMaxPoint(const TimedWarpElementList &list)
{
    if (list.empty()) {
        return TimedWarpElement();
    }

    auto compareIntensity = [](const TimedWarpElement &a, const TimedWarpElement &b) {
        return a.warpElement.intensity() < b.warpElement.intensity();
    };
    return *std::max_element(list.begin(), list.end(), compareIntensity);
}

/*!
    \brief Returns the closest index to \a targetTime, doing a search with a given \a startIndex and \a direction.
    \return Closest index on success, -1 on error.
*/
static int findClosestIndexDirectional(const TimedWarpElementList &plist, double targetTime, int startIndex,
    int direction, double *diff)
{
    if (plist.empty()) {
        return -1;
    }

    startIndex = std::max(0, startIndex);
    startIndex = std::min(static_cast<int>(plist.size() - 1), startIndex);

    bool first = true;
    int ret = 0;
    for (int i = startIndex; i >= 0 && i < static_cast<int>(plist.size()); i += direction) {
        double currentDiff = std::abs(targetTime - plist[i].timeInMinutes);

        if (first || currentDiff < *diff) {
            first = false;
            *diff = currentDiff;
            ret = i;
        }
    }

    return ret;
}

static int findClosestIndex(const TimedWarpElementList &plist, double time, int hint)
{

    double leftDiff = 0.0;
    double leftIndex = findClosestIndexDirectional(plist, time, hint, -1, &leftDiff);

    double rightDiff = 0.0;
    double rightIndex = findClosestIndexDirectional(plist, time, hint, 1, &rightDiff);

    if (leftDiff < rightDiff) {
        return leftIndex;
    }

    return rightIndex;
}

std::vector<int> findClosestIndexList(const TimedWarpElementList &plist,
    const std::vector<double> &xlocList)
{
    std::vector<int> ret;
    int hint = 0;

    for (double time : xlocList) {
        hint = findClosestIndex(plist, time, hint);
        ret.push_back(hint);
    }

    return ret;
}

bool TimedWarpElementList::isSortedAscendingTime() const
{
    if (size() <= 1) {
        return true;
    }

    for (int i = 1; i < static_cast<int>(size()); i++) {
        if ((*this)[i].timeInMinutes < (*this)[i - 1].timeInMinutes) {
            return false;
        }
    }

    return true;
}

void getPointsInBetween(const TimedWarpElementList &plist, double t_start, double t_end,
    TimedWarpElementList &outList, bool inclusive)
{
    outList = plist;

    auto outPredicate = [t_start, t_end, inclusive](const TimedWarpElement &element) {
        if (inclusive) {
            return element.timeInMinutes < t_start || element.timeInMinutes > t_end;
        }
        return element.timeInMinutes <= t_start || element.timeInMinutes >= t_end;
    };
    outList.erase(std::remove_if(outList.begin(), outList.end(), outPredicate), outList.end());
}

/*! \brief Checks that a and b are almost equal (within tolerance). */
static inline bool fuzzyCompare(double a, double b, double tolerance)
{
    double diff = std::abs(b - a);
    double maximumAbs = std::max(std::abs(a), std::abs(b));

    // This is a very small number in terms of minutes (used here)
    const double verySmallNumber = 0.00001;
    maximumAbs = std::max(maximumAbs, verySmallNumber);
    
    return (diff / maximumAbs) <= tolerance;
}

/*! \brief Recursively splits in two and checks if the two segments are within tolerance. */
static bool isGloballyUniform(const TimedWarpElementList &plot, int startIndex, int endIndex,
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
        = plot[segmentAEnd - 1].timeInMinutes - plot[segmentAStart].timeInMinutes;
    double segmentBDuration
        = plot[segmentBEnd - 1].timeInMinutes - plot[segmentBStart].timeInMinutes;
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

bool TimedWarpElementList::isUniform(double negligibleRatio) const
{
    if (size() < 3) {
        return true;
    }

    // Check small errors don't stack up.
    if (!isGloballyUniform(*this, 0, static_cast<int>(size()), negligibleRatio)) {
        return false;
    }

    // Make sure that all durations between samples are within tolerance
    double minimumDuration = (*this)[1].timeInMinutes - (*this)[0].timeInMinutes;
    double maximumDuration = minimumDuration;
    for (int i = 2; i < static_cast<int>(size()); i++) {
        double duration = (*this)[i].timeInMinutes - (*this)[i - 1].timeInMinutes;
        minimumDuration = std::min(minimumDuration, duration);
        maximumDuration = std::max(maximumDuration, duration);
    }

    return fuzzyCompare(minimumDuration, maximumDuration, negligibleRatio);    
}

_PMI_END