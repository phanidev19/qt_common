/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Andrew Nichols anichols@proteinmetrics.com
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "ChargeDeterminatorNN.h"
#include "CommonFunctions.h"
#include "Point2dListUtils.h"
#include "pmi_common_core_mini_debug.h"

#include <QFileInfo>

_PMI_BEGIN

ChargeDeterminatorNN::ChargeDeterminatorNN()
{
}

ChargeDeterminatorNN::~ChargeDeterminatorNN()
{
}

Err ChargeDeterminatorNN::init(const QString &nnFilePath)
{
    Err e = kNoErr;
    if (!QFileInfo::exists(nnFilePath)) {
        rrr(kError);
    }

    e = readNeuralNetworkWeightsSqlQT(nnFilePath.toStdString(), 1, &m_neuralNetworkWeights); ree;
    if (m_neuralNetworkWeights.size() == 0) {
        warningCoreMini() << "Neural network weights are empty!";
        rrr(kError);
    }

    buildSuccessiveCombFilters();
    return e;
}

void ChargeDeterminatorNN::buildSuccessiveCombFilters()
{
    // Builds a series of matrices to convert the input scan into a vector w/ 90 element.
    // A matrix is built for each charge and then the max value in each row is collated into a 90
    // element vector
    for (int charge = 1; charge < m_ffParams.maxChargeState + 1; ++charge) {

        int chargeDistance = FeatureFinderUtils::hashMz(HYDROGEN / charge, m_ffParams.vectorGranularity);
        bool isDimerCharge = 1 <= charge && charge <= 3;
        int dimerChargeDistance = isDimerCharge
            ? FeatureFinderUtils::hashMz(1.0 / (charge * 2), m_ffParams.vectorGranularity)
            : 0;
        int teeth = charge < m_ffParams.combFilterTeethChargeMax ? charge : m_ffParams.combFilterTeethChargeMax;

        std::vector<Eigen::SparseVector<double>> sparseVectorDepot;
        for (int tooth = -teeth; tooth <= teeth; ++tooth) {
            Eigen::SparseVector<double> tvec(m_ffParams.vectorArrayLength);
            tvec.setZero();
            int tindex = m_ffParams.mzMatchIndex + (tooth * chargeDistance);
            if ((tindex - m_ffParams.errorRangeHashed > 0)
                && (tindex + m_ffParams.errorRangeHashed < m_ffParams.vectorArrayLength - 1)) {
                for (int y = tindex - m_ffParams.errorRangeHashed; y <= tindex + m_ffParams.errorRangeHashed; ++y) {
                    tvec.coeffRef(y) = 1;
                }
                sparseVectorDepot.push_back(tvec);
                if (isDimerCharge && tooth < teeth) {
                    tvec.setZero();
                    for (int z = tindex + dimerChargeDistance - m_ffParams.errorRangeHashed;
                         z <= tindex + dimerChargeDistance + m_ffParams.errorRangeHashed; ++z) {
                        tvec.coeffRef(z) = -1;
                    }
                    sparseVectorDepot.push_back(tvec);
                }
            }
        }

        Eigen::SparseMatrix<double, Eigen::RowMajor> t_matrixByCharge(sparseVectorDepot.size(),
                                                                      m_ffParams.vectorArrayLength);
        for (size_t y = 0; y < sparseVectorDepot.size(); ++y) {
            t_matrixByCharge.row(y) = sparseVectorDepot[y];
        }

        m_successiveCombFilters.push_back(t_matrixByCharge);
    }
}

bool ChargeDeterminatorNN::isValid() const
{
    return !m_neuralNetworkWeights.empty();
}

int ChargeDeterminatorNN::determineCharge(const Eigen::SparseVector<double> &scanSegment) const
{
    if (!isValid()) {
        return -1;
    }

    ////Extract points from scan vector by combfilter for input into neural network forward
    ///propagation
    std::vector<double> passingVec;

    for (size_t j = 0; j < m_successiveCombFilters.size(); ++j) {
        const Eigen::SparseMatrix<double> &tsuccessiveFilter = m_successiveCombFilters[j];
        // TODO Explore replacing this summing w/ rowwise max()
        Eigen::VectorXd maxRowValues = tsuccessiveFilter * scanSegment; 

        maxRowValues /= maxRowValues.maxCoeff();
        for (int k = 0; k < maxRowValues.rows(); ++k) {
            passingVec.push_back(maxRowValues(k, 0));
        }
    }

    //// Neural Network Layers Matrix Math
    // TODO This function can probs be replaced by Eigen::Map Function
    Eigen::RowVectorXd forwardPropagation = convertVectorToEigenRowVector(passingVec); 
    forwardPropagation = (forwardPropagation * m_neuralNetworkWeights[0]) + m_neuralNetworkWeights[1];
    forwardPropagation = relu(forwardPropagation);
    forwardPropagation = (forwardPropagation * m_neuralNetworkWeights[2]) + m_neuralNetworkWeights[3];
    forwardPropagation = relu(forwardPropagation);
    forwardPropagation = (forwardPropagation * m_neuralNetworkWeights[4]) + m_neuralNetworkWeights[5];
    forwardPropagation = sigmoid(forwardPropagation);

    //// Return index of Max value.  Max value typically = 1 or close to it.
    int charge = 0;
    for (int m = 0; m < forwardPropagation.cols(); ++m) {
        if (forwardPropagation(m) == forwardPropagation.rowwise().maxCoeff()(0)) {
            charge = m + 1;
            break;
        }
    }

    return charge;
}

int ChargeDeterminatorNN::determineCharge(const point2dList &scanPart, double mz)
{
    std::vector<double> uniformIntensitySegment
        = FeatureFinderUtils::extractUniformIntensity(scanPart, mz, m_ffParams.apexChargeClustering);

    Eigen::SparseVector<double> vecEigenVec(m_ffParams.vectorArrayLength);
    for (size_t y = 0; y < uniformIntensitySegment.size(); ++y) {
        vecEigenVec.coeffRef(y) = uniformIntensitySegment[y];
    }

    return determineCharge(vecEigenVec);
}

_PMI_END