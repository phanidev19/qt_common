/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "NonUniformFeature.h"

_PMI_BEGIN

NonUniformFeature::NonUniformFeature(const QVector<NonUniformHillCluster> &chargeClusters)
    : m_id(0)
    , m_unchargedMass(0.0)
    , m_chargeClusters(chargeClusters)
{
}

NonUniformFeature::NonUniformFeature()
    : NonUniformFeature(QVector<NonUniformHillCluster>())
{
}

NonUniformFeature::~NonUniformFeature()
{
}

QVector<NonUniformHillCluster> NonUniformFeature::chargeClusters() const
{
    return m_chargeClusters;
}

void NonUniformFeature::append(const NonUniformHillCluster& chargeCluster)
{
    m_chargeClusters.append(chargeCluster);
}

bool NonUniformFeature::isNull() const
{
    return m_chargeClusters.isEmpty() && (m_id == 0);
}


ScanIndexInterval NonUniformFeature::minMaxScanIndex() const
{
    ScanIndexInterval result;
    bool first = false;
    for (const NonUniformHillCluster &cluster : qAsConst(m_chargeClusters)) {
        const ScanIndexInterval clusterInterval = cluster.minMaxScanIndex();
        Q_ASSERT(!clusterInterval.isNull());
        
        if (!first) {
            result = clusterInterval;
            first = true; 
        } else {
            result.rstart() = std::min(result.start(), clusterInterval.start());
            result.rend() = std::max(result.end(), clusterInterval.end());
        }
    }

    return result;
}

double NonUniformFeature::maximumIntensity() const
{
    double result = 0.0;

    if (!m_chargeClusters.isEmpty()) {
        // the main cluster is first in the list 
        const int mainClusterIndex = 0;
        result = m_chargeClusters.at(mainClusterIndex).maximumIntensity();
    }

    return result;
}


QString NonUniformFeature::chargeList() const
{
    QStringList charges;
    charges.reserve(m_chargeClusters.size());
    for (const NonUniformHillCluster &cluster : qAsConst(m_chargeClusters)) {
        charges.push_back(QString::number(cluster.charge()));
    }
    
    const QChar chargeSeparator(',');
    QString result = charges.join(chargeSeparator);
    
    return result;
}

_PMI_END

