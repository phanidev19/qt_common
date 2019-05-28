/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "NonUniformHillCluster.h"
#include "NonUniformTilePoint.h"

inline uint qHash(const QPoint &pos, uint seed)
{
    QtPrivate::QHashCombine hash;
    seed = hash(seed, static_cast<uint>(pos.x()));
    seed = hash(seed, static_cast<uint>(pos.y()));
    return seed;
}

_PMI_BEGIN

NonUniformHillCluster::NonUniformHillCluster(const QVector<NonUniformHill> &hills)
    : m_id(0)
    , m_charge(0)
    , m_scanIndex(0)
    , m_monoisotopicMz(0.0)
    , m_maximumIntensity(0.0)
    , m_hills(hills)
{
}

NonUniformHillCluster::NonUniformHillCluster()
    : NonUniformHillCluster(QVector<NonUniformHill>())
{
}

NonUniformHillCluster::~NonUniformHillCluster()
{
}

void NonUniformHillCluster::push_back(const NonUniformHill &hill)
{
    m_hills.push_back(hill);
}

bool NonUniformHillCluster::isNull() const
{
    return m_hills.isEmpty();
}

int NonUniformHillCluster::totalPoints() const
{
    int result = 0;
    for (const NonUniformHill &hill : qAsConst(m_hills)) {
        result += hill.points().size();
    }
    return result;
}

QSet<QPoint> NonUniformHillCluster::uniqueTilePositions() const
{
    QSet<QPoint> result;

    for (const NonUniformHill &hill : qAsConst(m_hills)) {
        for (const NonUniformTilePoint &pt : hill.points()) {
            result.insert(pt.tilePos);
        }
    }

    return result;
}

ScanIndexInterval NonUniformHillCluster::minMaxScanIndex() const
{
    ScanIndexInterval result;
    bool first = false;
    
    for (const NonUniformHill &hill : m_hills) {
        if (!first) {
            result = hill.area().scanIndex;
            first = true;
        } else {
            const MzScanIndexRect &area = hill.area();
            result.rstart() = std::min(result.start(), area.scanIndex.start());
            result.rend() = std::max(result.end(), area.scanIndex.end());
        }
    }

    return result;
}

bool NonUniformHillCluster::isEmpty() const
{
    if (m_hills.isEmpty()) {
        return true;
    }

    if (totalPoints() == 0) {
        return true;
    }

    return false;
}


_PMI_END
