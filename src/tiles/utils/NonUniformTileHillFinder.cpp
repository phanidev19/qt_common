/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

// common_tiles
#include "NonUniformTileHillFinder.h"

#include "MzScanIndexNonUniformTileRectIterator.h"
#include "MzScanIndexRect.h"
#include "NonUniformTileDevice.h"
#include "NonUniformTilePoint.h"
#include "NonUniformTileRange.h"
#include "NonUniformTileStore.h"
#include "RandomNonUniformTileIterator.h"

// common_ms
#include "NonUniformFeatureFindingSession.h"
#include "NonUniformHill.h"
#include "ScanIndexTimeConverter.h"
#include "ZScorePeakDetector.h"
#include "pmi_common_ms_debug.h"

// common_core_mini
#include <MzInterval.h>

// common_stable
#include <common_errors.h>

#include <unordered_map>
#include <vector>

#include <QDir>
#include <QtMath>

_PMI_BEGIN

class Q_DECL_HIDDEN NonUniformTileHillFinder::Private
{
public:
    Private(NonUniformFeatureFindingSession *ses)
        : session(ses)
        , device(session->device())
        , searchArea(ses->searchArea())
    {
    }

    NonUniformFeatureFindingSession *session;
    NonUniformTileDevice *device;
    MzScanIndexRect searchArea;
    double mzWidth = 0.05;
    HillAlgorithm algorithm = HillAlgorithm::ZeroBounded;
    int hillId = 0;
};

NonUniformTileHillFinder::NonUniformTileHillFinder(NonUniformFeatureFindingSession *session)
    : d(new Private(session))
{
}

NonUniformTileHillFinder::~NonUniformTileHillFinder()
{
}

NonUniformHill
NonUniformTileHillFinder::explainPeak(NonUniformTileSelectionManager *selectionTileManager,
                                      double mz, int scanIndex)
{
    NonUniformHill result;
    switch (d->algorithm) {
    case HillAlgorithm::ZScoreIntegration: {
        result = buildHillWithIntegration(selectionTileManager, mz, scanIndex);
        break;
    }
    case HillAlgorithm::ZeroBounded: {
        result = buildHillWithNonZeroSignal(selectionTileManager, mz, scanIndex);
        break;
    }
    default:
        qFatal("Unhandled algorithm type!");
        break;
    }

    return result;
}

