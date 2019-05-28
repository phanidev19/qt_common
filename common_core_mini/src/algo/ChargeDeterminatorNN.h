/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Andrew Nichols anichols@proteinmetrics.com
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef CHARGE_DETERMINATION_NN_H
#define CHARGE_DETERMINATION_NN_H

#include "AbstractChargeDeterminator.h"
#include "FeatureFinderParameters.h"
#include "pmi_common_core_mini_export.h"

// common_stable
#include <common_constants.h>
#include <common_errors.h>
#include <common_math_types.h>

// Eigen
#include <Eigen/Sparse>

// Qt
#include <QString>

_PMI_BEGIN

/*!
 * \brief Determines the charge for given part of the MS1 scan
 *
 * The algorithm is using neural network coefficients provided in nnFilePath
 */
class PMI_COMMON_CORE_MINI_EXPORT ChargeDeterminatorNN : public AbstractChargeDeterminator
{
public:
    ChargeDeterminatorNN();
    
    ~ChargeDeterminatorNN();

    /*!
     * \brief Initializes the class with filepath provided in constructor
     *
     * Be sure that filepath provided exists and is readable
     *
     * \return kNoErr if everything is fine
     */
    Err init(const QString &nnFilePath);

    /*!
     * \brief Determines the charge for given part of the MS1 scan \a scanSegment
     *
     * \return -1 if the class is not properly initialized, \see init();
     */
    int determineCharge(const Eigen::SparseVector<double> &scanSegment) const;

    int determineCharge(const point2dList &scanPart, double mz) override;

private:
    void buildSuccessiveCombFilters();
    bool isValid() const;

private:
    std::vector<Eigen::SparseMatrix<double>> m_successiveCombFilters;
    std::vector<Eigen::MatrixXd> m_neuralNetworkWeights;
    ImmutableFeatureFinderParameters m_ffParams;
    
};

_PMI_END

#endif // CHARGE_DETERMINATION_NN_H