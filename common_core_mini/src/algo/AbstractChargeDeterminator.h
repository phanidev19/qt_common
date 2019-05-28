/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef ABSTRACT_CHARGE_DETERMINATOR_H
#define ABSTRACT_CHARGE_DETERMINATOR_H

#include "pmi_common_core_mini_export.h"

#include <common_math_types.h>
#include <pmi_core_defs.h>

_PMI_BEGIN

class PMI_COMMON_CORE_MINI_EXPORT AbstractChargeDeterminator
{

public:
    virtual ~AbstractChargeDeterminator() {}

    virtual int determineCharge(const point2dList &scanPart, double mz) = 0;
	
};

_PMI_END

#endif // ABSTRACT_CHARGE_DETERMINATOR_H