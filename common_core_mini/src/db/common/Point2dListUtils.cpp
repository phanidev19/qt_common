/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "Point2dListUtils.h"

_PMI_BEGIN

point2dList Point2dListUtils::extractPoints(const point2dList &scanData, double xStart, double xEnd)
{
    point2dList filtered;

    point2d xStartPt(xStart, 0);
    auto lower = std::lower_bound(scanData.cbegin(), scanData.cend(), xStartPt, point2d_less_x);

    point2d xEndPt(xEnd, 0);
    auto upper = std::lower_bound(scanData.cbegin(), scanData.cend(), xEndPt, point2d_less_x);

    filtered.insert(filtered.end(), lower, upper);

    return filtered;
}

point2dList Point2dListUtils::extractPointsIncluding(const point2dList &scanData, double xStart,
                                                     double xEnd)
{
    point2dList filtered;

    point2d xStartPt(xStart, 0);
    auto lower = std::lower_bound(scanData.cbegin(), scanData.cend(), xStartPt, point2d_less_x);

    point2d xEndPt(xEnd, 0);
    auto upper = std::upper_bound(scanData.cbegin(), scanData.cend(), xEndPt, point2d_less_x);

    filtered.insert(filtered.end(), lower, upper);

    return filtered;
}

_PMI_END