NonUniformHill NonUniformTileHillFinder::buildHillWithNonZeroSignal(
    NonUniformTileSelectionManager *selectionTileManager, double mz, int scanIndex)
{
    NonUniformHill result;

    // how many scans can be empty in both directions
    const int consequtiveEmptyScanIndexLimit = 1;

    if (!d->searchArea.mz.contains(mz) || !d->searchArea.scanIndex.contains(scanIndex)) {
        return result;
    }

    RandomNonUniformTileIterator tileIterator = d->device->createRandomIterator();

    const NonUniformTileRange &range = d->device->range();

    RandomNonUniformTileSelectionIterator selectionIterator(selectionTileManager, range,
                                                            d->device->doCentroiding());

    // compute hill's coordinate mz range we accept
    double hillMz = mz;

    MzScanIndexRect rect;
    QVector<NonUniformTilePoint> hillPoints;

    rect.scanIndex.rstart() = scanIndex;
    rect.scanIndex.rend() = scanIndex;

    const double mzStart = qBound(d->searchArea.mz.start(), hillMz - d->mzWidth * 0.5, d->searchArea.mz.end());
    const double mzEnd = qBound(d->searchArea.mz.start(), hillMz + d->mzWidth * 0.5, d->searchArea.mz.end());

    const point2d mzStartPt(mzStart, 0.0);
    const point2d mzEndPt(mzEnd, 0.0);

    rect.mz.rstart() = hillMz;
    rect.mz.rend() = hillMz;

    int tileXStart = range.tileX(rect.mz.start());
    int tileXEnd = range.tileX(rect.mz.end());

    // find bottom boundary of the feature,
    // go down til we hit scan index which does not contain any centroided point in given mz range
    // bounded by search area
    int currentScanIndex = scanIndex;
    int lastScanIndex = std::min(range.scanIndexMax(), d->searchArea.scanIndex.end());
    int consequtiveEmptyScansObserved = 0;
    while (currentScanIndex <= lastScanIndex) {
        int currentScanIndexPoints = 0;

        // take everything that is in tiles at the current scan index and defined mz
        int tileY = range.tileY(currentScanIndex);
        for (int tileX = tileXStart; tileX <= tileXEnd; ++tileX) {
            tileIterator.moveTo(tileX, tileY, currentScanIndex);
            const point2dList &scanData = tileIterator.value();
            // find first index
            auto firstItem
                = std::lower_bound(scanData.cbegin(), scanData.cend(), mzStartPt, point2d_less_x);
            if (firstItem == scanData.cend()) {
                // go to next tile than
                continue;
            } else {

                int firstIndex = std::distance(scanData.cbegin(), firstItem);

                auto lastItem
                    = std::upper_bound(scanData.cbegin(), scanData.cend(), mzEndPt, point2d_less_x);
                int lastIndex = std::distance(scanData.cbegin(), lastItem);
                for (int i = firstIndex; i < lastIndex; ++i) {
                    // if is selected, skip it
                    selectionIterator.moveTo(tileX, tileY, currentScanIndex);
                    if (selectionIterator.value().testBit(i)) {
                        continue;
                    }

                    NonUniformTilePoint pt;
                    pt.tilePos.rx() = tileX;
                    pt.tilePos.ry() = tileY;
                    pt.scanIndex = currentScanIndex;
                    pt.internalIndex = i;

                    // and what if the point intensity is 0.0 ?? for now we take them
                    double candidateMz = scanData.at(i).x();
                    rect.mz.rstart() = std::min(rect.mz.start(), candidateMz);
                    rect.mz.rend() = std::max(rect.mz.end(), candidateMz);

                    hillPoints.push_back(pt);
                    currentScanIndexPoints++;
                }
            }
        }

        // we didn't find any centroided points, let's end our adventure
        if (currentScanIndexPoints > 0) {
            currentScanIndex++;
            rect.scanIndex.rend() = currentScanIndex;
            consequtiveEmptyScansObserved = 0;
        } else {
            consequtiveEmptyScansObserved++;
            if (consequtiveEmptyScansObserved > consequtiveEmptyScanIndexLimit) {
                break;
            } else {
                // just move the index, don't extend the hill bounding box
                currentScanIndex++;
            }
        }
    }

    // if we didn't find any centroided points, let's end our adventure
    if (hillPoints.isEmpty()) {
        return result;
    }

    // reset the counter when going down from initial scanindex
    consequtiveEmptyScansObserved = 0;

    // go up now
    // TODO: merge with go down ^ (copy-pasta + tweaks)
    currentScanIndex = scanIndex - 1;
    int firstScanIndex = std::max(range.scanIndexMin(), d->searchArea.scanIndex.start());
    while (currentScanIndex >= firstScanIndex) {
        int currentScanIndexPoints = 0;

        // take everything that is in tiles at the current scan index and defined mz
        int tileY = range.tileY(currentScanIndex);
        for (int tileX = tileXStart; tileX <= tileXEnd; ++tileX) {
            tileIterator.moveTo(tileX, tileY, currentScanIndex);
            const point2dList &scanData = tileIterator.value();
            // find first index
            auto firstItem
                = std::lower_bound(scanData.cbegin(), scanData.cend(), mzStartPt, point2d_less_x);
            if (firstItem == scanData.cend()) {
                // go to next tile than
                continue;
            } else {

                int firstIndex = std::distance(scanData.cbegin(), firstItem);

                auto lastItem
                    = std::upper_bound(scanData.cbegin(), scanData.cend(), mzEndPt, point2d_less_x);
                int lastIndex = std::distance(scanData.cbegin(), lastItem);
                for (int i = firstIndex; i < lastIndex; ++i) {
                    // if is selected, skip it
                    selectionIterator.moveTo(tileX, tileY, currentScanIndex);
                    if (selectionIterator.value().testBit(i)) {
                        continue;
                    }

                    NonUniformTilePoint pt;
                    pt.tilePos.rx() = tileX;
                    pt.tilePos.ry() = tileY;
                    pt.scanIndex = currentScanIndex;
                    pt.internalIndex = i;

                    // and what if the point intensity is 0.0 ?? for now we take them
                    double candidateMz = scanData.at(i).x();
                    rect.mz.rstart() = std::min(rect.mz.start(), candidateMz);
                    rect.mz.rend() = std::max(rect.mz.end(), candidateMz);

                    hillPoints.push_back(pt);
                    currentScanIndexPoints++;
                }
            }
        }

        // we didn't find any centroid points, lets end our adventure
        if (currentScanIndexPoints > 0) {
            rect.scanIndex.rstart() = currentScanIndex;
            currentScanIndex--;
            consequtiveEmptyScansObserved = 0;
        } else {
            consequtiveEmptyScansObserved++;
            if (consequtiveEmptyScansObserved > consequtiveEmptyScanIndexLimit) {
                break;
            } else {
                currentScanIndex--;
            }
        }
    }

    if (rect.mz.start() == rect.mz.end()) {
        rect.mz.rstart() = mzStart;
        rect.mz.rend() = mzEnd;
    }

    result = NonUniformHill(rect, hillPoints);
    return result;
}

