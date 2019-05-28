/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "NonUniformHill.h"

_PMI_BEGIN

NonUniformHill::NonUniformHill()
    : NonUniformHill(MzScanIndexRect(), QVector<NonUniformTilePoint>(), NonUniformHill::INVALID_ID)
{
}

NonUniformHill::NonUniformHill(const MzScanIndexRect &area,
                               const QVector<NonUniformTilePoint> points, int id)
    : m_id(id)
    , m_rect(area)
    , m_points(points)

{
}

NonUniformHill::~NonUniformHill()
{
}

void NonUniformHill::reset()
{
    m_points.clear();
    m_rect = MzScanIndexRect();
}

void NonUniformHill::append(const NonUniformTilePoint &point)
{
    m_points.append(point);
}

void NonUniformHill::push_back(const NonUniformTilePoint &point)
{
    m_points.push_back(point);
}

_PMI_END
