/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef COSINE_CORRELATOR_H
#define COSINE_CORRELATOR_H

#include "pmi_common_core_mini_export.h"

#include <pmi_core_defs.h>

#include <common_math_types.h>

#include <QVector>

_PMI_BEGIN

/**
* @brief Computes cosine similarity between two vectors
*        
* Allows you to work with two vectors that are not aligned properly with x elements.
* E.g. XIC data have x elements time, y elements intensity. You can compute correlation 
* between two arbitrary xic windows where \a first acts as the master and second vector 
* is zero-padded if some x element is missing
*
* You can also directly use cosineSimilarity to compute similarity between
* vectors which are already aligned properly
*/
class PMI_COMMON_CORE_MINI_EXPORT CosineCorrelator {

public:    
    /**
    * @brief \a first is the master vector to which the other vectors will be padded
    * It should NOT be empty, otherwise the behaviour is undefined
    */
    explicit CosineCorrelator(const point2dList &first);
    
    ~CosineCorrelator();

    /**
    * @brief \a second vector to which cosine similarity will be provided
    * If it is not aligned to master vector, it will be padded properly with zeros
    */
    double cosineSimilarityTo(const point2dList &second);

    /**
    * @brief Computes cosine similarity between two vectors, see https://en.wikipedia.org/wiki/Cosine_similarity
    */
    static double cosineSimilarity(const QVector<double> &first, const QVector<double> &second);

private:
    // provides zero-padded vector to m_first
    point2dList copyYForMatchingX(double xStart, double xEnd, const point2dList &second);

    QVector<double> yValues(const point2dList &data);

private:
    point2dList m_first;
    point2dList m_firstYZeroed;
    QVector<double> m_firstY;
};

_PMI_END

#endif // COSINE_CORRELATOR_H