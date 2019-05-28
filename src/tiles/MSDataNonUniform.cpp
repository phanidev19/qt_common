/*
* Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

// common_ms
#include "MSDataNonUniform.h"
#include "pmi_common_ms_debug.h"

// common_tiles
#include "RandomNonUniformTileIterator.h"
#include "SequentialNonUniformTileIterator.h"
#include "TilePositionConverter.h"

// common_core_mini
#include "CsvWriter.h"
#include "PlotBase.h"

#include <QDir>
#include <QRect>

#include <iterator>
#include "MzScanIndexRect.h"
#include "MzScanIndexNonUniformTileRectIterator.h"
#include "Point2dListUtils.h"


_PMI_BEGIN

using namespace msreader;

MSDataNonUniform::MSDataNonUniform(NonUniformTileStore *store, const NonUniformTileRange &range,
                                   const ScanIndexNumberConverter &converter)
    : m_manager(new NonUniformTileManager(store))
    , m_ownedManager(true)
    , m_range(range)
    , m_converter(converter)
{
    debugMs() << "Converter contains" << converter.size() << "scan numbers";
}


MSDataNonUniform::MSDataNonUniform(NonUniformTileManager *tileManager, const NonUniformTileRange &range, const ScanIndexNumberConverter &converter):
    m_manager(tileManager),
    m_ownedManager(false),
    m_range(range),
    m_converter(converter)
{
    Q_ASSERT(tileManager);
}

MSDataNonUniform::~MSDataNonUniform()
{
    if (m_ownedManager) {
        delete m_manager;
    }
}

Err MSDataNonUniform::getScanDataPart(int scanIndex, double mzStart, double mzEnd, point2dList *points, bool doCentroiding /*= false*/) const
{
    int tileXStart = m_range.tileX(mzStart);
    int tileXEnd = m_range.tileX(mzEnd);
    int tileCountX = (tileXEnd - tileXStart) + 1;

    RandomNonUniformTileIterator iterator(m_manager, m_range, doCentroiding);
    m_manager->setCacheSize(std::max(tileCountX, m_manager->cacheSize()));

    points->clear();

    point2dList tilePoints; // contains extra points from tiles
    for (int tileX = tileXStart; tileX <= tileXEnd; tileX++) {
        double mz = m_range.mzAt(tileX);
        iterator.moveTo(scanIndex, mz);
        const point2dList &data = iterator.value();
        tilePoints.insert(tilePoints.end(), data.cbegin(), data.cend());
    }

    // filter only to mzStart, mzEnd including
    *points = Point2dListUtils::extractPointsIncluding(tilePoints, mzStart, mzEnd);

    return kNoErr;
}


bool MSDataNonUniform::exportTile(const QPoint &tilePos, NonUniformTileStore::ContentType type,
                                  const QDir &dir) const
{
    int tileX = tilePos.x();
    int tileY = tilePos.y();

    // x interval of the tile
    double mzStart = 0;
    double mzEnd = 0;
    m_range.mzTileInterval(tileX, &mzStart, &mzEnd);

    // y interval of the tile
    int scanIndexStart = 0;
    int scanIndexEnd = 0;
    m_range.scanIndexInterval(tileY, &scanIndexStart, &scanIndexEnd);
    scanIndexEnd = std::min(m_range.scanIndexMax() + 1, scanIndexEnd);

    NonUniformTileManager manager(m_manager->store());
    RandomNonUniformTileIterator iterator(&manager, m_range,
                                          type == NonUniformTileStore::ContentMS1Centroided);

    int mzPrecision = 6;
    QString fileName = QString("%1_%2_mz_%3_%4.csv")
                           .arg(tileX)
                           .arg(tileY)
                           .arg(QString::number(mzStart, 'f', mzPrecision),
                                QString::number(mzEnd, 'f', mzPrecision));

    CsvWriter writer(dir.filePath(fileName));
    bool ok = writer.open();
    if (!ok) {
        return ok;
    }

    for (int scanIndex = scanIndexStart; scanIndex < scanIndexEnd; ++scanIndex) {
        iterator.moveTo(tileX, tileY, scanIndex);
        point2dList tileScanPart = iterator.value();

        QStringList row = QStringList{ QString::number(scanIndex), QString(), QString() };
        for (const point2d &pt : tileScanPart) {
            row[1] = QString::number(pt.x(), 'f', 15);
            row[2] = QString::number(pt.y(), 'f', 15);
            (void)writer.writeRow(row);
        }
    }

    return true;
}

