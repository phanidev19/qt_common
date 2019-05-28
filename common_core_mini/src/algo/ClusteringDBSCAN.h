/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Andrew Nichols anichols@proteinmetrics.com
*/

#ifndef CLUSTERING_DBSCAN_H
#define CLUSTERING_DBSCAN_H

#include "pmi_common_core_mini_export.h"

#include <pmi_core_defs.h>

#include <Eigen/Core>

#include <vector>

_PMI_BEGIN

/*!
 * @brief Implements DBSCAN clustering algorithm, see https://en.wikipedia.org/wiki/DBSCAN for more
 * details.
 *
 * Example of usage:
 *
 * //Build Test matrix
 * int theSize = 1000000;
 * Eigen::MatrixXd mat = Eigen::MatrixXd::Random(theSize, 2);
 * mat *= 10000;
 *
 * //Usage
 * ClusteringDBSCAN dbs(1.4, 2);
 * Eigen::VectorXi labels = dbs.performDBSCAN(mat);
 *
 */
class PMI_COMMON_CORE_MINI_EXPORT ClusteringDBSCAN
{
public:
    /*!
     * @param eps - defines neightborhood distance for the points
     * @param minSample - minimum number of points required to form a dense region
     */
    ClusteringDBSCAN(double eps, int minSample);

    ~ClusteringDBSCAN();

    // TODO: docs regarding mat
    Eigen::VectorXi performDBSCAN(const Eigen::MatrixXd &mat) const;

private:
    Eigen::VectorXi dbscan(int minPoints, const std::vector<std::vector<int>> &neighborhoods) const;
    void dbscanInternal(const std::vector<bool> &isCore,
                     const std::vector<std::vector<int>> &neighborhoods, Eigen::VectorXi &labels) const;

private:
    double m_eps;
    int m_minSample;
};

_PMI_END

#endif // CLUSTERING_DBSCAN_H
