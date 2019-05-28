/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "NonUniformFeatureFindingSession.h"

#include "MSDataNonUniform.h"
#include "MSDataNonUniformAdapter.h"
#include "MzScanIndexRect.h"
#include "NonUniformTileBuilder.h"
#include "NonUniformTileDevice.h"
#include "NonUniformTileIntensityIndex.h"
#include "NonUniformTileManager.h"
#include "NonUniformTileMaxIntensityFinder.h"
#include "NonUniformTileStore.h"
#include "NonUniformTileStoreMemory.h"
#include "NonUniformTileStoreSqlite.h"
#include "pmi_common_ms_debug.h"

#include <QRect>

_PMI_BEGIN

class Q_DECL_HIDDEN NonUniformFeatureFindingSession::Private
{
public:
    Private(MSDataNonUniformAdapter *document)
        : doc(document)
        , device(document->nonUniformData()->tileManager(), document->range(),
                 NonUniformTileStore::ContentMS1Centroided)
        , selectionTileManager(&selectionStore)
        , finder(&device)
    {
        Q_ASSERT(document);
    }

    MSDataNonUniformAdapter *doc = nullptr;

    MzScanIndexRect searchArea;
    QRect tileRect;

    NonUniformTileDevice device;

    // which points are marked as processed
    NonUniformTileSelectionStoreMemory selectionStore;
    NonUniformTileSelectionManager selectionTileManager;

    // optional hill index
    NonUniformTileHillIndexStore *hillIndexStore = nullptr;
    QScopedPointer<NonUniformTileHillIndexManager> hillIndexManager;

    NonUniformTileIntensityIndex tilesIntensityIndex;

    NonUniformTileMaxIntensityFinder finder;
};

/***/

NonUniformFeatureFindingSession::NonUniformFeatureFindingSession(MSDataNonUniformAdapter *doc)
    : d(new Private(doc))
{
    const NonUniformTileRange &range = d->device.range();

    d->device.tileManager()->setCacheSize(range.tileCountX() * range.tileCountY());
    QRect tileRect = range.tileRect(range.areaRect());
    setSearchArea(tileRect);

    // The cache has to be 0 when tile manager is used for read / write case.
    // Cache size 0 means that tile will be refetched from store everytime it is accessed.
    // We write directly to store for now as we don't have write iterators. We read from cache
    // if tile is already deserialized to cache, so what can happen is that  you write directly to
    // store but when you read and tile is cached, the tile from cache does not have the values from
    // store
    d->selectionTileManager.setCacheSize(0);
}

NonUniformFeatureFindingSession::~NonUniformFeatureFindingSession()
{
}

bool NonUniformFeatureFindingSession::begin()
{
    if (!initializeSelection()) {
        return false;
    }

    if (!initializeIndex()) {
        return false;
    }

    if (d->hillIndexStore) {
        // optional part
        if (!initializeHillIndex()) {
            return false;
        }

        d->hillIndexManager.reset(new NonUniformTileHillIndexManager(d->hillIndexStore));
    }

    return true;
}

void NonUniformFeatureFindingSession::end()
{
}

bool NonUniformFeatureFindingSession::initializeSelection()
{
    NonUniformTileBuilder builder(d->device.range());
    bool ok = builder.buildNonUniformTileSelection(d->doc->store(), d->device.content(),
                                                   &d->selectionStore, d->tileRect);
    return ok;
}
bool NonUniformFeatureFindingSession::initializeHillIndex()
{
    Q_ASSERT(d->hillIndexStore);

    NonUniformTileBuilder builder(d->device.range());
    bool ok = builder.buildNonUniformTileHillIndex(d->doc->store(), d->device.content(),
                                                   d->hillIndexStore, d->tileRect);
    return ok;
}

bool NonUniformFeatureFindingSession::initializeIndex()
{
    d->finder.createIndexForTiles(d->tileRect, &d->tilesIntensityIndex);
    return true;
}

void NonUniformFeatureFindingSession::setSearchArea(const MzScanIndexRect &searchArea)
{
    d->searchArea = searchArea;
    // FIXME: implies that we support specific selection initialization: select points that are not
    // desired to be used.
    // Do it naively and align to the tile rect for now
    QRect tileRect = d->device.range().tileRect(searchArea);
    setSearchArea(tileRect);
}

void NonUniformFeatureFindingSession::setSearchArea(const QRect &tileRect)
{
    d->tileRect = tileRect;
    // update search area based on the rect
    MzScanIndexRect rc = d->device.range().fromTileRect(tileRect);
    // intersect with domain range
    d->searchArea = d->device.range().areaRect().intersected(rc);
}

MzScanIndexRect NonUniformFeatureFindingSession::searchArea() const
{
    return d->searchArea;
}

QRect NonUniformFeatureFindingSession::searchAreaTile() const
{
    return d->tileRect;
}

