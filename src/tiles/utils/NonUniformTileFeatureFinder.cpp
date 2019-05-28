/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "NonUniformTileFeatureFinder.h"

#include "pmi_common_ms_debug.h"

#include "MSDataNonUniformAdapter.h"
#include "MSDataNonUniform.h"
#include "MSDataIterator.h"

#include <NonUniformTileStoreSqlite.h>
#include <RandomNonUniformTileIterator.h>
#include <MzScanIndexNonUniformTileRectIterator.h>

#include <CsvWriter.h>
#include <PlotBase.h>
#include <Point2dListUtils.h>

#include <QRectF>
#include "MzScanIndexRect.h"

_PMI_BEGIN

class NonUniformTileFeatureFinderPrivate
{
public:
    NonUniformTileFeatureFinderPrivate(MSDataNonUniformAdapter *doc)
        : document(doc){};

    ~NonUniformTileFeatureFinderPrivate() {}

    MSDataNonUniformAdapter *document;
    NonUniformIntensityPriorityQueue queue;
    int topk;
};

NonUniformTileFeatureFinder::NonUniformTileFeatureFinder(MSDataNonUniformAdapter *doc)
    : d(new NonUniformTileFeatureFinderPrivate(doc))
{
}

NonUniformTileFeatureFinder::~NonUniformTileFeatureFinder()
{
}

void NonUniformTileFeatureFinder::setTopKIntensities(int count)
{
    d->topk = count;
}

int NonUniformTileFeatureFinder::findLocalMaxima()
{
    NonUniformIntensityPriorityQueue queue = searchTileArea(d->document->range().area());

    d->queue = queue;

    return d->queue.size();
}

void NonUniformTileFeatureFinder::saveToCsv(const QString &csvFilePath)
{
    CsvWriter writer(csvFilePath, "\r\n", ',');
    if (!writer.open()) {
        warningMs() << "Cannot open destination file" << csvFilePath;
        return;
    }

    writer.writeRow({
        "mzStart", "mzEnd", "timeStart", "timeEnd", "label", // requested columns
        "mz", "intensity", "scanIndex",
    }); // debugging columns

    NonUniformIntensityPriorityQueue queue = d->queue;

    const ScanIndexNumberConverter &converter = d->document->nonUniformData()->converter();

    double MZ_WIDTH = 0.05;

    while (!queue.empty()) {
        const NonUniformIntensityPoint pt = queue.top();

        double mz = pt.mzIntensity.x();
        double intensity = pt.mzIntensity.y();

        // current and next
        int scanNumber = converter.toScanNumber(pt.scanIndex);
        double scanTime = converter.toScanTime(scanNumber);

        int scanNumberNext = converter.toScanNumber(pt.scanIndex + 1);
        double scanTimeEnd = converter.toScanTime(scanNumberNext);

        double mzStart = mz - (MZ_WIDTH * 0.5);
        double mzEnd = mz + (MZ_WIDTH * 0.5);

        const QString mzStartStr = QString::number(mzStart, 'g', 12);
        const QString mzEndStr = QString::number(mzEnd, 'g', 12);
        const QString timeStartStr = QString::number(scanTime, 'g', 12);
        const QString timeEndStr = QString::number(scanTimeEnd, 'g', 12);

        const QString mzStr = QString::number(pt.mzIntensity.x(), 'g', 12);
        const QString intensityStr = QString::number(pt.mzIntensity.y(), 'g', 12);
        const QString scanIndexStr = QString::number(pt.scanIndex);

        const QString label = intensityStr; // for now the label is intensity

        writer.writeRow({ mzStartStr, mzEndStr, timeStartStr, timeEndStr, label, mzStr,
                          intensityStr, scanIndexStr });

        queue.pop();
    }
}

