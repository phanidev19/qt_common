/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Andrew Nichols anichols@proteinmetrics.com
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef MONOISOTOPE_DETERMINATION_NN_H
#define MONOISOTOPE_DETERMINATION_NN_H

#include "pmi_common_core_mini_export.h"

#include "AbstractMonoisotopeDeterminator.h"
#include "FeatureFinderParameters.h"

#include <Eigen/Core>
#include <Eigen/Sparse>

// common_stable
#include <common_math_types.h>
#include <common_errors.h>
#include <pmi_core_defs.h>

#include <QString>

#include <vector>

_PMI_BEGIN
/*!
* @brief Determines the monoisotope using neural network
*/
class PMI_COMMON_CORE_MINI_EXPORT MonoisotopeDeterminatorNN : public AbstractMonoisotopeDeterminator
{
public:
    MonoisotopeDeterminatorNN();

    Err init(const QString &nnFilePath);

    int determineMonoisotopeOffset(const Eigen::SparseVector<double> &scanSegment, double mz,
                                   int charge);

    int determineMonoisotopeOffset(const point2dList &fullScan, double mz, int charge,
                                   double *score) override;

private:
    void buildSuccessiveCombFiltersMono();


private:
    std::vector<std::vector<Eigen::SparseMatrix<double>>> m_successiveCombFilters;
    std::vector<std::vector<Eigen::MatrixXd>> m_neuralNetworkWeights;
    ImmutableFeatureFinderParameters m_ffParams;
};

_PMI_END

#endif // CHARGE_DETERMINATION_NN_MONOISOTOPEDETERMINATION_H
