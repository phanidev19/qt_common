/*
 * Copyright (C) 2012-2014 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 */

#include "PlotBaseUtils.h"
#include "Convolution1dCore.h"
#include "RowNodeOptionsCore.h"
#include "PmiQtCommonConstants.h"
#include <math_utils.h>
#include <vector>

using namespace std;

_PMI_BEGIN

inline bool interval_less(const PeakInterval & a, const PeakInterval & b) {
    return a.istart < b.istart;
}

Err gaussianSmooth(PlotBase & plotbase, double sigma) {
    Err e = kNoErr;
    double averageWidth = 1;
    double pixelSigma = 1;
    vector<double> f, kernel, f_out;

    point2dList & pointList = plotbase.getPointList();
    if (pointList.size() <= 1) {
        return e;
    }

    e = plotbase.averageSampleWidth(&averageWidth); eee;
    pixelSigma = sigma / averageWidth ;

    f.resize(pointList.size());
    for (unsigned int i = 0; i < pointList.size(); i++) {
        f[i] = pointList[i].y();
    }
    core::makeGaussianKernel(pixelSigma, kernel);

    core::conv(f, kernel, kBoundaryTypeCopyEnd, f_out);
    if (f_out.size() != pointList.size()) {
        e = kBadParameterError; eee;
    }
    for (unsigned int i = 0; i < pointList.size(); i++) {
        pointList[i].ry() = f_out[i];
    }

error:
    return e;
}

Err makePeaksForInterval(const PlotBase &plot, double peakIntervalTimeMin,
                         double peakTolMin, double peakTolMax,
                         const PeakInterval &inputInterval, QList<PeakInterval> &intervalList,
                         QString *tempFolderPtr)
{
    Err e = kNoErr;
    point2d maxPoint, minPoint;
    QList<PeakInterval> intervals_gaps;
    Q_UNUSED(tempFolderPtr);

    intervalList.clear();
    const point2dList & pointList = plot.getPointList();
    if (pointList.size() <= 0)
        return e;

    {
        double startTime = pointList[inputInterval.istart].x();
        double endTime = pointList[inputInterval.iend].x();
        double intervalTime = endTime - startTime;
        if (intervalTime < peakIntervalTimeMin) {
            // need to combine interval
            intervalList.push_back(inputInterval);
            return e;
        }
    }

    plot.getMinMaxPointsIndex(inputInterval.istart, inputInterval.iend, minPoint, maxPoint);
    double maxIntensity = std::max(std::abs(maxPoint.y()), std::abs(minPoint.y()));

    double peak_tol_max = maxIntensity * peakTolMax;
    double peak_tol_min = maxIntensity * peakTolMin;
//    debugCoreMini() << "startIndex, endIndex, startTime, endTime" << inputInterval.istart << "," << inputInterval.iend << "," << m_pointList[inputInterval.istart].x() << "," << m_pointList[inputInterval.iend].x();
//    debugCoreMini() << "ymax " << maxIntensity;
//    debugCoreMini() << "tolmax" << options.peakToleranceScaleForMax;
//    debugCoreMini() << "tolmin" << options.peakToleranceScaleForMin;

    point2dIdxList maxIdxList, minIdxList;

    //get critical max/min points
    e = plot.findPeaksIndex(peak_tol_max, PlotBase::FindMax, maxIdxList, inputInterval.istart, inputInterval.iend); eee;
    e = plot.findPeaksIndex(peak_tol_min, PlotBase::FindMin, minIdxList, inputInterval.istart, inputInterval.iend); eee;

    //for each max point, find the most adjacent min point and construct the peak_interval
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
            if (intervalList.size() > 0 && intervalList.back().istart == idx_start && intervalList.back().iend == idx_end) {
                //If the start & end interval already exists, consider changing the max index for this interval.
                if (pointList[intervalList.back().iapex].y() < pointList[max_idx].y())
                    intervalList.back().iapex = max_idx;
            } else {
                /*
                //This sure the peaks and created end-to-end; no gaps between peaks.
                if (intervalList.size() > 0) {
                    idx_start = intervalList.back().istart;
                }
                */
                //Create the peak
                intervalList.push_back(PeakInterval(idx_start,idx_end,max_idx));
            }
        }
    }

    //Add empty intervals
    if (intervalList.size() <= 0) {
        intervalList.push_back(inputInterval);
    } else {
        if (intervalList[0].istart != inputInterval.istart) {
            intervals_gaps.push_back(PeakInterval(inputInterval.istart, intervalList[0].istart));
        }
        if (intervalList.back().iend != inputInterval.iend) {
            intervals_gaps.push_back(PeakInterval(intervalList.back().iend, inputInterval.iend));
        }

        for (int i = 0; i < intervalList.size()-1; i++) {
            if (intervalList[i].iend != intervalList[i+1].istart) {
                intervals_gaps.push_back(PeakInterval(intervalList[i].iend, intervalList[i+1].istart));
            }
        }

        intervalList.append(intervals_gaps);
        qSort(intervalList.begin(), intervalList.end(), interval_less);
    }