NonUniformIntensityPriorityQueue NonUniformTileFeatureFinder::searchTileArea(const QRectF &area)
{
    NonUniformIntensityPriorityQueue queue;

    if (!d->document->hasData(NonUniformTileStore::ContentMS1Raw)) {
        warningMs() << "Document does not have centroided data.";
        return queue;
    }

    const NonUniformTileRange &range = d->document->range();

    MzScanIndexRect mzScanIndexArea = MzScanIndexRect::fromQRectF(area);

    NonUniformTileManager manager(d->document->store());
    MzScanIndexNonUniformTileRectIterator iterator(&manager, range, false, mzScanIndexArea);
    RandomNonUniformTileIterator neighborIterator(&manager, d->document->range(), false);
    
    double areaMzStart = area.left();
    double areaMzEnd = area.right();

    int firstTileX = range.tileX(areaMzStart);
    int lastTileX = range.tileX(areaMzEnd);

    int tileYprev = 0;
    while (iterator.hasNext()) {
        
        point2dList currentTile = iterator.next();
        // raw scan part, might contain more than area.left, area.right

        if (tileYprev != iterator.y()) {
            qDebug() << iterator.y();
            tileYprev = iterator.y();
        }

        if (currentTile.empty()) {
            continue;
        }
        
        point2dList scanPart;
        // PHASE 1: add scan from previous tile 
        if (iterator.x() == firstTileX) {
            // there is no prev tile for the first tile 
        } else {
            int prevTileX = iterator.x() - 1;

            // special case: we iterate within single tile
            Q_ASSERT(prevTileX >= firstTileX); 

            double tileMzStart = range.mzAt(prevTileX);
            double tileMzEnd = range.mzAt(prevTileX + 1);

            double mzStart = qMax(areaMzStart, tileMzStart);
            double mzEnd = qMin(areaMzEnd, tileMzEnd);

            neighborIterator.moveTo(iterator.scanIndex(), tileMzStart);

            if (prevTileX == firstTileX) {
                scanPart = Point2dListUtils::extractPoints(neighborIterator.value(), mzStart, mzEnd);
            } else {
                scanPart = neighborIterator.value();
            }
            
        }

        // PHASE 2: add current tile scan data
        int currentTileX = iterator.x();
        double tileMzStart = range.mzAt(currentTileX);
        double tileMzEnd = range.mzAt(currentTileX + 1);

        double currentTileMzStart = qMax(areaMzStart, tileMzStart);
        double currentTileMzEnd = qMin(areaMzEnd, tileMzEnd);

        //if (currentTileX == firstTileX || currentTileX == lastTileX) {
            currentTile = Point2dListUtils::extractPoints(currentTile, currentTileMzStart, currentTileMzEnd);
        //} 
        
        scanPart.insert(scanPart.end(), currentTile.begin(), currentTile.end());

        // PHASE 3: add scan from next tile data
        if (iterator.x() == lastTileX) {
            // there is no next tile for the last tile 
        } else {
            int nextTileX = iterator.x() + 1;
            
            Q_ASSERT(nextTileX <= lastTileX);

            double tileMzStart = range.mzAt(nextTileX);
            double tileMzEnd = range.mzAt(nextTileX + 1);

            double mzStart = qMax(areaMzStart, tileMzStart);
            double mzEnd = qMin(areaMzEnd, tileMzEnd);

            neighborIterator.moveTo(iterator.scanIndex(), tileMzStart);
            point2dList nextScanPart = neighborIterator.value();

            // search area might point to part of the tile
            if (nextTileX == lastTileX) {
                nextScanPart = Point2dListUtils::extractPoints(nextScanPart, mzStart, mzEnd);
            }

            scanPart.insert(scanPart.end(), nextScanPart.begin(), nextScanPart.end());
        }

        // note pb check if it is sorted, not needed
        PlotBase plot(scanPart);

        point2dList centroided;
        plot.makeCentroidedPoints(&centroided, PlotBase::CentroidNaiveMaxValue);

        if (!centroided.empty()) {
            // limit the results to current scan mz range to avoid duplicates
            centroided = Point2dListUtils::extractPoints(centroided, currentTileMzStart, currentTileMzEnd);

            for (const point2d &pt : centroided) {
                NonUniformIntensityPoint item;
                item.mzIntensity = pt;
                item.scanIndex = iterator.scanIndex();

                NonUniformIntensityPriorityQueueUtils::pushWithLimit(&queue, item, d->topk);
            }
        }
    }

    return queue;
}

_PMI_END