NonUniformHill NonUniformTileHillFinder::buildHillWithIntegration(
    NonUniformTileSelectionManager *selectionTileManager, double mz, int scanIndex)
{
    // TODO: possible user-parameter
    const double INTEGRATION_TIME_LIMIT = 2.0; // minutes

    NonUniformHill result;
    if (!d->searchArea.mz.contains(mz) || !d->searchArea.scanIndex.contains(scanIndex)) {
        return result;
    }

    MzScanIndexRect window;

    const double mzStart
        = qBound(d->searchArea.mz.start(), mz - d->mzWidth * 0.5, d->searchArea.mz.end());
    const double mzEnd
        = qBound(d->searchArea.mz.start(), mz + d->mzWidth * 0.5, d->searchArea.mz.end());

    // from time domain to scan index domain
    const ScanIndexNumberConverter &converter = d->session->converter();
    double time = converter.scanIndexToScanTime(scanIndex);

    double windowTimeStart = time - INTEGRATION_TIME_LIMIT;
    double windowTimeEnd = time + INTEGRATION_TIME_LIMIT;

    int scanIndexStart = converter.getBestScanIndexFromTime(windowTimeStart);
    int scanIndexEnd = converter.getBestScanIndexFromTime(windowTimeEnd);

    scanIndexStart
        = qBound(d->searchArea.scanIndex.start(), scanIndexStart, d->searchArea.scanIndex.end());
    scanIndexEnd
        = qBound(d->searchArea.scanIndex.start(), scanIndexEnd, d->searchArea.scanIndex.end());

    window.mz = MzInterval(mzStart, mzEnd);
    window.scanIndex = ScanIndexInterval(scanIndexStart, scanIndexEnd);

    point2dList xicData;
    getXICDataWithSelection(window, xicData);
    if (xicData.empty()) {
        warningMs() << "Empty XIC!";
    }

    ZScoreSettings settings;
    settings.threshold = 4.0;
    settings.influence = 0.015;
    settings.lag = qCeil(0.25 * xicData.size());

    // convert XIC to list of intervals
    QVector<Interval<int>> intervals = ZScorePeakDetector::findPeaks(xicData, settings);

    // check if the maxima is in the interval and use that interval
    auto lower
        = std::lower_bound(xicData.begin(), xicData.end(), point2d(time, 0.0), point2d_less_x);
    // this will not happen
    Q_ASSERT(lower != xicData.end());
    Q_ASSERT(lower->x() == time);

    int index = std::distance(xicData.begin(), lower);
    Q_ASSERT(index >= 0);
    Q_ASSERT(index < static_cast<int>(xicData.size()));

    Interval<int> peakInterval;
    for (const Interval<int> &interval : qAsConst(intervals)) {
        if (interval.contains(index)) {
            peakInterval = interval;
        }
    }

    if (peakInterval.isNull()) {
        DEBUG_WARNING_LIMIT(warningMs() << "Explained point" << mz << scanIndex
                                        << "does not have integration limit",
                            5);
        return result;
    }

    // use the values from interval to build hill
    double startTime = xicData.at(peakInterval.start()).x();
    double endTime = xicData.at(peakInterval.end()).x();

    int hillScanIndexStart = converter.timeToScanIndex(startTime);
    Q_ASSERT(hillScanIndexStart != -1);

    int hillScanIndexEnd = converter.timeToScanIndex(endTime);
    Q_ASSERT(hillScanIndexEnd != -1);

    MzScanIndexRect hillArea;
    hillArea.mz = window.mz;
    hillArea.scanIndex = ScanIndexInterval(hillScanIndexStart, hillScanIndexEnd);

    // collect all the centroided points for the hill now
    MzScanIndexRect fittedHillArea;
    QVector<NonUniformTilePoint> hillPoints;

    makeHill(selectionTileManager, hillArea, &fittedHillArea, &hillPoints);
    if (hillPoints.isEmpty()) {
        warningMs() << "Unexpected empty hill found:" << hillArea.toQRectF();
        result = NonUniformHill();
    } else {
        fittedHillArea.scanIndex.rend() += 1; // make it compatible with explainPeak definition
        result = NonUniformHill(fittedHillArea, hillPoints);
    }

    return result;
}

