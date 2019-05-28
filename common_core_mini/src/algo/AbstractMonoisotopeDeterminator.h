/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef ABSTRACT_MONOISOTOPE_DETERMINATOR_H
#define ABSTRACT_MONOISOTOPE_DETERMINATOR_H

#include "pmi_common_core_mini_export.h"

#include <common_math_types.h>
#include <pmi_core_defs.h>

_PMI_BEGIN

class AbstractMonoisotopeDeterminator {

public:    
    virtual ~AbstractMonoisotopeDeterminator() {}

    virtual int determineMonoisotopeOffset(const pmi::point2dList &scanPart, double mz, int charge,
                                           double *score) = 0;
};

_PMI_END

#endif // ABSTRACT_MONOISOTOPE_DETERMINATOR_H