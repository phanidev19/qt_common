/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Andrew Nichols anichols@proteinmetrics.com
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "MonoisotopeDeterminatorNN.h"
#include "CommonFunctions.h"

#include "pmi_common_core_mini_debug.h"

#include <QFileInfo>

_PMI_BEGIN

MonoisotopeDeterminatorNN::MonoisotopeDeterminatorNN()
{
}

Err MonoisotopeDeterminatorNN::init(const QString &nnFilePath)
{
    Err e = kNoErr;
    if (!QFileInfo::exists(nnFilePath)) {
        rrr(kError);
    }

    // Read neural network weights
    for (int i = 2; i <= 11; ++i) {
        std::vector<Eigen::MatrixXd> matrixByCharge;
        e = readNeuralNetworkWeightsSqlQT(nnFilePath.toStdString(), i, &matrixByCharge); ree;
        m_neuralNetworkWeights.push_back(matrixByCharge);
    }

    if (m_neuralNetworkWeights.size() == 0) {
        warningCoreMini() << "Neural network weights are empty!";
        rrr(kError);
    }

    buildSuccessiveCombFiltersMono();

    return e;
}

int MonoisotopeDeterminatorNN::determineMonoisotopeOffset(const Eigen::SparseVector<double> &scanSegment,
                                                          double mz, int charge)
{
    Eigen::SparseVector<double> scanSegmentNorm
        = scanSegment / scanSegment.coeff(m_ffParams.mzMatchIndex);
    scanSegmentNorm.prune(0.0);

    double mwNNScalingImpact = 100.0;
    double mw = (charge * mz) / mwNNScalingImpact;
    std::vector<double> passingVec = { (mw / mwNNScalingImpact) };
    const std::vector<Eigen::SparseMatrix<double>> &ttSuccessiveCombFilter = m_successiveCombFilters[charge - 1];

    for (size_t j = 0; j < ttSuccessiveCombFilter.size(); ++j) {
        // TODO Check about going back to using max and min values instead of sum.
        const Eigen::SparseMatrix<double> &tsuccessiveFilter = ttSuccessiveCombFilter[j];

//#define MINMAX
#ifdef MINMAX
        ///// Takes minimum value for first tooth and maximum value for all others.
        Eigen::SparseMatrix<double> tsuccessiveFilterTrans = tsuccessiveFilter.transpose();
        for (int k = 1; k < tsuccessiveFilterTrans.cols(); ++k) {
            Eigen::VectorXd minMaxVector = scanSegmentNorm.cwiseProduct(tsuccessiveFilterTrans.col(k));
            double minMax = k == 0 ? minMaxVector.minCoeff() : minMaxVector.maxCoeff();
            passingVec.push_back(minMax);
        }
#else
        // this sums everything in the tooth instead of taking the max or min
        Eigen::VectorXd maxRowValues = tsuccessiveFilter * scanSegmentNorm;
        for (int i = 1; i < maxRowValues.rows(); ++i) {
            passingVec.push_back(maxRowValues(i, 0));
        }
#endif    



    }
    // Neural Network Layers Matrix Math
    Eigen::RowVectorXd forwardPropagation = convertVectorToEigenRowVector(passingVec);
    forwardPropagation = (forwardPropagation * m_neuralNetworkWeights[charge - 1][0])
        + m_neuralNetworkWeights[charge - 1][1];
    forwardPropagation = relu(forwardPropagation);
    forwardPropagation = (forwardPropagation * m_neuralNetworkWeights[charge - 1][2])
        + m_neuralNetworkWeights[charge - 1][3];
    forwardPropagation = relu(forwardPropagation);
    forwardPropagation = (forwardPropagation * m_neuralNetworkWeights[charge - 1][4])
        + m_neuralNetworkWeights[charge - 1][5];
    forwardPropagation = sigmoid(forwardPropagation);

    // Return index of Max value.  Max value typically = 1 or close to it.
    int monoisotopeOffset = 0;

    for (int m = 0; m < forwardPropagation.cols(); ++m) {
        if (forwardPropagation(m) == forwardPropagation.rowwise().maxCoeff()(0)) {
            monoisotopeOffset = m;
            break;
        }
    }

    return monoisotopeOffset;
}

int MonoisotopeDeterminatorNN::determineMonoisotopeOffset(const pmi::point2dList &scanPart,
                                                          double mz, int charge, double *score)
{
    Q_UNUSED(score);
    std::vector<double> uniformIntensitySegment
        = FeatureFinderUtils::extractUniformIntensity(scanPart, mz, m_ffParams.apexChargeClustering);
    
    Eigen::SparseVector<double> vecEigenVec(m_ffParams.vectorArrayLength);
    for (size_t y = 0; y < uniformIntensitySegment.size(); ++y) {
        vecEigenVec.coeffRef(y) = uniformIntensitySegment[y];
    }

    return determineMonoisotopeOffset(vecEigenVec, mz, charge);
}

void MonoisotopeDeterminatorNN::buildSuccessiveCombFiltersMono()
{
    for (int charge = 1; charge <= m_ffParams.maxChargeState; ++charge) {

        int chargeDistance = FeatureFinderUtils::hashMz(1.0 / charge, m_ffParams.vectorGranularity);
        std::vector<Eigen::SparseMatrix<double>> ttSuccessiveCombFilter;

        for (int roll = 0; roll <= charge + 1; ++roll) {
            std::vector<Eigen::SparseVector<double>>
                tSuccessiveCombFilterPassing; // Temporary Holder for Matrix
            int teeth = charge + 1;

            for (int tooth = -teeth; tooth < 1; ++tooth) {
                Eigen::SparseVector<double> t_vec(m_ffParams.vectorArrayLength);
                t_vec.setZero();
                int t_index = m_ffParams.mzMatchIndex + (chargeDistance * tooth) + (chargeDistance * roll);
                if (t_index - m_ffParams.errorRangeHashed > 0
                    && t_index + m_ffParams.errorRangeHashed <= t_vec.rows()) {

                    for (int y = t_index - m_ffParams.errorRangeHashed; y <= t_index + m_ffParams.errorRangeHashed;
                         ++y) {
                        t_vec.coeffRef(y) = 1;
                    }

                    if (tooth == -teeth) {
                        Eigen::SparseVector<double> t_vecNegative(m_ffParams.vectorArrayLength);
                        t_vecNegative.setZero();

                        for (int z = t_index - m_ffParams.errorRangeHashed - chargeDistance;
                             z <= t_index + m_ffParams.errorRangeHashed - chargeDistance; ++z) {
                            t_vecNegative.coeffRef(z) = -1;
                        }
                        tSuccessiveCombFilterPassing.push_back(t_vecNegative);
                    }
                    tSuccessiveCombFilterPassing.push_back(t_vec);
                }
            }

            Eigen::SparseMatrix<double, Eigen::RowMajor> tSuccessiveCombFilter(
                tSuccessiveCombFilterPassing.size(), m_ffParams.vectorArrayLength);
            for (size_t y = 0; y < tSuccessiveCombFilterPassing.size(); ++y) {
                tSuccessiveCombFilter.row(y) = tSuccessiveCombFilterPassing[y];
            }
            ttSuccessiveCombFilter.push_back(tSuccessiveCombFilter);
        }
        m_successiveCombFilters.push_back(ttSuccessiveCombFilter);
    }
}


_PMI_END