NonUniformHill
NonUniformTileHillFinder::explainNeighbor(NonUniformTileSelectionManager *selectionTileManager,
                                          double neighborMzValue, const MzScanIndexRect &parentHill)
{
    NonUniformHill result;

    if (d->searchArea.mz.start() > neighborMzValue || neighborMzValue > d->searchArea.mz.end()) {
        return result;
    }

    MzScanIndexRect neighborRect;
    QVector<NonUniformTilePoint> neighborPoints;

    MzScanIndexRect candidateRect = parentHill;
    candidateRect.mz.rstart() = qBound(d->searchArea.mz.start(), neighborMzValue - d->mzWidth * 0.5, d->searchArea.mz.end());
    candidateRect.mz.rend() = qBound(d->searchArea.mz.start(), neighborMzValue + d->mzWidth * 0.5, d->searchArea.mz.end());
    // parentHill is in range <scanIndexStart, end)
    // candidateRect is in <scanIndexStart, end>
    candidateRect.scanIndex.rend() -= 1;

    makeHill(selectionTileManager, candidateRect, &neighborRect, &neighborPoints);

    // we have found something
    if (neighborPoints.size() > 0) {
        neighborRect.scanIndex.rend() += 1; // make it compatible with explainPeak
        result = NonUniformHill(neighborRect, neighborPoints);
    } else {
        // null otherwise
    }
    return result;
}

void NonUniformTileHillFinder::setMzTolerance(double mzTolerance)
{
    d->mzWidth = mzTolerance;
}

double NonUniformTileHillFinder::mzTolerance() const
{
    return d->mzWidth;
}

