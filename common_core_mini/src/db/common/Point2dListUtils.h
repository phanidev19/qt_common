/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef POINT2D_LIST_UTILS_H
#define POINT2D_LIST_UTILS_H

#include "pmi_common_core_mini_export.h"

#include <common_math_types.h>
#include <pmi_core_defs.h>

_PMI_BEGIN

class PMI_COMMON_CORE_MINI_EXPORT Point2dListUtils {
public:
    //! \brief from the sorted input (sorted by x), extract all points starting by xStart included
    //! and til xEnd (excluded) [xStart, xEnd)
    static point2dList extractPoints(const point2dList &scanData, double xStart, double xEnd);

    //! \brief from the sorted input (sorted by x), extract all points starting by xStart included
    //! and til xEnd (included) [xStart, xEnd]
    static point2dList extractPointsIncluding(const point2dList &scanData, double xStart,
                                              double xEnd);
};

_PMI_END

#endif // POINT2D_LIST_UTILS_H