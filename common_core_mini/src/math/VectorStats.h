/*
* Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef VECTOR_STATS_H
#define VECTOR_STATS_H

#include "pmi_common_core_mini_export.h"

#include <pmi_core_defs.h>

#include <algorithm>
#include <iterator>
#include <numeric>
#include <vector>

_PMI_BEGIN

template <class T>
class PMI_COMMON_CORE_MINI_EXPORT VectorStats
{

public:
    explicit VectorStats(const std::vector<T> &data)
    {
        if (data.empty()) {
            return;
        }

        std::vector<T> copyData = data;
        std::sort(copyData.begin(), copyData.end());

        min = copyData.front();
        max = copyData.back();

        T sum = std::accumulate(copyData.begin(), copyData.end(), 0.0);
        average = sum / static_cast<double>(copyData.size());

        // this is not a real median, in case when we have even numbers then 
        // the median should be equal to the arithmetic mean of (n / 2) and (n / 2 + 1)
        // here we chose floor(n / 2)
        int medianPos = static_cast<int>(std::floor(copyData.size() * 0.5));
        median = copyData.at(medianPos);
    }

    static double mean(typename std::vector<T>::const_iterator begin,
        typename std::vector<T>::const_iterator end)
    {
        ptrdiff_t size = std::distance(begin, end);
        if (size <= 0) {
            return 0.0;
        }
        double sum = std::accumulate(begin, end, 0.0);
        return sum / static_cast<double>(size);
    }

    static double stdDev(typename std::vector<T>::const_iterator begin,
        typename std::vector<T>::const_iterator end)
    {
        ptrdiff_t size = std::distance(begin, end);
        if (size <= 0) {
            return 0.0;
        }

        double meanValue = mean(begin, end);

        std::vector<double> diff(static_cast<size_t>(size));
        std::transform(begin, end, diff.begin(), [meanValue](double x) { return x - meanValue; });

        double squaredSum = std::inner_product(diff.begin(), diff.end(), diff.begin(), 0.0);
        double stdDev = std::sqrt(squaredSum / size);
        return stdDev;
    }

    T min = T();
    T max = T();
    T average = T();
    T median = T();
};

template class PMI_COMMON_CORE_MINI_EXPORT VectorStats<double>;

_PMI_END

#endif // VECTOR_STATS_H