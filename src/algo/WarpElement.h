/*
 * Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author  : Michael Georgoulopoulos mgeorgoulopoulos@gmail.com
 */

#ifndef __WARP_ELEMENT_H__
#define __WARP_ELEMENT_H__

#include <pmi_common_ms_export.h>

#include <pmi_core_defs.h>
#include <common_errors.h>
#include <common_math_types.h>

_PMI_BEGIN

/*! \brief Represents a pair of m/z and intensity values, in the context of a MS file scan. */
struct PMI_COMMON_MS_EXPORT MzIntensityPair
{
    MzIntensityPair();

    double intensity;
    double mz;

    static bool compareIntensity(const MzIntensityPair &a, const MzIntensityPair &b)
    {
        return a.intensity < b.intensity;
    }

    static bool reverseCompareIntensity(const MzIntensityPair &a, const MzIntensityPair &b)
    {
        return b.intensity < a.intensity;
    }

    bool isEmpty() const {
        return mz <= 0.0;
    }
};

/*! \brief Collection of mzIntensityPairs. Can be seen as the 'y' value of an intensity plot, but with a vector of intensities. */
struct PMI_COMMON_MS_EXPORT WarpElement
{
    /*! Hard-coded maximum mz-intensity pair count */
    static constexpr int maxMzIntensityPairCount = 4;

    WarpElement();

    /*! Returns true if legacy (intensity only, mz not used) */
    int isLegacy() const
    {
        return mzIntensityPairs[0].isEmpty();
    }
    int mzIntensityPairCount() const;


    MzIntensityPair mzIntensityPairs[maxMzIntensityPairCount];

    /*! \brief Returns intensity. The same function can be used to get intensity regardless of mode
     * (legacy, 1,2,3 or 4 mz/intensity pairs. */
    double intensity() const;

    /*! \brief Minimum mz value. */
    double minMz() const;

    /*! \brief Maximum mz value. */
    double maxMz() const;

    /*! \brief Mz value of the greatest intensity. */
    double dominantMz() const;

    /*! \brief Intensity of dominant m/z. */
    double dominantMzIntensity() const;

    WarpElement &scale(double factor);

    static double averageIntensity(const std::vector<WarpElement> &v);

    /*! 
        \brief Interpolate two WarpElements.
        \param ppm When two  mz/int pairs are within given ppm, they will be linearly interpolated and added to resulting WarpElement.
    */
    static WarpElement interpolate(const WarpElement &a, const WarpElement &b, double ppm, double t);

    /*! \brief Similarity assessment of two WarpElements. Negative is bad, zero is good. */
    static double scoreFunction(const WarpElement &a, const WarpElement &b, double ppm);


    static Err fromScan(const point2dList &scan, int mzIntensityPairCount, double mzWindow, WarpElement *warpElement);
    
    static bool compareIntensity(const WarpElement &a, const WarpElement &b) {
        return a.intensity() < b.intensity();
    }

    /*! \brief Create an intensity-only warp element */
    static WarpElement legacyWarpElement(const double intensity) {
        WarpElement ret;
        ret.mzIntensityPairs[0].intensity = intensity;
        return ret;
    }

    /*! \brief Returns a list of strings, to be used as labels for CSV export. */
    static QString csvLabels(int mzIntensityPairCount);

    /*! \brief Returns a list of values which form one row in the CSV file that corresponds to this WarpElement. */
    QString csvValues() const;
};

typedef std::vector<WarpElement> WarpElementList;

/*! \brief In the 2D warp context, this is a point2d, with the difference that it does not contain just one y value, but a \a WarpElement. */
struct PMI_COMMON_MS_EXPORT TimedWarpElement
{
    TimedWarpElement();
    TimedWarpElement(double timeInMinutes, const WarpElement &warpElement);

    double timeInMinutes;
    WarpElement warpElement;

    static bool compareIntensity(const TimedWarpElement &a, const TimedWarpElement &b) {
        return WarpElement::compareIntensity(a.warpElement, b.warpElement);
    }
};

/*! \brief This is analogous to PlotBase, but for \a TimedWarpElement instead of point2d. */
class PMI_COMMON_MS_EXPORT TimedWarpElementList : public std::vector<TimedWarpElement>
{
public:
    /*! \brief Returns start and end time. */
    Err getXBound(double *startTime, double *endTime) const;

    /*! 
        \brief Evaluates time \a timeInMinutes. 
        \a ppm to determine if a pair of intensities matches.
    */
    WarpElement evaluate(double ppm, double timeInMinutes) const;

    /*! \brief Produces a uniformly resampled WarpElement list from this plot. */
    Err makeResampledPlotMaxPoints(int maxNumberOfPoints, double ppm, TimedWarpElementList &outPoints) const;

    /*! \brief Checks if the values in this plot are x-sorted. */
    bool isSortedAscendingTime() const;

    /*! \brief Checks if the plot is uniformly sampled. */
    bool isUniform(double negligibleRatio=0.01) const;
};

PMI_COMMON_MS_EXPORT TimedWarpElement getMaxPoint(const TimedWarpElementList &list);
PMI_COMMON_MS_EXPORT std::vector<int> findClosestIndexList(const TimedWarpElementList & plist, const std::vector<double> & xlocList);
PMI_COMMON_MS_EXPORT void getPointsInBetween(const TimedWarpElementList &plist, double t_start, double t_end, TimedWarpElementList &outList, bool inclusive);

_PMI_END

#endif // __WARP_ELEMENT_H__