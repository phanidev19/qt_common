/*
 * Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Andrew Nichols anichols@proteinmetrics.com
 */

#include "FindMzToProcess.h"

#include "CommonFunctions.h"
#include "Point2dListUtils.h"
#include "pmi_common_core_mini_debug.h"

#include <common_constants.h>

_PMI_BEGIN

FindMzToProcess::FindMzToProcess(const ChargeDeterminatorNN &chargeDeterminator,
                                 const SettableFeatureFinderParameters &ffUserParams)
    : m_chargeDeterminator(chargeDeterminator)
{
}

FindMzToProcess::~FindMzToProcess()
{
}

point2dList FindMzToProcess::searchFullScanForMzIterators(
    const point2dList &scanData, const Eigen::SparseVector<double, Eigen::RowMajor> &fullScan)
{
    double noiseFloor = determineNoiseFloor(scanData);
    std::vector<point2dList> clusters = linearDBSCAN(scanData, noiseFloor);

    //// Iterate over the dbscan returned clusters
    point2dList iters;
    for (size_t i = 0; i < clusters.size(); ++i) {

        point2dList tclust = clusters[i];
        std::vector<bool> tclustVisited(tclust.size(), false);

        ////Process all points within 1 cluster
        if (tclust.size() > 1) {
            Eigen::SparseVector<double, Eigen::RowMajor> dbscanCluster(
                static_cast<int>(m_ffParams.mzMax * m_ffParams.vectorGranularity));

            //// Translate full scan to portion only concerning the charge cluster currently being
            /// extracted.
            int startTranslate
                = FeatureFinderUtils::hashMz(tclust[0].x(), m_ffParams.vectorGranularity);
            int endTranslate = FeatureFinderUtils::hashMz(tclust[tclust.size() - 1].x(),
                                                          m_ffParams.vectorGranularity);

            for (Eigen::SparseVector<double, Eigen::RowMajor>::InnerIterator it(fullScan); it;
                 ++it) {
                if ((it.index() >= startTranslate) & (it.index() <= endTranslate)) {
                    dbscanCluster.coeffRef(it.index()) = it.value();
                }
            }

            double maxIntensity = returnSparseVectorMaximum(dbscanCluster);
            while (maxIntensity > noiseFloor) {
                //// Finds the cooresponding mz to the max intensity for use in charge determination
                /// and subtraction
                maxIntensity = returnSparseVectorMaximum(dbscanCluster);
                double tempMz = -1;

                for (size_t m = 0; m < tclust.size(); ++m) {
                    if ((tclust[m].y() == maxIntensity) && (tclustVisited[m] != true)) {
                        tempMz = tclust[m].x();
                        tclustVisited[m] = true;
                        break;
                    }
                }

                if (tempMz == -1) {
                    continue;
                }

                ////Slice the array to get charge state.
                int tIndex = FeatureFinderUtils::hashMz(tempMz, m_ffParams.vectorGranularity);
                Eigen::SparseVector<double> scanSegment = fullScan.middleCols(
                    tIndex - m_ffParams.mzMatchIndex, (2 * m_ffParams.mzMatchIndex) + 1);
                int charge = m_chargeDeterminator.determineCharge(scanSegment);
                if (charge == 0) {
                    break;
                }
                double chargeDistance = 1 / static_cast<double>(charge);

                ////build subtraction indicies
                size_t whileIter = 0;
                std::vector<double> storedMzToExtract;
                while (1) {
                    double whileMz = tempMz - (whileIter * chargeDistance);
                    if (whileMz < tclust[0].x()) {
                        break;
                    }
                    storedMzToExtract.push_back(whileMz);
                    whileIter++;
                }
                whileIter = 1;
                while (1) {
                    double whileMz = tempMz + (whileIter * chargeDistance);
                    if (whileMz > tclust[tclust.size() - 1].x()) {
                        break;
                    }
                    storedMzToExtract.push_back(whileMz);
                    whileIter++;
                }
                std::sort(storedMzToExtract.begin(), storedMzToExtract.end());

                ////Extract Ions for maxima determination and erase
                std::vector<int> extractedMaxima;
                for (size_t n = 0; n < storedMzToExtract.size(); ++n) {
                    int startIndex = FeatureFinderUtils::hashMz(storedMzToExtract[n],
                                                                m_ffParams.vectorGranularity)
                        - m_ffParams.errorRangeHashed;
                    int distance = (2 * m_ffParams.errorRangeHashed) + 1;
                    extractedMaxima.push_back(
                        returnSparseVectorMaximum(dbscanCluster.middleCols(startIndex, distance)));

                    for (int p = startIndex; p < (startIndex + distance); ++p) {
                        dbscanCluster.coeffRef(p) = 0;
                        dbscanCluster.prune(0.0);
                    }

                } //// END while(1)

                std::vector<int> intensitiesMaximus = findLocalMaxima(extractedMaxima);
                for (size_t j = 0; j < intensitiesMaximus.size(); ++j) {
                    int t_intensity = intensitiesMaximus[j];
                    for (size_t z = 0; z < tclust.size(); ++z) {
                        if (t_intensity == static_cast<int>(tclust[z].y())) {
                            iters.push_back(tclust[z]);
                        }
                    }
                }

                int ionCount = dbscanCluster.nonZeros();
                if (ionCount < 2) {
                    break;
                }
            } //// END n loop
        }
    }

    std::sort(iters.begin(), iters.end(),
              [](const point2d &a, const point2d &b) { return a.y() > b.y(); });

    return iters;
}

