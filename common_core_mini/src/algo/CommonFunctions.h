/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Andrew Nichols anichols@proteinmetrics.com
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef CHARGE_DETERMINATION_NN_COMMONFUNCTIONS_H
#define CHARGE_DETERMINATION_NN_COMMONFUNCTIONS_H

#include "pmi_common_core_mini_export.h"

#include <common_errors.h>
#include <common_math_types.h>


#include <Eigen/Sparse>
#include <Eigen/Dense>
#include <Eigen/Core>

#include <string>
#include <vector>


// TODO: bag-of-everything-pattern-here so let's clean-up to separate entities when things are more
// settled in feature finder

namespace FeatureFinderUtils {
    PMI_COMMON_CORE_MINI_EXPORT Q_DECL_CONSTEXPR int hashMz(double mz, double granularity);  //hashMz()

    PMI_COMMON_CORE_MINI_EXPORT std::vector<double>
        extractUniformIntensity(const pmi::point2dList &scanPart, double mz, double mzRadius);
}

_PMI_BEGIN
Eigen::RowVectorXd relu(Eigen::RowVectorXd inputVector);
Eigen::RowVectorXd sigmoid(Eigen::RowVectorXd inputVector);
Eigen::RowVectorXd convertVectorToEigenRowVector(const std::vector<double> & inputVector);
Eigen::MatrixXd appendEigenMatrixWithVector(Eigen::MatrixXd inputMatrix, Eigen::VectorXd inputVector);
Err readNeuralNetworkWeightsSqlQT(const std::string &nnPath, int modelID, std::vector<Eigen::MatrixXd> * matrices);
Eigen::RowVectorXd convertPoint2dIntoVector(const pmi::point2dList &scan);
_PMI_END

#endif //CHARGE_DETERMINATION_NN_COMMONFUNCTIONS_H