void NonUniformTileHillFinder::makeHill(NonUniformTileSelectionManager *selectionTileManager,
                                        const MzScanIndexRect &area, MzScanIndexRect *hillRect,
                                        QVector<NonUniformTilePoint> *hillPoints)
{
    bool doCentroiding = d->device->doCentroiding();
    MzScanIndexNonUniformTileRectIterator iterator(d->device->tileManager(), d->device->range(),
                                                   doCentroiding, area);

    RandomNonUniformTileSelectionIterator selectionIterator(
        selectionTileManager, d->device->range(), d->device->doCentroiding());

    *hillRect = area;

    int scanIndexStart = 0;
    int scanIndexEnd = 0;

    double mzStart = -1.0;
    double mzEnd = -1.0;

    bool firstProcessed = false;

    while (iterator.hasNext()) {
        const point2dList &data = iterator.next();
        selectionIterator.moveTo(iterator.x(), iterator.y(), iterator.scanIndex());

        for (int i = 0; i < static_cast<int>(data.size()); ++i) {
            NonUniformTilePoint tilePoint;
            tilePoint.scanIndex = iterator.scanIndex();
            tilePoint.tilePos = QPoint(iterator.x(), iterator.y());
            tilePoint.internalIndex = iterator.internalIndex() + i;

            // if selected, we don't take that, so take only un-selected centroided points
            if (!selectionIterator.value().testBit(tilePoint.internalIndex)) {
                double candidateMz = data.at(i).x();

                if (!firstProcessed) {
                    mzStart = candidateMz;
                    mzEnd = candidateMz;

                    scanIndexStart = tilePoint.scanIndex;
                    scanIndexEnd = tilePoint.scanIndex;

                    firstProcessed = true;
                } else {
                    // mz fitting of the hill rect
                    mzStart = std::min(mzStart, candidateMz);
                    mzEnd = std::max(mzEnd, candidateMz);

                    scanIndexStart = std::min(tilePoint.scanIndex, scanIndexStart);
                    scanIndexEnd = std::max(tilePoint.scanIndex, scanIndexEnd);
                }

                hillPoints->push_back(tilePoint);
            }
        }
    }

    // if we find only one point, use the area mz range
    if (mzStart == mzEnd) {
        mzStart = area.mz.start();
        mzEnd = area.mz.end();
    }

    // fit the hill rect's scans and mz properly
    hillRect->mz.rstart() = mzStart;
    hillRect->mz.rend() = mzEnd;

    hillRect->scanIndex.rstart() = scanIndexStart;
    hillRect->scanIndex.rend() = scanIndexEnd;
}

NonUniformHill NonUniformTileHillFinder::makeDefaultHill(const NonUniformTilePoint &tilePoint)
{
    NonUniformHill result;
    if (!tilePoint.isValid()) {
        return result;
    }

    double mz = 0.0;
    {
        RandomNonUniformTileIterator it = d->device->createRandomIterator();
        it.moveTo(tilePoint.tilePos.x(), tilePoint.tilePos.y(), tilePoint.scanIndex);
        const point2dList &tileContent = it.value();
        if (tileContent.empty()) {
            return result;
        }

        mz = tileContent.at(static_cast<size_t>(tilePoint.internalIndex)).x();
    }

    int scanIndex = tilePoint.scanIndex;

    MzScanIndexRect candidateRect;

    candidateRect.scanIndex.rstart() = scanIndex;
    candidateRect.scanIndex.rend() = scanIndex;

    candidateRect.mz.rstart()
        = qBound(d->searchArea.mz.start(), mz - d->mzWidth * 0.5, d->searchArea.mz.end());
    candidateRect.mz.rend()
        = qBound(d->searchArea.mz.start(), mz + d->mzWidth * 0.5, d->searchArea.mz.end());

    MzScanIndexRect area;
    QVector<NonUniformTilePoint> points;

    makeHill(d->session->selectionTileManager(), candidateRect, &area, &points);

    return NonUniformHill(area, points);
}