void NonUniformFeatureFindingSession::maxIntensity(point2d *maxMzIntensity,
                                                   NonUniformTilePoint *point)
{
    double top = d->tilesIntensityIndex.topIntensity();
    NonUniformTilePoint pt;

    if (top == -1.0) {
        // we don't have max intensity anymore
        warningMs() << "Cannot find unselected maxima!";
        *maxMzIntensity = point2d();
        *point = pt;
        return;
    }

    double mz = -1.0;
    QList<IntensityIndexEntry *> topEntries = d->tilesIntensityIndex.topIntensityEntries();

    // reduce the list to one entry
    // find the one with maximum mz
    RandomNonUniformTileIterator tileIterator = d->device.createRandomIterator();

    if (topEntries.size() > 1) {
        QVector<qreal> mzs(topEntries.size());
        // let's find the one with biggest mz
        int index = 0;
        for (IntensityIndexEntry *biggestMzEntry : topEntries) {

            int tileX = biggestMzEntry->pos.tilePos.x();
            int tileY = biggestMzEntry->pos.tilePos.y();
            int scanIndex = biggestMzEntry->pos.scanIndex;
            int internalIndex = biggestMzEntry->pos.internalIndex;

            tileIterator.moveTo(tileX, tileY, scanIndex);
            mzs[index] = tileIterator.value().at(static_cast<size_t>(internalIndex)).x();
            index++;
        }

        int maxIndex = static_cast<int>(std::distance(mzs.begin(), std::max_element(mzs.begin(), mzs.end())));

        pt = topEntries[maxIndex]->pos;
        mz = mzs[maxIndex];

    } else if (topEntries.size() == 1) {

        pt = topEntries.first()->pos;
        tileIterator.moveTo(pt.tilePos.x(), pt.tilePos.y(), pt.scanIndex);
        mz = tileIterator.value().at(static_cast<size_t>(pt.internalIndex)).x();

    } else {
        qFatal("indexer provided 0 results, this is not acceptable; fix indexer!");
    }

    Q_ASSERT(mz > 0.0);
    Q_ASSERT(pt.scanIndex >= 0);

    *maxMzIntensity = point2d(mz, top);
    *point = pt;
}

void NonUniformFeatureFindingSession::setHillIndexStore(
    NonUniformTileHillIndexStore *hillIndexStore)
{
    d->hillIndexStore = hillIndexStore;
}

const ScanIndexNumberConverter &NonUniformFeatureFindingSession::converter() const
{
    return d->doc->nonUniformData()->converter();
}

int NonUniformFeatureFindingSession::totalPointCount() const
{
    NonUniformTileStoreSqlite *store = d->doc->store();
    //TODO: change API to quint32
    return store->pointCount(d->device.content(), d->tileRect);
}

void pmi::NonUniformFeatureFindingSession::updateIndexForTiles(const QList<QPoint> &tilePositions)
{
    d->finder.updateIndexForTiles(&d->selectionTileManager, tilePositions, &d->tilesIntensityIndex);
}

NonUniformTileHillIndexManager *NonUniformFeatureFindingSession::hillIndexManager() const
{
    return d->hillIndexManager.data();
}

NonUniformTileSelectionManager *NonUniformFeatureFindingSession::selectionTileManager() const
{
    return &d->selectionTileManager;
}

NonUniformTileManager *NonUniformFeatureFindingSession::tileManager() const
{
    return d->device.tileManager();
}

NonUniformTileDevice *NonUniformFeatureFindingSession::device() const
{
    return &d->device;
}

MSDataNonUniformAdapter *NonUniformFeatureFindingSession::document() const
{
    return d->doc;
}

void NonUniformFeatureFindingSession::searchAreaSelectionStats(int *selected, int *deselected)
{
    if (d->selectionStore.tileCount(d->device.content()) == 0) {
        warningMs() << "Selection is not initialized!";
        return;
    }

    NonUniformTileSelectionManager selectionTileManager(&d->selectionStore);
    RandomNonUniformTileSelectionIterator bitIterator(&selectionTileManager, d->device.range(),
                                                      d->device.doCentroiding());

    *deselected = 0;
    *selected = 0;
    for (int tileY = d->tileRect.top(); tileY <= d->tileRect.bottom(); ++tileY) {
        for (int tileX = d->tileRect.left(); tileX <= d->tileRect.right(); ++tileX) {

            int firstScanIndex = d->device.range().scanIndexAt(tileY);
            int lastScanIndex
                = std::min(d->device.range().lastScanIndexAt(tileY), d->device.range().scanIndexMax());
            for (int scanIndex = firstScanIndex; scanIndex <= lastScanIndex; ++scanIndex) {
                bitIterator.moveTo(tileX, tileY, scanIndex);
                const QBitArray values = bitIterator.value();
                for (int i = 0; i < values.size(); ++i) {
                    if (values.testBit(i)) {
                        (*selected)++;
                    } else {
                        (*deselected)++;
                    }
                }
            }
        }
    }
}

_PMI_END