Err MSDataNonUniform::getScanData(long scanNumber, point2dList *points, bool doCentroiding) const
{
    int scanIndex = m_converter.toScanIndex(scanNumber);
    return getScanDataPart(scanIndex, m_range.mzMin(), m_range.mzMax(), points, doCentroiding);
}

Err MSDataNonUniform::getXICData(const XICWindow &win, QVector<int> *scanNumbers,
                                 QVector<point2dList> *points, bool doCentroiding) const
{
    int scanIndexStart = m_converter.timeToScanIndex(win.time_start);
    int scanIndexEnd = m_converter.timeToScanIndex(win.time_end);

    for (int scanIndex = scanIndexStart; scanIndex <= scanIndexEnd; ++scanIndex) {
        int scanNumber = m_converter.toScanNumber(scanIndex);

        point2dList data;
        Err e = getScanDataPart(scanIndex, win.mz_start, win.mz_end, &data, doCentroiding);
        points->push_back(data);
        scanNumbers->push_back(scanNumber);
    }

    return kNoErr;
}

Err MSDataNonUniform::getXICData(const XICWindow &win, point2dList *points)
{
    QVector<int> scanNumbers;
    QVector<point2dList> listEntry;
    Err e = getXICData(win, &scanNumbers, &listEntry);

    for (int i = 0; i < scanNumbers.size(); ++i) {
        int scanNumber = scanNumbers.at(i);
        const point2dList &entry = listEntry.at(i);

        double time = m_converter.toScanTime(scanNumber);
        //double sum = accumulate(entry.begin(), entry.end(), 0.0, sumY);

        double sum = 0.0;
        for (const QPointF& pt : entry) {
            sum += pt.y();
        }

        points->push_back(QPointF(time, sum));
    }

    return kNoErr;
}


Err MSDataNonUniform::getXICDataNG(const XICWindow &win, point2dList *points)
{
    // contains tile indexes 
    QRect xicTileRect = tileRect(win);
    int cacheSize = qMax(xicTileRect.width(), xicTileRect.height());
    bool doCentroiding = true;
    RandomNonUniformTileIterator iterator(m_manager, m_range, true);

    const int scanIndexStart = m_converter.timeToScanIndex(win.time_start);
    const int scanIndexEnd = m_converter.timeToScanIndex(win.time_end);
    int rowsRemaining = scanIndexEnd - scanIndexStart + 1; // 0-based indexing, thus +1

    // init the points 
    points->resize(rowsRemaining);
    for (int i = 0; i < rowsRemaining; i++) {
        double time = m_converter.toScanTime(m_converter.toScanNumber(scanIndexStart + i));
        (*points)[i] = QPointF(time, 0.0);
    }

    int scanIndexY = scanIndexStart;
    while (rowsRemaining > 0) {
        int numContiguousTileRows = iterator.numContiguousRows(scanIndexY);
        int rowsToWork = std::min(numContiguousTileRows, rowsRemaining);

        for (int tileXIndex = xicTileRect.x(); tileXIndex < xicTileRect.x() + xicTileRect.width(); ++tileXIndex) {
            
            double tileMzStart = m_range.mzAt(tileXIndex);
            double tileMzEnd = m_range.mzAt(tileXIndex + 1);

            double mzStart = qMax(win.mz_start, tileMzStart);
            double mzEnd = std::min(win.mz_end, tileMzEnd);

            for (int rows = 0; rows < rowsToWork; ++rows) {
                int currentScanIndex = scanIndexY + rows;
                iterator.moveTo(currentScanIndex, mzStart);

                // take the part between mzStart and mzEnd excluding
                point2dList scanPart = Point2dListUtils::extractPointsIncluding(iterator.value(), mzStart, mzEnd);

                double sumPart = 0.0;
                for (const QPointF& pt : scanPart) {
                    sumPart += pt.y();
                }

                int pointsIndex = currentScanIndex - scanIndexStart;

                double newSum = points->at(pointsIndex).y() + sumPart;
                (*points)[pointsIndex].setY(newSum);
            }
        }

        scanIndexY += rowsToWork;
        rowsRemaining -= rowsToWork;
    }

    return kNoErr;
}

