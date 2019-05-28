/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef NONUNIFORM_HILL_H
#define NONUNIFORM_HILL_H

#include "pmi_common_ms_export.h"

#include <NonUniformTilePoint.h>
#include <MzScanIndexRect.h>

#include <pmi_core_defs.h>

#include <QVector>


_PMI_BEGIN

/*!
\class NonUniformHill
\brief NonUniformHill represents hills found across MS1 scan data 

Not every point existing in \a area() belongs to the hill
Only points in \a points() do, their bounding box is \a area

\ingroup Feature finder for tiles

\reentrant
*/
class NonUniformHill
{

public:
    static const int INVALID_ID = -1;

    NonUniformHill();
    NonUniformHill(const MzScanIndexRect &area, const QVector<NonUniformTilePoint> points,
                   int id = NonUniformHill::INVALID_ID);
    ~NonUniformHill();

    MzScanIndexRect area() const { return m_rect; }

    QVector<NonUniformTilePoint> points() const { return m_points; }

    void setId(int id) { m_id = id; }
    int id() const { return m_id; }

    void setCorrelation(double correlation) { m_correlation = correlation; }
    double correlation() const { return m_correlation; }

    bool isNull() { return m_points.isEmpty(); }

    void reset();

    void append(const NonUniformTilePoint &point);

    void push_back(const NonUniformTilePoint &point);

    // support for range-for
    QVector<NonUniformTilePoint>::iterator begin() { return m_points.begin(); }
    QVector<NonUniformTilePoint>::iterator end() { return m_points.end(); }
    QVector<NonUniformTilePoint>::const_iterator begin() const { return m_points.begin(); }
    QVector<NonUniformTilePoint>::const_iterator end() const { return m_points.end(); }

private:
    int m_id;
    double m_correlation;

    MzScanIndexRect m_rect;
    QVector<NonUniformTilePoint> m_points;
};

_PMI_END

#endif // NONUNIFORM_HILL_H