/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/


#include "CosineCorrelator.h"

#include "pmi_common_core_mini_debug.h"

_PMI_BEGIN

CosineCorrelator::CosineCorrelator(const point2dList &first)
    : m_first(first)
{
    Q_ASSERT(!first.empty());
    
    m_firstYZeroed.reserve(m_first.size());
    for (const point2d &pt : m_first) {
        m_firstYZeroed.push_back(point2d(pt.x(), 0.0));
    }

    m_firstY = yValues(m_first);
}

CosineCorrelator::~CosineCorrelator()
{

}

double CosineCorrelator::cosineSimilarity(const QVector<double> &first, const QVector<double> &second)
{
    double result = 0.0;
    if (first.size() != second.size()) {
        return result;
    }

    // sum Ai * Bi
    double product = std::inner_product(first.begin(), first.end(), second.begin(), 0.0);

    // sum Ai * Ai
    double firstProduct = std::inner_product(first.begin(), first.end(), first.begin(), 0.0);

    // sum Bi * Bi
    double secondProduct = std::inner_product(second.begin(), second.end(), second.begin(), 0.0);

    if (firstProduct == 0.0 || secondProduct == 0) {
        return result;
    }

    result = product / (sqrt(firstProduct) * sqrt(secondProduct));

    return result;
}


point2dList CosineCorrelator::copyYForMatchingX(double xStart, double xEnd, const point2dList &second)
{
    // make same size vector as parent
    point2dList normalized = m_firstYZeroed;

    // copy the part 
    auto startXIterSrc = std::lower_bound(second.begin(), second.end(), point2d(xStart, 0.0), [](const point2d &lhs, const point2d &rhs) {
        return lhs.x() < rhs.x();
    });

    // this will not happen, but let's check for this 
    Q_ASSERT(startXIterSrc != second.end());
    Q_ASSERT(!(startXIterSrc->x() < xStart));

    auto endXIterSrc = std::lower_bound(second.begin(), second.end(), point2d(xEnd, 0.0), [](const point2d &lhs, const point2d &rhs) {
        return lhs.x() < rhs.x();
    });

    // this will not happen, but let's check for this 
    Q_ASSERT(endXIterSrc != second.end());
    Q_ASSERT(!(endXIterSrc->x() < xEnd));

    // find the place in the dst where to copy the values
    auto startXIterDst = std::lower_bound(normalized.begin(), normalized.end(), point2d(xStart, 0.0), [](const point2d &lhs, const point2d &rhs) {
        return lhs.x() < rhs.x();
    });

    // this will not happen, but let's check for this 
    Q_ASSERT(startXIterDst != normalized.end());

    ptrdiff_t distance = std::distance(normalized.begin(), startXIterDst);
    std::copy(startXIterSrc, std::next(endXIterSrc), normalized.begin() + distance);

    return normalized;
}

QVector<double> CosineCorrelator::yValues(const point2dList &data)
{
    QVector<double> result;
    result.reserve(static_cast<int>(data.size()));

    for (const point2d &pt : data) {
        result.push_back(pt.y());
    }

    return result;
}

double CosineCorrelator::cosineSimilarityTo(const point2dList &second)
{
    if (second.empty()) {
        return 0.0;
    }

    // compute intersection between m_first and second
    double firstStartX = m_firstYZeroed.front().x();
    double firstEndX = m_firstYZeroed.back().x();

    double childStartX = second.front().x();
    double childEndX = second.back().x();

    double intersectedStartX = std::max(firstStartX, childStartX);
    double intersectedEndX = std::min(firstEndX, childEndX);

    if (intersectedStartX > intersectedEndX) {
        warningCoreMini() << "First and second does not intersect!";
        return 0.0;
    }
    
    point2dList normalized  = copyYForMatchingX(intersectedStartX, intersectedEndX, second);
    
    QVector<double> secondY = yValues(normalized);
    
    return cosineSimilarity(m_firstY, secondY);
}


_PMI_END

