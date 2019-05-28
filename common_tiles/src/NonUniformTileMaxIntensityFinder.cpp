/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "NonUniformTileMaxIntensityFinder.h"
#include "NonUniformTileDevice.h"
#include "NonUniformTileIntensityIndex.h"
#include "RandomNonUniformTileIterator.h"

#include "MzScanIndexRect.h"

#include "NonUniformTilePoint.h"

#include <QRect>

#include <QFuture>
#include <QThread>
#include <QtConcurrent/QtConcurrentRun>

_PMI_BEGIN

const double INVALID_INTENSITY = -1.0; // anything negative is invalid

class Q_DECL_HIDDEN NonUniformTileMaxIntensityFinder::Private
{
public:
    Private(NonUniformTileDevice *dev)
        : device(dev)
    {
    }

    int threadCount = 0;
    NonUniformTileDevice *device = nullptr;
};

NonUniformTileMaxIntensityFinder::NonUniformTileMaxIntensityFinder(NonUniformTileDevice *device)
    : d(new Private(device))
{
}

NonUniformTileMaxIntensityFinder::~NonUniformTileMaxIntensityFinder()
{
}

void NonUniformTileMaxIntensityFinder::createIndexForAll(NonUniformTileIntensityIndex *index)
{
    // tile indexes
    QRect tileRect = d->device->range().tileRect(d->device->range().areaRect());
    createIndexForTiles(tileRect, index);
}

void NonUniformTileMaxIntensityFinder::createIndexForTiles(const QRect &tileRect,
                                                           NonUniformTileIntensityIndex *index)
{
    if (d->threadCount <= 0) {
        d->threadCount = QThread::idealThreadCount();
    }

    d->threadCount = std::max(d->threadCount, 1); // we need at least one thread

    debugTiles() << "Thread count" << d->threadCount;

    // every other thread needs distinct store due to limitation in sqlite
    // even read access requires separate connection
    QVector<NonUniformTileManager *> tileManagers;
    // main thread will not use any clone but provided store passed by parameter
    int distincStoreCount = d->threadCount - 1;
    for (int i = 0; i < distincStoreCount; ++i) {
        NonUniformTileManager *cloned = d->device->tileManager()->clone();
        if (!cloned) {
            warningTiles() << "Failed to clone store!";
            return;
        }
        tileManagers.push_back(cloned);
    }

    QVector<QVector<IntensityIndexEntry *>> threadEntries;
    threadEntries.resize(d->threadCount);

    int rowsOfTilesPerThread = tileRect.height() / d->threadCount;

    QList<QFuture<void>> futures;
    for (int i = 0; i < d->threadCount; i++) {

        QRect threadTileRect(tileRect.x(), tileRect.y() + (i * rowsOfTilesPerThread), tileRect.width(), rowsOfTilesPerThread);
        // last one is this thread
        if (i == (d->threadCount - 1)) {
            threadTileRect.setHeight(tileRect.height() - i * rowsOfTilesPerThread);
            indexTileRect(d->device->tileManager(), d->device->content(), threadTileRect, &threadEntries[i]);
        } else {
            futures += QtConcurrent::run(this, &NonUniformTileMaxIntensityFinder::indexTileRect,
                                         tileManagers.at(i), d->device->content(), threadTileRect, &threadEntries[i]);
        }
    }

    for (int i = 0; i < futures.size(); ++i) {
        futures[i].waitForFinished();
    }

    qDeleteAll(tileManagers);

    index->clear();
    // to the index with pointers
    for (const QVector<IntensityIndexEntry *> &entries : threadEntries) {
        for (IntensityIndexEntry *indexEntry : entries) {
            // takes ownership
            index->insertIndexEntry(indexEntry);
        }
    }
}

void NonUniformTileMaxIntensityFinder::updateIndexForTiles(
    NonUniformTileSelectionManager *selectionTileManager, const QList<QPoint> &tilePositions,
    NonUniformTileIntensityIndex *index)
{
    QVector<IntensityIndexEntry> results;
    results.resize(tilePositions.size());
    for (int i = 0; i < tilePositions.size(); ++i) {
        indexTileWithSelection(d->device->tileManager(), selectionTileManager, tilePositions.at(i), &results[i]);
    }

    // update index
    for (const IntensityIndexEntry &item : results) {
        index->updateIntensity(item);
    }
}

