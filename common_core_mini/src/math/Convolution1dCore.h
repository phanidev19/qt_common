/*
 * Copyright (C) 2012-2014 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 */
 
#ifndef CONVOLUTION1D_CORE_H
#define CONVOLUTION1D_CORE_H

#include <vector>
#include <QList>
#include "pmi_common_core_mini_export.h"
#include "common_errors.h"

#define kBoundaryTypeZero 0
#define kBoundaryTypeCopyEnd 1

_PMI_BEGIN
_CORE_BEGIN

PMI_COMMON_CORE_MINI_EXPORT bool conv(const std::vector<double> & in, const std::vector<double> & kernel, int boundary_type, std::vector<double> & out);

PMI_COMMON_CORE_MINI_EXPORT bool convForReal(const std::vector<double> & in, const std::vector<double> & kernel, std::vector<double> & out);

PMI_COMMON_CORE_MINI_EXPORT bool makeGaussianKernel(double std_dev, std::vector<double> & kernel);

PMI_COMMON_CORE_MINI_EXPORT bool makeSavitzkyGolayKernel(const int order, const int numb_pnts, std::vector<double> & vkrn);

_CORE_END
_PMI_END

#endif
