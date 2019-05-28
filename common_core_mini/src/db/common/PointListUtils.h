/*
 * Copyright (C) 2012-2014 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 */

#ifndef __POINT_LIST_UTILS_H__
#define __POINT_LIST_UTILS_H__

#include <limits>
#include <pmi_core_defs.h>
#include <common_types.h>

_PMI_BEGIN

inline void
scale_height(point2dList & plist, double scale) {
    for (unsigned int i = 0; i < plist.size(); i++) {
        plist[i].ry() *= scale;
    }
}

inline double
point2dMin(const point2dList & plist, pt2idx * index = 0) {
    if (plist.size() <= 0) {
        if (index)
            *index = -1;
        return -1.0 * (std::numeric_limits<double>::max());
    }
    double minval = plist[0].y();
    pt2idx minindex = 0;

    for (unsigned int i = 1; i < plist.size(); i++) {
        if (minval > plist[i].y()) {
            minval = plist[i].y();
            minindex = i;
        }
    }
    if (index)
        *index = minindex;
    return minval;
}


inline double
point2dMax(const point2dList & plist, pt2idx * index = 0) {
    if (plist.size() <= 0) {
        if (index)
            *index = -1;
        return (std::numeric_limits<double>::max());
    }
    double maxval = plist[0].y();
    pt2idx maxindex = 0;

    for (unsigned int i = 1; i < plist.size(); i++) {
        if (maxval < plist[i].y()) {
            maxval = plist[i].y();
            maxindex = i;
        }
    }
    if (index)
        *index = maxindex;
    return maxval;
}

_PMI_END

#endif

