/*
 * Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Lukas Tvrdy ltvrdy@milosolutions.com
 */

#ifndef INTENSITY_PRIORITY_QUEUE_H
#define INTENSITY_PRIORITY_QUEUE_H

#include "pmi_core_defs.h"

#include "pmi_common_ms_export.h"

#include <queue>

#include <QPoint>
#include <QPointF>
#include <QVector>

_PMI_BEGIN

class PMI_COMMON_MS_EXPORT NonUniformIntensityPoint
{
public:
    bool operator()(const NonUniformIntensityPoint &lhs, const NonUniformIntensityPoint &rhs)
    {
        return lhs.mzIntensity.y() > rhs.mzIntensity.y();
    }

    QPointF mzIntensity;
    int scanIndex;
};

class PMI_COMMON_MS_EXPORT IntensityPoint
{
public:
    bool operator()(const IntensityPoint &lhs, const IntensityPoint &rhs)
    {
        return lhs.intensity > rhs.intensity;
    }

    // Uniform tile coordinate
    QPoint globalTilePos;
    double intensity;
};

typedef std::priority_queue<IntensityPoint, QVector<IntensityPoint>, IntensityPoint>
    IntensityPriorityQueue;

typedef std::priority_queue<NonUniformIntensityPoint, QVector<NonUniformIntensityPoint>,
                            NonUniformIntensityPoint>
    NonUniformIntensityPriorityQueue;

class PMI_COMMON_MS_EXPORT IntensityPriorityQueueUtils
{
public:
    static void pushWithLimit(IntensityPriorityQueue *queue, const IntensityPoint &point, int limit)
    {
        if (queue->size() < limit) {
            queue->push(point);
            return;
        }

        const IntensityPoint minElement = queue->top();
        if (minElement.intensity < point.intensity) {
            queue->pop();
            queue->push(point);
        }
    }
};

class PMI_COMMON_MS_EXPORT NonUniformIntensityPriorityQueueUtils
{
public:
    static void pushWithLimit(NonUniformIntensityPriorityQueue *queue,
                              const NonUniformIntensityPoint &point, int limit)
    {
        if (queue->size() < limit) {
            queue->push(point);
            return;
        }

        const NonUniformIntensityPoint minElement = queue->top();
        if (minElement.mzIntensity.y() < point.mzIntensity.y()) {
            queue->pop();
            queue->push(point);
        }
    }
};

_PMI_END

#endif // INTENSITY_PRIORITY_QUEUE_H