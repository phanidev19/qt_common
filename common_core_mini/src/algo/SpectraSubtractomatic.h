/*
 * Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Andrew Nichols anichols@proteinmetrics.com
 * Author: Lukas Tvrdy ltvrdy@milosolutions.com
 */

#ifndef SPECTRA_SUBTRACTOMATIC_H
#define SPECTRA_SUBTRACTOMATIC_H

#include "AveragineIsotopeTable.h"
#include "FeatureFinderParameters.h"
#include "pmi_common_core_mini_export.h"

#include <Eigen/Core>
#include <Eigen/Sparse>

// common_stable
#include <common_errors.h>
#include <common_math_types.h>

#include <QString>

_PMI_BEGIN

struct Decimator {
    double correlation = 0;
    int isotopeCount = 0;
    Eigen::SparseVector<double, Eigen::RowMajor> clusterDecimator;
};

/*!
 * @brief Build a vector to subtract charge clusters according to their
 * theoretical averagine ratios.
 *
 * The purpose is to build a vector to subtract charge clusters according to their
 * theoretical averagine ratios from the scan after the charge and monoisotope is determined.This
 * allows any spectra that may overlap to be accurately determined.It also determines averagine
 * correlation as well, which is used to decide if the cluster should be entered in the DB.
 */
class PMI_COMMON_CORE_MINI_EXPORT SpectraSubtractomatic
{
public:
    SpectraSubtractomatic(int minimumIsotopeCount = 3);

    ~SpectraSubtractomatic();

    Decimator buildChargeClusterDecimator(Eigen::SparseVector<double> scanSegment, double mz,
                                          int charge, int monoOffset, bool preciseDecimator = false);
    Err init();

private:
    static double pearsonCorrelation(const Eigen::RowVectorXd &x, const Eigen::RowVectorXd &y);

private:
    std::vector<Eigen::RowVectorXd> m_averagineTable;
    int m_minimumIsotopeCount;
    ImmutableFeatureFinderParameters m_ffParams;
};

_PMI_END
#endif // SPECTRA_SUBTRACTOMATIC_H