void NonUniformTileMaxIntensityFinder::indexTileRect(NonUniformTileManager *tileManager,
                                                     NonUniformTileStoreType::ContentType type,
                                                     const QRect &tileRect,
                                                     QVector<IntensityIndexEntry *> *entries)
{
    NonUniformTileRange range = d->device->range();
    RandomNonUniformTileIterator iterator(tileManager, range,
                                          type == NonUniformTileStoreType::ContentMS1Centroided);

    // for every tile in the tileRect
    NonUniformTilePoint maximumTilePosition;
    for (int y = tileRect.top(); y <= tileRect.bottom(); ++y) {

        int scanIndexStart;
        int scanIndexEnd;
        range.scanIndexInterval(y, &scanIndexStart,
                                &scanIndexEnd); // [scanIndexStart, scanIndexEnd)
        // scanIndexMax() is still available valid scan, so we need to find interval scanIndexEnd) thus +1
        scanIndexEnd = std::min(range.scanIndexMax() + 1, scanIndexEnd);
        maximumTilePosition.tilePos.setY(y);

        for (int x = tileRect.left(); x <= tileRect.right(); ++x) {
            point2d currentMaximum = point2d(0.0, 0.0);
            maximumTilePosition.tilePos.setX(x);
            bool maxInit = false;
            // for every scan part in the tile

            for (int scanIndex = scanIndexStart; scanIndex < scanIndexEnd; ++scanIndex) {
                iterator.moveTo(x, y, scanIndex);
                point2dList data = iterator.value();

                // for every point in scan part
                for (size_t i = 0; i < data.size(); ++i) {
                    point2d currentIntensity = data.at(i);
                    if (currentIntensity.y() >= currentMaximum.y()) {
                        if (currentIntensity.y() == currentMaximum.y()) {
                            // prefer higher mz
                            if (currentIntensity.x() < currentMaximum.x()) {
                                continue;
                            }
                        }

                        // we found temporary new max
                        // now check if the point is marked
                        maximumTilePosition.scanIndex = scanIndex;
                        maximumTilePosition.internalIndex = static_cast<int>(i);
                        currentMaximum = currentIntensity;
                        maxInit = true;
                    }
                }
            }

            IntensityIndexEntry *indexEntry = new IntensityIndexEntry();
            indexEntry->intensity = currentMaximum.y();
            indexEntry->pos = maximumTilePosition;
            entries->push_back(indexEntry);
        }
    }
}

void NonUniformTileMaxIntensityFinder::indexTileWithSelection(
    NonUniformTileManager *tileManager, NonUniformTileSelectionManager *selectionTileManager, const QPoint &tilePos,
    IntensityIndexEntry *indexEntry)
{
    NonUniformTileRange range = d->device->range();
    bool doCentroiding = d->device->doCentroiding();

    RandomNonUniformTileSelectionIterator selectionIterator(selectionTileManager, range, doCentroiding);

    RandomNonUniformTileIterator iterator(tileManager, range, doCentroiding);

    int scanIndexStart;
    int scanIndexEnd;
    range.scanIndexInterval(tilePos.y(), &scanIndexStart,
                            &scanIndexEnd); // [scanIndexStart, scanIndexEnd)
    // scanIndexMax() is still available valid scan, so we need to find interval scanIndexEnd) thus +1
    scanIndexEnd = std::min(range.scanIndexMax() + 1, scanIndexEnd);

    NonUniformTilePoint pt;
    pt.tilePos = tilePos;
    point2d currentMaximum = point2d(0.0, 0.0);
    bool maxInit = false;

    for (int scanIndex = scanIndexStart; scanIndex < scanIndexEnd; ++scanIndex) {
        // fetch selection from the store
        selectionIterator.moveTo(tilePos.x(), tilePos.y(), scanIndex);
        const QBitArray &selection = selectionIterator.value();

        // note: this already fetches the tile's scan index
        // so we could probably optimize
        iterator.moveTo(tilePos.x(), tilePos.y(), scanIndex);
        const point2dList &data = iterator.value();

        for (int i = 0; i < selection.size(); ++i) {
            bool markedAsUsed = selection.testBit(i);
            if (markedAsUsed) {
                continue;
            }

            // we have enabled point
            const point2d &currentIntensity = data.at(static_cast<size_t>(i));
            if (isMaxima(currentIntensity, currentMaximum)) {
                // we found temporary new max, that is not selected as processed
                pt.tilePos = QPoint(tilePos.x(), tilePos.y());
                pt.scanIndex = scanIndex;
                pt.internalIndex = i;
                currentMaximum = currentIntensity;
                maxInit = true;
            }
        }
    }

    if (!maxInit) {
        warningTiles() << "Tile" << tilePos << "does not have maxima";
    }

    indexEntry->pos = pt;
    indexEntry->intensity = currentMaximum.isNull() ? INVALID_INTENSITY : currentMaximum.y();
}

void NonUniformTileMaxIntensityFinder::setThreadCount(int threadCount)
{
    d->threadCount = threadCount;
}

bool NonUniformTileMaxIntensityFinder::isMaxima(const point2d &currentIntensity,
                                                const point2d &currentMaximum)
{
    if (currentIntensity.y() >= currentMaximum.y()) {
        if (currentIntensity.y() == currentMaximum.y()) {
            // prefer higher mz
            return (currentIntensity.x() >= currentMaximum.x());
        }

        return true;
    }

    return false;
}

_PMI_END
