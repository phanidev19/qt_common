/*
* Copyright (C) 2019 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Drew Nichols anichols@proteinmetrics.com
*/

#include "CommonFunctions.h"
#include "SpectraDisambigutron.h"

_PMI_BEGIN

SpectraDisambigutron::SpectraDisambigutron()
{
}

SpectraDisambigutron::~SpectraDisambigutron()
{
}

Err SpectraDisambigutron::init()
{
    Err e = kNoErr;
    buildSuccessiveCombFilters();
    if (m_successiveCombFilters.empty()) {
        rrr(kError);
    }
    return e;
}

Err SpectraDisambigutron::removeOverlappingIonsFromScan(const Eigen::SparseVector<double> &inputScan, int charge, Eigen::SparseVector<double> *outputScan)
{
    Err e = kNoErr;

    Eigen::SparseVector<double> constructionVector(m_ffParams.vectorArrayLength);

    if (charge < 1 || charge > m_successiveCombFilters.size()) {
        rrr(kError);
    }
    const Eigen::SparseMatrix<double> &tsuccessiveFilter = m_successiveCombFilters[charge - 1];
    
    double lastIntensity = -1;
    double noiseCutoffPercent = 0.05;
    for (int k = 0; k < tsuccessiveFilter.cols(); ++k) {
        Eigen::SparseVector<double> minMaxVector = inputScan.cwiseProduct(tsuccessiveFilter.col(k));
        minMaxVector.prune(0.0);
        
        if (minMaxVector.nonZeros() > 1) {           
            if (charge < 1 || charge > m_combSpikes.size()) {
                rrr(kError);
            }
            int spikeOfInterest = m_combSpikes[charge - 1][k];
            int spikeOfInterestDistance = std::numeric_limits<int>::max();
            int indexOfClosestMatch = -1;
            double valueOfClosestMatch = -1;
            for (Eigen::SparseVector<double>::InnerIterator it(minMaxVector); it; ++it) {
                if (std::abs(it.index() - spikeOfInterest) < spikeOfInterestDistance && it.value() >= lastIntensity * noiseCutoffPercent) {
                    spikeOfInterestDistance = std::abs(it.index() - spikeOfInterest);
                    indexOfClosestMatch = it.index();
                    valueOfClosestMatch = it.value();
                }
            }
            if (indexOfClosestMatch != -1 && valueOfClosestMatch != -1) {
                constructionVector.coeffRef(indexOfClosestMatch) = valueOfClosestMatch;
                lastIntensity = valueOfClosestMatch;
            }  
        }
        else {
            for (Eigen::SparseVector<double>::InnerIterator it(minMaxVector); it; ++it)
            {
                constructionVector.coeffRef(it.index()) = it.value();
                lastIntensity = it.value();
            }
        }
    }
    
    *outputScan = constructionVector;
    return e;
}

void SpectraDisambigutron::buildSuccessiveCombFilters()
{
    for (int charge = 1; charge <= m_ffParams.maxChargeState; ++charge) {

        int chargeDistance = FeatureFinderUtils::hashMz(HYDROGEN / charge, m_ffParams.vectorGranularity);
        int teeth = charge < m_ffParams.combFilterTeethChargeMax ? charge : m_ffParams.combFilterTeethChargeMax;

        std::vector<int> combSpike;
        std::vector<Eigen::SparseVector<double>> sparseVectorDepot;
        for (int tooth = -teeth; tooth <= teeth; ++tooth) {
            Eigen::SparseVector<double> tvec(m_ffParams.vectorArrayLength);

            int tindex = m_ffParams.mzMatchIndex + (tooth * chargeDistance);
            combSpike.push_back(tindex);
            if ((tindex - m_ffParams.errorRangeHashed > 0)
                && (tindex + m_ffParams.errorRangeHashed < m_ffParams.vectorArrayLength - 1)) {
                for (int y = tindex - m_ffParams.errorRangeHashed; y <= tindex + m_ffParams.errorRangeHashed; ++y) {
                    tvec.coeffRef(y) = 1;
                }
                tvec.prune(0.0);
                sparseVectorDepot.push_back(tvec);
            }
        }
        //// Assembles the depot vectors into a matrix
        Eigen::SparseMatrix<double, Eigen::RowMajor> matrixByCharge(sparseVectorDepot.size(),
            m_ffParams.vectorArrayLength);
        for (size_t y = 0; y < sparseVectorDepot.size(); ++y) {
            matrixByCharge.row(y) = sparseVectorDepot[y];
        }
        m_combSpikes.push_back(combSpike);
        m_successiveCombFilters.push_back(matrixByCharge.transpose());
    }
}

_PMI_END