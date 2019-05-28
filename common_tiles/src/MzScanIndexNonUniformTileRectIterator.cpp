/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "MzScanIndexNonUniformTileRectIterator.h"

#include "MzScanIndexRect.h"
#include "SequentialNonUniformTileIterator.h"
#include "pmi_common_tiles_debug.h"

// common_core_mini
#include "Point2dListUtils.h"

_PMI_BEGIN

class Q_DECL_HIDDEN MzScanIndexNonUniformTileRectIterator::Private
{
public:
    Private(NonUniformTileManager *tileManager, const NonUniformTileRange &range, bool doCentroiding,
            const MzScanIndexRect &a)
        : area(a)
        , tileIterator(
              tileManager, range, doCentroiding,
              range.tileRect(a.mz.start(), a.mz.end(), a.scanIndex.start(), a.scanIndex.end()))
    {
    }

    ~Private() {}

    MzScanIndexRect area;
    point2dList lastScanPart;

    SequentialNonUniformTileIterator tileIterator;

    MzInterval mz;

    bool firstDone = false;

    int internalIndex = -1;
};

MzScanIndexNonUniformTileRectIterator::MzScanIndexNonUniformTileRectIterator(
    NonUniformTileManager *tileManager, const NonUniformTileRange &range, bool doCentroiding,
    const MzScanIndexRect &area)
    : d(new Private(tileManager, range, doCentroiding, area))
{
    Q_ASSERT(!area.isNull());
    Q_ASSERT(range.contains(area));
    init();
}

MzScanIndexNonUniformTileRectIterator::~MzScanIndexNonUniformTileRectIterator()
{
}

void MzScanIndexNonUniformTileRectIterator::init()
{
    d->mz.rstart() = d->area.mz.start();
    d->mz.rend() = d->mz.start();

    d->tileIterator.restrictScanIndexInterval(d->area.scanIndex);
}

void MzScanIndexNonUniformTileRectIterator::advance()
{
    extractValidMzRange(d->tileIterator.next());
}

point2dList extractPointsIncluding(const point2dList &scanData, double xStart, double xEnd, int *index)
{
    point2dList filtered;

    point2d xStartPt(xStart, 0);
    auto lower = std::lower_bound(scanData.cbegin(), scanData.cend(), xStartPt, point2d_less_x);
    
    if (lower != scanData.end()) {
        *index = static_cast<int>(std::distance(scanData.cbegin(), lower));
    }

    point2d xEndPt(xEnd, 0);
    auto upper = std::upper_bound(scanData.cbegin(), scanData.cend(), xEndPt, point2d_less_x);

    filtered.insert(filtered.end(), lower, upper);

    return filtered;
}


void MzScanIndexNonUniformTileRectIterator::extractValidMzRange(const point2dList &scanPart)
{
    double tileMzStart = d->tileIterator.mzStart();
    double tileMzEnd = d->tileIterator.mzEnd();

    d->mz.rstart() = qMax(d->area.mz.start(), tileMzStart);
    d->mz.rend() = qMin(d->area.mz.end(), tileMzEnd);

    // avoid calling extractPointsIncluding when you are in tile that contains just mz within
    // desired area
    if (tileMzStart >= d->area.mz.start() && tileMzEnd <= d->area.mz.end()) {
        d->lastScanPart = scanPart;
        d->internalIndex = 0;
    } else {
        // -1 means that scanPart was empty, that can happen
        int index = -1;
        // cut-out current part
        d->lastScanPart = extractPointsIncluding(scanPart, d->mz.start(), d->mz.end(), &index);
        d->internalIndex = index;
    }
}

int MzScanIndexNonUniformTileRectIterator::x() const
{
    return d->tileIterator.x();
}

int MzScanIndexNonUniformTileRectIterator::y() const
{
    return d->tileIterator.y();
}

int MzScanIndexNonUniformTileRectIterator::scanIndex() const
{
    return d->tileIterator.scanIndex();
}

double MzScanIndexNonUniformTileRectIterator::mzStart() const
{
    return d->mz.start();
}

double MzScanIndexNonUniformTileRectIterator::mzEnd() const
{
    return d->mz.end();
}

bool MzScanIndexNonUniformTileRectIterator::hasNext() const
{
    return (d->tileIterator.hasNext());
}

point2dList MzScanIndexNonUniformTileRectIterator::next()
{
    advance();
    return d->lastScanPart;
}

point2dList MzScanIndexNonUniformTileRectIterator::value() const
{
    return d->lastScanPart;
}

// TODO: auto test this 
int MzScanIndexNonUniformTileRectIterator::internalIndex() const
{
    return d->internalIndex;
}


_PMI_END