error:
    return e;
}

Err makePeaksForInterval(const PlotBase & plot, const RowNode & options, const PeakInterval & input,
               QList<PeakInterval> & intervals, QString * tempFolderPtr)
{
    return makePeaksForInterval(plot,
                                rn::getOption(options, kpeak_interval_time_min).toDouble(),
                                rn::getOption(options, kpeak_tol_min).toDouble(),
                                rn::getOption(options, kpeak_tol_max).toDouble(),
                                input, intervals, tempFolderPtr);
}

Err makePeaks2(const PlotBase &plot,
               double peakIntervalTimeMin,
               double peakTolMin, double peakTolMax,
               point2dIdxList &startIdxList,
               point2dIdxList &endIdxList,
               point2dIdxList &apexIdxList,
               QString *tempFolderPtr)
{
    Err e = kNoErr;

    startIdxList.clear();
    endIdxList.clear();
    apexIdxList.clear();

    QList<PeakInterval> finalList, queList, peakIntervalList;
    const point2dList & pointList = plot.getPointList();

    queList.push_back(PeakInterval(0, static_cast<pt2idx>(pointList.size()) - 1));
    while(queList.size() > 0) {
        PeakInterval peakInterval = queList.back();
        queList.pop_back();

        e = makePeaksForInterval(plot, peakIntervalTimeMin, peakTolMin,
                                 peakTolMax, peakInterval,
                                 peakIntervalList, tempFolderPtr); eee;
        if (peakIntervalList.size() == 1) {
            finalList.append(peakIntervalList);
        } else {
            queList.append(peakIntervalList);
        }
        //break;
    }

    // finalList.append(queList);

    qSort(finalList.begin(), finalList.end(), interval_less);

    //recompute apex
    for (int i = 0; i < finalList.size(); i++) {
        double startTime = pointList[finalList[i].istart].x();
        double endTime = pointList[finalList[i].iend].x();
        pt2idx idx_most_middle = 0;
        getBestCriticalPositions(plot, PlotBase::FindMax, startTime, endTime, &idx_most_middle, &(finalList[i].iapex));
    }

    //correct first and last to match the domain boundaries
    if (finalList.size() > 0) {
        if (plot.getPointListSize() <= 0) {
            e = kBadParameterError; eee;
        }
        finalList[0].istart = 0;
        finalList[finalList.size() - 1].iend = plot.getPointListSize() - 1;
    }

    // add signal start and end positions
    for (int i = 0; i < finalList.size(); i++) {
        startIdxList.push_back(finalList[i].istart);
        endIdxList.push_back(finalList[i].iend);
        apexIdxList.push_back(finalList[i].iapex);
    }

error:
    return e;
}

Err makePeaks2(const PlotBase & plot, const RowNode & options, point2dIdxList & xstartIdxList, point2dIdxList & xendIdxList,
               point2dIdxList & xapexIdxList, QString * tempFolderPtr)
{
    return makePeaks2(plot,
                      rn::getOption(options, kpeak_interval_time_min).toDouble(),
                      rn::getOption(options, kpeak_tol_min).toDouble(),
                      rn::getOption(options, kpeak_tol_max).toDouble(),
                      xstartIdxList, xendIdxList, xapexIdxList, tempFolderPtr);
}

_PMI_END