void NonUniformTileHillFinder::markPointsAsProcessed(
    const QVector<NonUniformTilePoint> &hillPoints,
    NonUniformTileSelectionManager *selectionTileManager)
{
    RandomNonUniformTileSelectionIterator bitIterator(selectionTileManager, d->device->range(),
                                                      d->device->doCentroiding());

    bitIterator.setCacheSize(0);

    NonUniformTileSelectionStoreMemory *selectionStore
        = dynamic_cast<NonUniformTileSelectionStoreMemory *>(selectionTileManager->store());

    if (!selectionStore) {
        qFatal("For now only in-memory selection is implemented");
        return;
    }

    for (int i = 0; i < hillPoints.size(); ++i) {
        const NonUniformTilePoint &hillPoint = hillPoints.at(i);

        bitIterator.moveTo(hillPoint.tilePos.x(), hillPoint.tilePos.y(), hillPoint.scanIndex);
        QBitArray selection = bitIterator.value();
        // we could call QBitArray::toggleBit and check for proper selection in the end,
        // but that would not detect selecting particular point 3,5,7 times
        if (selection.testBit(hillPoint.internalIndex)) {
            qWarning() << "Trying to select point that is already selected, at tile pos:"
                       << hillPoint.tilePos << "scanIndex" << hillPoint.scanIndex
                       << "internal index" << hillPoint.internalIndex;

            if (selection.testBit(hillPoint.internalIndex)) {
                qFatal("Already marked!");
                return;
            }
        }

        selection.setBit(hillPoint.internalIndex, true);

        // write to store directly, we don't have Write iterators right now
        // so update the tile in the store now
        int tileOffset = d->device->range().tileOffset(hillPoint.scanIndex);
        selectionStore->tile(hillPoint.tilePos, d->device->content())
            .setValue(tileOffset, selection);
    }
}

void NonUniformTileHillFinder::resetId()
{
    d->hillId = 0;
}

int NonUniformTileHillFinder::nextId()
{
    d->hillId++;
    return d->hillId;
}

Err NonUniformTileHillFinder::getXICDataWithSelection(const MzScanIndexRect &window,
                                                      point2dList &points)
{
    Err e = kNoErr;

    ScanIndexInterval scanIndex = window.scanIndex;
    int rowsRemaining = scanIndex.end() - scanIndex.start() + 1; // 0-based indexing, thus +1
    if (rowsRemaining < 0) {
        rrr(kBadParameterError);
    }

    // partial sums
    const ScanIndexNumberConverter &m_converter = d->session->converter();
    points.resize(static_cast<size_t>(rowsRemaining));
    for (int i = 0; i < rowsRemaining; i++) {
        double time = m_converter.toScanTime(m_converter.toScanNumber(scanIndex.start() + i));
        points[i] = QPointF(time, 0.0);
    }

    bool doCentroiding = true;
    MzScanIndexNonUniformTileRectIterator iterator(d->device->tileManager(), d->device->range(),
                                                   doCentroiding, window);

    RandomNonUniformTileSelectionIterator bitIterator(
        d->session->selectionTileManager(), d->device->range(), d->device->doCentroiding());
    bitIterator.setCacheSize(0);

    NonUniformTilePoint pt;
    while (iterator.hasNext()) {
        // we don't have any additional mzs here
        point2dList scanPart = iterator.next();
        pt.tilePos = QPoint(iterator.x(), iterator.y());
        pt.scanIndex = iterator.scanIndex();
        int internalIndex = iterator.internalIndex();

        double sumPart = 0.0;
        for (const QPointF &pt : scanPart) {
            // include into integration only points that are not selected
            bitIterator.moveTo(iterator.x(), iterator.y(), iterator.scanIndex());
            bool processed = bitIterator.value().at(internalIndex);
            if (!processed) {
                sumPart += pt.y();
            }
            internalIndex++;
        }

        int pointsIndex = iterator.scanIndex() - scanIndex.start();
        Q_ASSERT(pointsIndex >= 0);

        double newSum = points.at(pointsIndex).y() + sumPart;
        points[pointsIndex].setY(newSum);
    }

    return e;
}

_PMI_END