int FindMzToProcess::determineNoiseFloor(const point2dList &scanData)
{
    //// Perhaps figure out a better way to determine noise other than the median.
    std::vector<int> intensities;
    intensities.reserve(scanData.size());
    for (size_t y = 1; y < scanData.size(); ++y) {
        intensities.push_back(scanData[y].y());
    }

    std::sort(intensities.begin(), intensities.end());
    size_t s = intensities.size();
    double intensityPercentCutoff = 0.8;
    intensities.resize(static_cast<int>(s * intensityPercentCutoff));

    double median = calculateMedian(intensities);
    double stDev = calculateStDev(intensities);

    m_noiseFloor = median + (m_ffUserParams.noiseFactorMultiplier * stDev);

    return m_noiseFloor;
}

std::vector<point2dList> FindMzToProcess::linearDBSCAN(const point2dList &scanData,
                                                       double intensityCutoff, double epsilon,
                                                       int minMemberCount) const
{
    // Consider using DBSCAN Class instead of this code.
    std::vector<point2dList> clusters;

    // Filter out all points below intensityCutoff
    point2dList pointsOfInterest;
    for (size_t i = 0; i < scanData.size(); ++i) {
        if (scanData[i].y() > intensityCutoff) {
            // qDebug() << scanData[i].x() << " " << scanData[i].y();
            pointsOfInterest.push_back(scanData[i]);
        }
    }

    if (pointsOfInterest.size() == 0) {
        return clusters;
    }

    point2dList t_store;
    for (size_t i = 0; i < pointsOfInterest.size() - 1; ++i) {
        if (pointsOfInterest[i + 1].x() - pointsOfInterest[i].x() <= epsilon) {
            // qDebug() << pointsOfInterest[i].x();
            t_store.push_back(pointsOfInterest[i]);
        } else {
            t_store.push_back(pointsOfInterest[i]);

            if (static_cast<int>(t_store.size()) >= minMemberCount) {
                clusters.push_back(t_store);
                /* for (auto ss : t_store) {
                qDebug() << ss.x();
                }
                qDebug() << " ";*/
            }
            t_store.clear();
        }
    }
    if (static_cast<int>(t_store.size()) >= minMemberCount) {
        clusters.push_back(t_store);
    }

    return clusters;
}

std::vector<int> FindMzToProcess::findLocalMaxima(std::vector<int> &vec)
{
    // This might need to be revisited.
    vec.push_back(0);
    std::reverse(vec.begin(), vec.end());
    vec.push_back(0);
    std::reverse(vec.begin(), vec.end());
    // qDebug() << vec;

    std::vector<int> localMaxima;
    for (size_t y = 0; y < vec.size(); ++y) {
        if (y == 0 && vec[y] > vec[y + 1]) {
            localMaxima.push_back(vec[y]);
        } else if (y > 0 && y < vec.size() - 1) {
            if (vec[y] > vec[y - 1] && vec[y] > vec[y + 1]) {
                localMaxima.push_back(vec[y]);
            }
        } else if (y == vec.size() - 1) {
            if (vec[y] > vec[y - 1]) {
                localMaxima.push_back(vec[y]);
            }
        }
    }

    return localMaxima;
}

double FindMzToProcess::noiseFloor() const
{
    return m_noiseFloor;
}

double FindMzToProcess::calculateStDev(const std::vector<int> &data)
{
    size_t dataSize = data.size();
    double sum = 0.0;
    double mean = 0.0;
    double standardDeviation = 0.0;
    for (size_t i = 0; i < dataSize; ++i) {
        sum += data[i];
    }
    mean = sum / dataSize;
    for (size_t i = 0; i < dataSize; ++i) {
        standardDeviation += std::pow(data[i] - mean, 2);
    }

    return std::sqrt(standardDeviation / dataSize);
}

double FindMzToProcess::calculateMedian(std::vector<int> data)
{
    if (data.empty()) {
        return 0.0;
    }

    std::sort(data.begin(), data.end());

    int median = 0;
    int arraySize = static_cast<int>(data.size());
    int midPoint = 0;

    if (arraySize % 2 != 0) {
        midPoint = static_cast<int>(std::floor(arraySize / 2) + 1);
        median = data[midPoint];

    } else {
        midPoint = static_cast<int>(arraySize / 2);
        median = ((data[midPoint] + data[midPoint + 1]) / 2);
    }

    return median;
}

double FindMzToProcess::returnSparseVectorMaximum(const Eigen::SparseVector<double> &sparseVector,
                                                  bool returnIndex)
{
    double maxValue = 0;
    double maxIndex = 0;
    for (Eigen::SparseVector<double>::InnerIterator it(sparseVector); it; ++it) {
        if (maxValue < it.value()) {
            maxValue = it.value();
            maxIndex = it.index();
        }
    }
    if (returnIndex) {
        return maxIndex;
    }
    return maxValue;
}

_PMI_END