Err MSDataNonUniform::getXICDataSequentialIterator(const QRect &tileArea, point2dList *points)
{
    // contains tile indexes 
    const int scanIndexStart = tileArea.top();
    const int scanIndexEnd = tileArea.bottom();
    int rowsRemaining = scanIndexEnd - scanIndexStart + 1; // 0-based indexing, thus +1

    // partial sums
    points->resize(rowsRemaining);
    for (int i = 0; i < rowsRemaining; i++) {
        double time = m_converter.toScanTime(m_converter.toScanNumber(scanIndexStart + i));
        (*points)[i] = QPointF(time, 0.0);
    }

    bool doCentroiding = true;
    SequentialNonUniformTileIterator iterator(m_manager, m_range, doCentroiding, tileArea);
    while (iterator.hasNext()) {
        point2dList scanPart = iterator.next();

        // take the part between mzStart and mzEnd excluding
        scanPart = Point2dListUtils::extractPointsIncluding(scanPart, iterator.mzStart(), iterator.mzEnd());

        double sumPart = 0.0;
        for (const QPointF& pt : scanPart) {
            sumPart += pt.y();
        }

        // map to index in the subsums
        int pointsIndex = iterator.scanIndex() - scanIndexStart;

        double newSum = points->at(pointsIndex).y() + sumPart;
        (*points)[pointsIndex].setY(newSum);
    }

    return kNoErr;

}

Err MSDataNonUniform::getXICDataMzScanIndexIterator(const XICWindow &win, point2dList *points)
{
    // contains tile indexes 
    QRect xicTileRect = tileRect(win);

    const int scanIndexStart = m_converter.timeToScanIndex(win.time_start);
    const int scanIndexEnd = m_converter.timeToScanIndex(win.time_end);
    int rowsRemaining = scanIndexEnd - scanIndexStart + 1; // 0-based indexing, thus +1

    // partial sums
    points->resize(rowsRemaining);
    for (int i = 0; i < rowsRemaining; i++) {
        double time = m_converter.toScanTime(m_converter.toScanNumber(scanIndexStart + i));
        (*points)[i] = QPointF(time, 0.0);
    }

    MzScanIndexRect area;
    area.mz = MzInterval(win.mz_start, win.mz_end);
    area.scanIndex = ScanIndexInterval(scanIndexStart, scanIndexEnd);
    
    bool doCentroiding = true;
    MzScanIndexNonUniformTileRectIterator iterator(m_manager, m_range, doCentroiding, area);
    while (iterator.hasNext()) {
        // we don't have any additional mzs here 
        point2dList scanPart = iterator.next();

        double sumPart = 0.0;
        for (const QPointF& pt : scanPart) {
            sumPart += pt.y();
        }

        int pointsIndex = iterator.scanIndex() - scanIndexStart;
        
        double newSum = points->at(pointsIndex).y() + sumPart;
        (*points)[pointsIndex].setY(newSum);
    }

    return kNoErr;
}

QRect MSDataNonUniform::tileRect(const XICWindow &win) const
{
    int scanIndexStart = m_converter.timeToScanIndex(win.time_start);
    int scanIndexEnd = m_converter.timeToScanIndex(win.time_end);

    return m_range.tileRect(win.mz_start, win.mz_end, scanIndexStart, scanIndexEnd);
}


Err MSDataNonUniform::writeUniformData(const XICWindow &win, const TileRange &range, bool doCentroiding, QVector<double> *buffer) const
{
    buffer->clear();
    
    TilePositionConverter converter(range);
    const int steps = range.size().width();
    std::vector<double> sortedXs;
    sortedXs.reserve(steps);
    const QPoint baseLevel(1, 1);
    for (int i = 0; i < steps; i++) {
        sortedXs.push_back(converter.levelToWorldPositionX(i, baseLevel));
    }

    // slow; TODO: per tile 
    QVector<int> scanNumbers;
    QVector<point2dList> scansData;
    Err e = getXICData(win, &scanNumbers, &scansData, doCentroiding); ree;

    if (range.size().height() != scanNumbers.size()) {
        warningMs() << "Mis-match between range and NonUniform content!";
        return kBadParameterError;
    }

    //TODO: try resize 
    buffer->reserve(static_cast<int>(scanNumbers.size() * sortedXs.size()));

    for (int i = 0; i < scansData.size(); ++i) {
        PlotBase pb(scansData.at(i));
        std::vector<double> intensities;
        pb.evaluate_linear(sortedXs, &intensities);
        
        std::copy(intensities.begin(), intensities.end(), std::back_inserter(*buffer));
    }

    return e;
}

_PMI_END


