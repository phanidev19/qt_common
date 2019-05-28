/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "ZScorePeakDetector.h"

#include <Convolution1dCore.h>
#include <PlotBase.h>
#include <VectorStats.h>

#include <QDir>

#include <unordered_map>

_PMI_BEGIN

/**
 * This is the implementation of the Smoothed Z-Score Algorithm.
 * This is direction translation of https://stackoverflow.com/a/22640362/1461896.
 *
 * @param input - input signal
 * @param lag - the lag of the moving window
 * @param threshold - the z-score at which the algorithm signals
 * @param influence - the influence (between 0 and 1) of new signals on the mean and standard
 * deviation
 * @return a hashmap containing the filtered signal and corresponding mean and standard deviation.
 */
std::unordered_map<std::string, std::vector<double>>
z_score_thresholding(const std::vector<double> &input, int lag, double threshold, double influence)
{
    std::unordered_map<std::string, std::vector<double>> output;

    int n = static_cast<int>(input.size());
    std::vector<double> signal(input.size());
    std::vector<double> filteredInput(input.begin(), input.end());
    std::vector<double> filteredMean(input.size());
    std::vector<double> filteredStddev(input.size());

    filteredMean[lag - 1] = VectorStats<double>::mean(input.begin(), input.begin() + lag);
    filteredStddev[lag - 1] = VectorStats<double>::stdDev(input.begin(), input.begin() + lag);

    for (int i = lag; i < n; i++) {
        if (std::abs(input[i] - filteredMean[i - 1]) > threshold * filteredStddev[i - 1]) {
            signal[i] = (input[i] > filteredMean[i - 1]) ? 1.0 : -1.0;
            filteredInput[i] = influence * input[i] + (1 - influence) * filteredInput[i - 1];
        } else {
            signal[i] = 0.0;
            filteredInput[i] = input[i];
        }

        auto begin = std::next(filteredInput.begin(), (i - lag));
        auto end = std::next(filteredInput.begin() + i);
        filteredMean[i] = VectorStats<double>::mean(begin, end);
        filteredStddev[i] = VectorStats<double>::stdDev(begin, end);
    }

    output["signals"] = signal;
    output["filtered_mean"] = filteredMean;
    output["filtered_stddev"] = filteredStddev;

    return output;
}

QVector<Interval<int>> fromZScoreSignal(const std::vector<double> &signal)
{
    QVector<Interval<int>> result;
    if (signal.empty()) {
        return result;
    }

    const double DOWN = 0.0;
    const double UP = 1.0;

    Interval<int> item;
    double prevFlag = signal.at(0);
    if (prevFlag == UP) {
        item.setStart(0);
    }

    for (int i = 1; i < static_cast<int>(signal.size()); ++i) {
        double currentFlag = signal.at(i);

        if (currentFlag != prevFlag) {

            if (prevFlag == DOWN && currentFlag == UP) {
                // start the interval
                item.setStart(i);
                prevFlag = currentFlag;
            } else if (prevFlag == UP && currentFlag == DOWN) {
                item.setEnd(i - 1);
                prevFlag = currentFlag;

                // reset
                result.push_back(item);
                item = Interval<int>();
            }
        }
    }

    return result;
}

QVector<Interval<int>> ZScorePeakDetector::findPeaks(const std::vector<double> &input,
                                                     const ZScoreSettings &settings)
{
    QVector<Interval<int>> result;
    if (input.empty()) {
        return result;
    }

    std::vector<double> smoothedInput = smoothedSignal(input);

    std::vector<double> zScoreSignal = z_score_thresholding(
        smoothedInput, settings.lag, settings.threshold, settings.influence)["signals"];

#ifdef DEBUG_SIGNAL
    point2dList debugSignal;
    debugSignal.reserve(zScoreSignal.size());
    for (size_t i = 0; i < zScoreSignal.size(); ++i) {
        debugSignal.push_back(point2d(smoothedInput.at(i), zScoreSignal.at(i)));
    }

#endif

    result = fromZScoreSignal(zScoreSignal);

    return result;
}

QVector<Interval<int>> ZScorePeakDetector::findPeaks(const point2dList &signal,
                                                     const ZScoreSettings &settings)
{
    return findPeaks(extractIntensity(signal), settings);
}

std::vector<double> ZScorePeakDetector::zScore(const point2dList &signal,
                                               const ZScoreSettings &settings)
{
    std::vector<double> input = extractIntensity(signal);
    std::vector<double> smoothedInput = smoothedSignal(input);

    std::vector<double> zScoreSignal = z_score_thresholding(
        smoothedInput, settings.lag, settings.threshold, settings.influence)["signals"];

    return zScoreSignal;
}

std::vector<double> ZScorePeakDetector::toZScoreSignal(const QVector<Interval<int>> &intervals,
                                                       int size)
{
    std::vector<double> signal;
    signal.resize(size);
    for (const Interval<int> &item : intervals) {
        for (int i = item.start(); i <= item.end(); ++i) {
            signal[i] = 1.0;
        }
    }

    return signal;
}

std::vector<double> ZScorePeakDetector::smoothedSignal(const std::vector<double> &signal)
{
    std::vector<double> result;

    std::vector<double> kernel;
    // TODO: possible user parameter
    double GAUSSIAN_STD_DEV = 2.0;
    core::makeGaussianKernel(GAUSSIAN_STD_DEV, kernel);
    core::conv(signal, kernel, kBoundaryTypeZero, result);

    return result;
}

std::vector<double> ZScorePeakDetector::extractIntensity(const point2dList &signal)
{
    std::vector<double> intensity;
    intensity.reserve(intensity.size());
    for (const point2d &pt : signal) {
        intensity.push_back(pt.y());
    }

    return intensity;
}

_PMI_END
