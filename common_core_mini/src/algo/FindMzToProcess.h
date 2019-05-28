/*
 * Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Andrew Nichols anichols@proteinmetrics.com
 */
#ifndef FIND_MZ_TO_PROCESS_H
#define FIND_MZ_TO_PROCESS_H

#include "ChargeDeterminatorNN.h"
#include "FeatureFinderParameters.h"
#include "pmi_common_core_mini_export.h"

// common_stable
#include <common_errors.h>
#include <common_math_types.h>

#include <vector>

_PMI_BEGIN

/*!
 * @brief Looks at all points in a scan and determines the ones to process into features.
 * This replaces the hi-lo iteration paradigm which tends to produce a lot of false positives.
 * This class needs to be improved.  It may not work well for dense proteomics data.
 */
class PMI_COMMON_CORE_MINI_EXPORT FindMzToProcess
{

public:
    FindMzToProcess(const ChargeDeterminatorNN &chargeDeterminator,
                    const SettableFeatureFinderParameters &ffUserParams);
    ~FindMzToProcess();
    point2dList
    searchFullScanForMzIterators(const point2dList &scanData,
                                 const Eigen::SparseVector<double, Eigen::RowMajor> &fullScan);
    double noiseFloor() const;

private:
    int determineNoiseFloor(const point2dList &scanData);
    std::vector<point2dList> linearDBSCAN(const point2dList &scanData, double intensityCutoff,
                                          double epsilon = 1.05, int minMemberCount = 2) const;
    static std::vector<int> findLocalMaxima(std::vector<int> &vec);

    // This is duplicated in ScanIterator.  Put in common functions
    static double calculateStDev(const std::vector<int> &data);
    // This is duplicated in ScanIterator.  Put in common functions
    static double calculateMedian(std::vector<int> data);

    double returnSparseVectorMaximum(const Eigen::SparseVector<double> &sparseVector,
                                     bool returnIndex = false);

private:
    ChargeDeterminatorNN m_chargeDeterminator;
    double m_noiseFloor = 0.0;
    ImmutableFeatureFinderParameters m_ffParams;
    SettableFeatureFinderParameters m_ffUserParams;
};

_PMI_END

#endif // FIND_MZ_TO_PROCESS_H