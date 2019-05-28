/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Andrew Nichols anichols@proteinmetrics.com
*/

#include "ClusteringDBSCAN.h"
#include "pmi_common_core_mini_debug.h"

// fix warning: undefine _USE_MATH_DEFINES as nanoflann.hpp is defining it 
#undef _USE_MATH_DEFINES
#include "nanoflann.hpp"
#ifndef _USE_MATH_DEFINES
#error "nanoflann.hpp was responsible to define _USE_MATH_DEFINES here. We can remove the #undef _USE_MATH_DEFINES if this fails
#endif

_PMI_BEGIN

ClusteringDBSCAN::ClusteringDBSCAN(double eps, int minSample)
    : m_eps(eps)
    , m_minSample(minSample)
{
}

ClusteringDBSCAN::~ClusteringDBSCAN()
{
}

Eigen::VectorXi ClusteringDBSCAN::performDBSCAN(const Eigen::MatrixXd &mat) const
{

    typedef nanoflann::KDTreeEigenMatrixAdaptor<Eigen::MatrixXd> KDTree;
    int maxTreeLeafSize = 30;
    KDTree index(mat.cols(), mat, maxTreeLeafSize);
    
    // This must be squared because NearNbrs library uses reduced distance metric
    const double searchRadius = std::pow(m_eps, 2);
    std::vector<std::pair<Eigen::Index, double>> matches;
    nanoflann::SearchParams params;

    std::vector<std::vector<int>> neighborhoods;
    std::vector<double> query_pt(mat.cols());
    neighborhoods.reserve(mat.rows());
    for (int y = 0; y < mat.rows(); ++y) {
        for (int z = 0; z < mat.cols(); ++z) {
            query_pt[z] = mat(y, z);
        }

        const size_t nMatches
            = index.index->radiusSearch(&query_pt[0], searchRadius, matches, params);
        
        std::vector<int> rowNeighbors;
        rowNeighbors.reserve(nMatches);
        for (size_t i = 0; i < nMatches; i++) {
            rowNeighbors.push_back(matches[i].first);
        }
        
        std::sort(rowNeighbors.begin(), rowNeighbors.end());
        neighborhoods.push_back(rowNeighbors);
    }

    return dbscan(m_minSample, neighborhoods);
}

Eigen::VectorXi ClusteringDBSCAN::dbscan(int minPoints,
                                         const std::vector<std::vector<int>> &neighborhoods) const
{
    Eigen::VectorXi labels(neighborhoods.size());
    labels.setOnes();
    labels *= -1;

    std::vector<int> nneighborhoods(neighborhoods.size());
    std::vector<bool> coreSamples(neighborhoods.size());
    for (size_t y = 0; y < neighborhoods.size(); ++y) {
        int tvectorSize = static_cast<int>(neighborhoods[y].size());
        nneighborhoods[y] = tvectorSize;
        bool v = tvectorSize >= minPoints ? 1 : 0;
        coreSamples[y] = v;
    }

    dbscanInternal(coreSamples, neighborhoods, labels);

    return labels;
}

void ClusteringDBSCAN::dbscanInternal(const std::vector<bool> &isCore,
                                  const std::vector<std::vector<int>> &neighborhoods,
                                  Eigen::VectorXi &labels) const
{
    int labelNum = 0;
    std::vector<int> stack;
    for (int i = 0; i < labels.size(); ++i) {
        if (labels(i) != -1 || !isCore[i]) {
            continue;
        }
        
        while (true) {
            if (labels(i) == -1) {
                labels(i) = labelNum;
                if (isCore[i]) {
                    std::vector<int> neighb = neighborhoods[i];
                    for (size_t j = 0; j < neighb.size(); ++j) {
                        int v = neighb[j];
                        if (labels[v] == -1) {
                            stack.push_back(v);
                        }
                    }
                }
            }
            if (stack.size() == 0) {
                break;
            }
            i = stack.back();
            stack.pop_back();
        }
        labelNum++;
    }
}

_PMI_END
