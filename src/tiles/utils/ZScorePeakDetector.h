/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef Z_SCORE_PEAK_DETECTOR_H
#define Z_SCORE_PEAK_DETECTOR_H

#include "pmi_common_ms_export.h"

#include <Interval.h>

#include <common_math_types.h>
#include <pmi_core_defs.h>

#include <vector>

_PMI_BEGIN

struct ZScoreSettings {
    int lag;
    double threshold;
    double influence;
};

/*!
\brief ZScorePeakDetector provides z-score algorithm-based peak detector
*/
class PMI_COMMON_MS_EXPORT ZScorePeakDetector
{

public:
    /*!
    \brief find peaks in given \a signal.
    */
    static QVector<Interval<int>> findPeaks(const std::vector<double> &signal,
                                            const ZScoreSettings &settings);

    /*!
    \brief Overloaded function, signal will be extracted from point2d::y() coordinate
    */
    static QVector<Interval<int>> findPeaks(const point2dList &signal,
                                            const ZScoreSettings &settings);

    /*!
    \brief Computes z-score values. Every change in the signal is represented as 1, 0, or -1
    */
    static std::vector<double> zScore(const point2dList &signal, const ZScoreSettings &settings);

    /*!
    \brief converts intervals to output of \a zScore function.
    */
    static std::vector<double> toZScoreSignal(const QVector<Interval<int>> &intervals, int size);

private:
    static std::vector<double> smoothedSignal(const std::vector<double> &signal);

    static std::vector<double> extractIntensity(const point2dList &signal);
};

_PMI_END

#endif // Z_SCORE_PEAK_DETECTOR_H