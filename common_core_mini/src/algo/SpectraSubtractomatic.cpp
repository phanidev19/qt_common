/*
 * Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Andrew Nichols anichols@proteinmetrics.com
 */

#include "SpectraSubtractomatic.h"
#include "CommonFunctions.h"
#include "Point2dListUtils.h"
#include "pmi_common_core_mini_debug.h"

// stable
#include <common_constants.h>

#include <QFileInfo>

#include <fstream>
#include <string>

_PMI_BEGIN

SpectraSubtractomatic::SpectraSubtractomatic(int minimumIsotopeCount):m_minimumIsotopeCount(minimumIsotopeCount)
{
}

SpectraSubtractomatic::~SpectraSubtractomatic()
{
}

Err SpectraSubtractomatic::init()
{
    Err e = kNoErr;

    using namespace coreMini;

    AveragineIsotopeTable::Parameters parameters;
    parameters.interIsoTrim = 0.00001;
    parameters.finalIsoTrim = 0.0001;
    AveragineIsotopeTable averagineIsotopeTableGenerator(parameters);
    std::vector<std::vector<double>> averagineTable
        = averagineIsotopeTableGenerator.averagineTable();

    for (size_t i = 0; i < averagineTable.size(); ++i) {
        Eigen::RowVectorXd averagineRowEigen = convertVectorToEigenRowVector(averagineTable[i]);
        averagineRowEigen /= averagineRowEigen.maxCoeff();
        m_averagineTable.push_back(averagineRowEigen);
    }

    if (m_averagineTable.empty()) {
        warningCoreMini() << "Averagine Table is not Loaded!";
        rrr(kError);
    }

    return e;
}

Decimator
SpectraSubtractomatic::buildChargeClusterDecimator(Eigen::SparseVector<double> scanSegment,
                                                   double mz, int charge, int monoOffset, bool preciseDecimator)
{

    // Set needed values
    int nominalMW = static_cast<int>((mz * charge) / 100);
    const Eigen::RowVectorXd &averagineRatio = m_averagineTable[nominalMW];
    int chargeDistance = FeatureFinderUtils::hashMz(HYDROGEN / charge, m_ffParams.vectorGranularity);

    // Simultanously build decimator vector to subtract scan as well as matrix distiller for use in
    // pearson's corr calc.
    int fullScanLength = static_cast<int>(m_ffParams.mzMax * m_ffParams.vectorGranularity);
    Eigen::SparseVector<double, Eigen::RowMajor> clusterDecimatorVectorFull(fullScanLength);
    Eigen::SparseMatrix<double, Eigen::RowMajor> corrExtractorMatrix(averagineRatio.cols(),
                                                                     m_ffParams.vectorArrayLength);
    int fullMatchIndex = FeatureFinderUtils::hashMz(mz, m_ffParams.vectorGranularity);
    
    int precisionRange = 2;
    int errorRangeHashed = preciseDecimator ? precisionRange : m_ffParams.errorRangeHashed;
    
    for (int y = 0; y < averagineRatio.cols(); ++y) {
        int offsetDistance = (y * chargeDistance) - (monoOffset * chargeDistance);
        int insertIndex = m_ffParams.mzMatchIndex + offsetDistance;
        int insertIndexFull = fullMatchIndex + offsetDistance;

        if (preciseDecimator) {
            for (Eigen::SparseVector<double>::InnerIterator it(scanSegment); it; ++it) {
                if (std::abs(insertIndexFull - it.index()) <= m_ffParams.errorRangeHashed) {
                    insertIndexFull = it.index();
                    break;
                }
            }
        }

        if (insertIndexFull < fullScanLength) {
            for (int z = insertIndexFull - errorRangeHashed;
                 z <= insertIndexFull + errorRangeHashed; ++z) {
                clusterDecimatorVectorFull.coeffRef(z) = averagineRatio(y);
            }
        }

        if (insertIndex < m_ffParams.vectorArrayLength) {
            Eigen::SparseVector<double> t_vecSuccComb(m_ffParams.vectorArrayLength);
            for (int w = insertIndex - m_ffParams.errorRangeHashed; w <= insertIndex + m_ffParams.errorRangeHashed;
                 ++w) {
                t_vecSuccComb.coeffRef(w) = 1;
            }
            corrExtractorMatrix.row(y) = t_vecSuccComb;
        }
    }

    // Add padding to the decimator vector in intensity to make sure the whole charge cluster is
    // subtracted
    double augmentFactor = scanSegment.coeffRef(m_ffParams.mzMatchIndex) > 0
        ? scanSegment.coeffRef(m_ffParams.mzMatchIndex) / clusterDecimatorVectorFull.coeffRef(fullMatchIndex)
        : 1;
    clusterDecimatorVectorFull *= (augmentFactor * m_ffParams.augmentFactor);
    Eigen::RowVectorXd scanDistilled = corrExtractorMatrix * scanSegment;

    // Determine Pearson's Corr.
    int maxTeethThreshold = 4;
    int teethInclusion = 2;

    // TODO  Rethink how you want to do this
    int corrLength = charge < maxTeethThreshold ? charge + teethInclusion : scanDistilled.cols();

    scanDistilled
        = (scanDistilled.array() < (scanDistilled.maxCoeff() * m_ffParams.isotopeCutOffClusterPercent))
              .select(0, scanDistilled);
    Eigen::RowVectorXi zeros = scanDistilled.cast<int>().colwise().any();
    int isotopeCount = zeros.sum();

    Eigen::RowVectorXd x = scanDistilled.head(corrLength);
    Eigen::RowVectorXd y = averagineRatio.head(corrLength) * augmentFactor;

    double pearsonCorr = pearsonCorrelation(x, y);
    //// Use this to get rid of unrealistic charge matches, i.e. a +10 charge where only two
    //// isotopes are found.  This is not realistic

    if ((charge > 1 && isotopeCount < m_minimumIsotopeCount) || isotopeCount < 2) {
        pearsonCorr = 0;
    }

    Decimator returnDecimator;
    returnDecimator.correlation = pearsonCorr;
    returnDecimator.clusterDecimator = clusterDecimatorVectorFull;
    returnDecimator.isotopeCount = isotopeCount;
    return returnDecimator;
}

double SpectraSubtractomatic::pearsonCorrelation(const Eigen::RowVectorXd &x,
                                                 const Eigen::RowVectorXd &y)
{
    assert(x.cols() == y.cols());
    // https://en.wikipedia.org/wiki/Pearson_correlation_coefficient

    double n = x.cols();
    // sum xi * yi
    double productXY = x.dot(y);

    //// sum xi
    double sumX = x.sum();

    //// sum yi
    double sumY = y.sum();

    double numerator = n * productXY - sumX * sumY;

    //// denominator calculations
    //// sum (xi * xi)
    double sumSquaredXes = x.dot(x);

    //// (sum xi)^2
    double sumXSquared = sumX * sumX;

    //// sum (yi * yi)
    double sumSquaredYs = y.dot(y);

    //// (sum yi)^2
    double sumYSquared = sumY * sumY;

    double denominator
        = std::sqrt(n * sumSquaredXes - sumXSquared) * std::sqrt(n * sumSquaredYs - sumYSquared);
    double r = denominator != 0 ? numerator / denominator : 0;

    return r;
}

_PMI_END