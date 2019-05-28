/*
 * Copyright (C) 2012-2014 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 */

#ifndef __PLOT_BASE_UTILS_H__
#define __PLOT_BASE_UTILS_H__

#include "PlotBase.h"

_PMI_BEGIN

PMI_COMMON_CORE_MINI_EXPORT Err gaussianSmooth(PlotBase & plotbase, double sigma);

/**
* \brief Identifies peaks and their apexes in the signal \a plot 
* 
* \a plot suppose to contain time, intensity points.

* \a options suppose to contain following columns with their values:
*   kpeak_interval_time_min
*   kpeak_tol_max
*   kpeak_tol_min
*
*   Example of initialization:
*
*   RowNode options;
*   rn::setOption(options, kpeak_interval_time_min, 0.5);
*   rn::setOption(options, kpeak_tol_max, 0.005);
*   rn::setOption(options, kpeak_tol_min, 0.0005);
*/
PMI_COMMON_CORE_MINI_EXPORT Err makePeaks2(const PlotBase &plot, const RowNode &options,
                                           point2dIdxList &startIdxList, point2dIdxList &endIdxList,
                                           point2dIdxList &apexIdxList,
                                           QString *tempFolderPtr = nullptr);

/**
 * @brief This function overloads \overload makePeaks2. It does not use RowNode as an argument.
 * @param peakIntervalTimeMin peak minimum width
 * @param peakTolMin peak tolerance min
 * @param peakTolMax peak tolerance max
 */
PMI_COMMON_CORE_MINI_EXPORT Err makePeaks2(const PlotBase &plot,
                                           double peakIntervalTimeMin,
                                           double peakTolMin, double peakTolMax,
                                           point2dIdxList &startIdxList, point2dIdxList &endIdxList,
                                           point2dIdxList &apexIdxList,
                                           QString *tempFolderPtr = nullptr);

PMI_COMMON_CORE_MINI_EXPORT Err makePeaksForInterval(const PlotBase &plot, const RowNode &options,
                                                     const PeakInterval &inputInterval,
                                                     QList<PeakInterval> &intervalList,
                                                     QString *tempFolderPtr = nullptr);

/**
 * @brief This function overloads \overload makePeaksForInterval. It does not use RowNode as an argument.
 * @param peakIntervalTimeMin peak minimum width
 * @param peakTolMin peak tolerance min
 * @param peakTolMax peak tolerance max
 */
PMI_COMMON_CORE_MINI_EXPORT Err makePeaksForInterval(const PlotBase & plot, double peakIntervalTimeMin,
                                                     double peakTolMin, double peakTolMax,
                                                     const PeakInterval & inputInterval,
                                                     QList<PeakInterval> & intervalList,
                                                     QString * tempFolderPtr = nullptr);


_PMI_END

#endif
