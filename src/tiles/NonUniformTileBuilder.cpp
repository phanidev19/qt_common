/*
* Copyright (C) 2016-2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "NonUniformTileBuilder.h"
#include "NonUniformTileStore.h"
#include "MSDataIterator.h"
#include "MSReader.h"
#include "NonUniformTileStoreMemory.h"

#include "ScanDataTiledIterator.h"

#include "ProgressBarInterface.h"
#include "ProgressContext.h"
#include "SequentialNonUniformTileIterator.h"

#include "MzScanIndexRect.h"

#include "pmi_common_ms_debug.h"

_PMI_BEGIN

NonUniformTileBuilder::NonUniformTileBuilder(const NonUniformTileRange &range) :m_range(range)
{
}

void NonUniformTileBuilder::buildNonUniformTiles(MSReader * reader, const ScanIndexNumberConverter &converter, NonUniformTileStore * store, NonUniformTileStore::ContentType type)
{
    int tileCountX = m_range.tileCountX();
    int tileCountY = m_range.tileCountY();

    MSDataIterator iterator(reader, type == NonUniformTileStore::ContentMS1Centroided, converter);

    store->start();
    for (int y = 0; y < tileCountY; y++) {
        for (int x = 0; x < tileCountX; x++) {
            double mzStart; 
            double mzEnd; 
            m_range.mzTileInterval(x, &mzStart, &mzEnd);

            int scanIndexStart;
            int scanIndexEnd;
            m_range.scanIndexInterval(y, &scanIndexStart, &scanIndexEnd);
            
            QVector<point2dList> tileData;
            iterator.fetchTileData(mzStart, mzEnd, scanIndexStart, scanIndexEnd, &tileData);

            NonUniformTile tile;
            tile.setPosition(QPoint(x, y));
            tile.setData(tileData);
            if (!store->saveTile(tile, type)) {
                qWarning() << "Failed to save NonUniformTile at" << tile.position();
            }
        }
    }
    store->end();
}

void NonUniformTileBuilder::buildNonUniformTilesNG(MSReader *reader,
                                                   const ScanIndexNumberConverter &converter,
                                                   NonUniformTileStore *store,
                                                   NonUniformTileStore::ContentType type,
                                                   QSharedPointer<ProgressBarInterface> progress)
{
    static const quint32 TWO_DOUBLES_IN_BYTES = sizeof(double) * 2;
    static const quint32 EMPTY_TILE_PART_SCAN_SIZE = sizeof(point2dList);
    static const qulonglong CACHE_SIZE_MB = 256; // max 1GB  // TODO: set by AdvancedSettings
    static const qulonglong CACHE_SIZE_BYTES = CACHE_SIZE_MB * 1024 * 1024;

    const int tileCountX = m_range.tileCountX();
    
    NonUniformTileStoreMemory tileStore; // memory cache

    quint32 flushCount = 0;

    qulonglong pointSizeInMemory = 0;
    qulonglong emptyScansSizeInMemory = 0;


    if (progress) {
        QString typeStr = (type == NonUniformTileStore::ContentMS1Centroided) ? "centroided" : "profile";
        progress->setText(QString("Creating %1 cache...").arg(typeStr));
    }

    {
        const int scanCount = m_range.scanIndexMax() - m_range.scanIndexMin() + 1;
        ProgressContext progressContext(scanCount, progress);

        for (int scanIndex = m_range.scanIndexMin(); scanIndex <= m_range.scanIndexMax(); ++scanIndex, ++progressContext) {
            const int scanNumber = converter.toScanNumber(scanIndex);

            point2dList scanData;
            Err e = reader->getScanData(scanNumber, &scanData, type == NonUniformTileStore::ContentMS1Centroided);
            if (e != kNoErr) {
                QString msg = QString("Error reading scans for scan number %1").arg(scanNumber);
                qWarning() << msg;
                break;
            }

            const int tileY = m_range.tileY(scanIndex);

            int tileXIndex = 0;
            ScanDataTiledIterator it(&scanData, m_range, tileXIndex, tileCountX);
            while (it.hasNext()) {
                point2dList tilePart = it.next();
                size_t partLength = tilePart.size();

                // do we need to make space in our cache?
                pointSizeInMemory += partLength * TWO_DOUBLES_IN_BYTES;
                emptyScansSizeInMemory += EMPTY_TILE_PART_SCAN_SIZE;
                qulonglong memoryOccupied = pointSizeInMemory + emptyScansSizeInMemory;
                // if size of the cache is not enough, time to flush tiles to disk storage (db or file)
                if (memoryOccupied > CACHE_SIZE_BYTES) {
                    // flush to secondary storage (disk or db)
                    flushCount++;
                    flush(&tileStore, store, type, flushCount);
                    // cache is empty now
                    pointSizeInMemory = 0;
                    emptyScansSizeInMemory = 0;
                }

                // save tile part to tile memory store
                QPoint tilePos(tileXIndex, tileY);
                tileStore.append(tilePos, type, tilePart);
                tileXIndex++;
            }
        }
    }

    flushCount++;
    flush(&tileStore, store, type, flushCount);
    pointSizeInMemory = 0;
    emptyScansSizeInMemory = 0;

    store->defragmentTiles(store);

//#define DEBUG_TILE_PART_TABLES
#ifndef DEBUG_TILE_PART_TABLES
    if (!store->dropTilePartCache()) {
        qWarning() << "Cannot drop tile parts cache!";
    }
#endif

}

bool NonUniformTileBuilder::buildNonUniformTileSelection(
    NonUniformTileStore *store, NonUniformTileStore::ContentType type,
    NonUniformTileSelectionStore *selectionStore, const QRect &tileArea)
{
    selectionStore->clear();

    // we want to build selection for all points
    bool doCentroiding = (type == NonUniformTileStoreType::ContentMS1Centroided);

    // sequential iterator goes tile-by-tile-scanpart-by-scanpart sequentially so we will use this
    // behavioral attribute, we can change this when we have write iterators available
    NonUniformTileManager tileManager(store);
    SequentialNonUniformTileIterator tileIterator(&tileManager, m_range, doCentroiding, tileArea);

    // use invalid tile position
    QPoint processedTilePos = NonUniformTilePoint::INVALID_TILE_POSITION;
    NonUniformSelectionTile tile;

    int selectionTileHeight = m_range.scanIndexTileLength();
    QVector<QBitArray> defaultTileData;
    defaultTileData.resize(selectionTileHeight);

    while (tileIterator.hasNext()) {
        point2dList scanPart = tileIterator.next();
        int tileX = tileIterator.x();
        int tileY = tileIterator.y();

        QPoint currentTilePos = QPoint(tileX, tileY);
        // if we are in new tile, allocate space for it in selection store
        if (processedTilePos != currentTilePos) {
            tile.setPosition(currentTilePos);
            tile.setData(defaultTileData);
            processedTilePos = currentTilePos;
        }

        // nothing is selected by default
        QBitArray scanPartSelection = QBitArray(static_cast<int>(scanPart.size()), false);

        int tileOffset = m_range.tileOffset(tileIterator.scanIndex());
        tile.setValue(tileOffset, scanPartSelection);

        // if this was last tileOffset, save the tile to store
        if (tileIterator.isLastVisitedScanIndexInTile()) {
            bool ok = selectionStore->saveTile(tile, type);
            if (!ok) {
                warningMs() << "Failed to save tile!";
                return false;
            }
        }
    }

    return true;
}

bool NonUniformTileBuilder::buildNonUniformTileHillIndex(NonUniformTileStore *store,
                                                         NonUniformTileStore::ContentType type,
                                                         NonUniformTileHillIndexStore *hillStore,
                                                         const QRect &tileArea)
{
    // TODO: merge with buildNonUniformTileSelection using templates: 
    // types differs only and some initialization routines
    hillStore->clear();

    bool doCentroiding = (type == NonUniformTileStoreType::ContentMS1Centroided);

    // sequential iterator goes tile-by-tile-scanpart-by-scanpart sequentially so we will use this
    // behavioral attribute, we can change this when we have write iterators available
    NonUniformTileManager tileManager(store);
    SequentialNonUniformTileIterator tileIterator(&tileManager, m_range, doCentroiding, tileArea);

    // use invalid tile position
    QPoint processedTilePos = NonUniformTilePoint::INVALID_TILE_POSITION;
    NonUniformHillIndexTile tile;

    int selectionTileHeight = m_range.scanIndexTileLength();
    QVector<QVector<int>> defaultTileData;
    defaultTileData.resize(selectionTileHeight);

    while (tileIterator.hasNext()) {
        point2dList scanPart = tileIterator.next();
        int tileX = tileIterator.x();
        int tileY = tileIterator.y();

        QPoint currentTilePos = QPoint(tileX, tileY);
        // if we are in new tile, allocate space for it in selection store
        if (processedTilePos != currentTilePos) {
            tile.setPosition(currentTilePos);
            tile.setData(defaultTileData);
            processedTilePos = currentTilePos;
        }

        // zero is invalid, default feature id
        QVector<int> scanPartSelection = QVector<int>(static_cast<int>(scanPart.size()), 0);

        int tileOffset = m_range.tileOffset(tileIterator.scanIndex());
        tile.setValue(tileOffset, scanPartSelection);

        // if this was last tileOffset, save the tile to store
        if (tileIterator.isLastVisitedScanIndexInTile()) {
            bool ok = hillStore->saveTile(tile, type);
            if (!ok) {
                warningMs() << "Failed to save tile!";
                return false;
            }
        }
    }

    return true;
}

bool NonUniformTileBuilder::flush(NonUniformTileStoreMemory *src, NonUniformTileStore *dst,
                                  NonUniformTileStore::ContentType type, int flushCount)
{
    // time to initialize
    if (flushCount == 1) {
        dst->initTilePartCache();
    }
    
    return src->flush(dst, type, flushCount);
}

_PMI_END


