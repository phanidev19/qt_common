/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef TILE_FEATURE_FINDER_H
#define TILE_FEATURE_FINDER_H

#include "pmi_common_ms_export.h"
#include "pmi_core_defs.h"
#include "MSEquispacedData.h"

#include "IntensityPriorityQueue.h"

#include <QRect>
#include <QScopedPointer>
#include <QVector>

_PMI_BEGIN

class TileFeatureFinderPrivate;
class Tile;
class RandomTileIterator;
class IntensityPeak;

class PMI_COMMON_MS_EXPORT TileFeatureFinder
{

public:
    TileFeatureFinder(const QString &filePath);

    ~TileFeatureFinder();

    enum SearchDirection { SearchDirectionLeft, SearchDirectionRight };

    void setTopKIntensities(int count);

    int topKIntensities() const;

    void setLevelOfInterest(int level);

    //! this version searches per tile
    int findLocalMaxima();

    //! this version search per tile position (takes neighbor tile into account)
    int findLocalMaximaNG();

    //! Creates a sorted array of peaks from priority queue, for further processing
    void initializePeakArray();

    //! Adjusts the start to end range along the x and y axes of tiles.
    void updateTimeMzRanges();

    //! Determine which peaks are associated locally based on 1.003/charge separation
    void associatePeaks();

    void allData(MSEquispacedData *rd);

    void findMinMaxIntensity(double *minIntensity, double *maxIntensity, bool fastIterator = true);

    //! This function checks every maxima found by findLocalMaxima functions and filters them out
    // The maxima is kept if maxima is also found in area distant from found maxima by mzDistance in
    // radius of mzRadius
    // @param mzDistance
    // @param mzWidth - mz width defines the width of search rectangle
    // @param timeWidth - time width defines the height of search rectangle
    // @param intensityTolerance - in percents, e.g. 0.1 = 10%, means that at least one local maxima
    // with 90% of the currently inspected local maxima's intensity has to be found in search rectangle in order to allow the
    // maxima to stay in
    // @param direction - if you want to search +mzDistance or -mzDistance for currently inspected local maxima point 
    int filterLocalMaxima(double mzDistance, double mzWidth, double timeWidth,
                          double intensityTolerance, SearchDirection direction);

    void saveToCsv(const QString &csvFilePath);
    void saveToCsv(const QString &csvFilePath, bool showOriginal, bool showAll);

private:
    QVector<IntensityPoint> localMaximaForTile(const Tile &t, const int radiusX, const int radiusY);

    //! Returns INVALID_INTENSITY if the point is not local maxima
    //
    // @param domainTileArea is needed for checking if the tilePos neighbor are valid tile coordinates
    // @param tilePos - coordinate in global tile positions
    // @param iterator - re-used iterator for navigating in the global tile space
    double localMaximaForTilePos(const QRect &domainTileArea, const QPoint &tilePos, RandomTileIterator * const iterator);

    QRect wholeDomainTileIndexArea() const;

    QRect wholeDomainTileArea() const;

    //! Searches individual tiles for local maxima, tile border values are excluded from search
    // @see TileFeatureFinder::tileArea
    IntensityPriorityQueue searchTileIndexArea(const QRect &tileIndexArea);

    //! Searches the tiles tile-point-by-tile-point for local maxima respecting neighbor tiles
    IntensityPriorityQueue searchTileArea(const QRect &tileArea);

    void readTileArea(const QRect &tileArea, MSEquispacedData *rd);

    // tile index area is defined in tile rows and tile columns
    // QRect::topLeft is first row, first column
    // QRect::width is last row, last column
    QRect toTileIndexArea(double mzStart, double mzEnd, int scanIndexStart, int scanIndexEnd) const;

    // tile area, in global tile coordinates
    QRect toTileArea(double mzStart, double mzEnd, int scanIndexStart, int scanIndexEnd) const;

    void findMinMaxIntensityInArea(const QRectF &area, int id, QVector<double> *minIntensity, QVector<double> *maxIntensity);

    void findMinMaxIntensityInAreaFast(const QRectF &area, int id, QVector<double> *minIntensity, QVector<double> *maxIntensity);

    // Find the x and y extent of a peak
    void adjustGlobalXRangeWithMinima(IntensityPeak &ip);
    void adjustGlobalYRangeWithMinima(IntensityPeak &ip);

    int searchMzUp(int startIndex, int timeIndex, int maxPoints, double compareIntensity,
        RandomTileIterator * const iterator) const;
    int searchMzDown(int startIndex, int timeIndex, double compareIntensity,
        RandomTileIterator * const iterator) const;

    int searchTimeUp(int startIndex, int mzIndex, int maxPoints, double compareIntensity,
        double peakIntensity, RandomTileIterator * const iterator);
    int searchTimeDown(int startIndex, int mzIndex, double compareIntensity,
        double peakIntensity, RandomTileIterator * const iterator);

private:
    QScopedPointer<TileFeatureFinderPrivate> d;
    
    const double FRACTION_OF_PEAK_MZ = .1;
    const double FRACTION_OF_PEAK_TIME = .01;
    const double MIN_PEAK_SIZE = 10000;

    // 1.003355, Do we want the added accuracy?
    const double C12C13_SPACING = 1.003;

    const double MAX_PEAK_TIME_DIFF = 0.05;
    const double MIN_PEAK_INTENSITY_FRACTION = 0.0025;
    const double MAX_PEAK_MZ_DIFF_ABOVE = 100.0;
    const double MAX_PEAK_MZ_DIFF_BELOW = 50.0;

    // Pixels widths incorporated into maximum difference between 
    // expected and actual peak separation
    const double PIXEL_SIZE_MULTIPLIER = 2.1;
    const double SPACING_RESOLUTION_DIVISOR = 150.0;

    const double LAST_PEAK_MULTIPLIER = 1.5;
    const double UPPER_REAL_MINIMA_FRACTION_SEARCH_DOWN = 0.4;
    const double UPPER_REAL_MINIMA_FRACTION_SEARCH_UP = 0.5;

    // To make sure we have the right peak.
    const double MAX_PEAK_VAL_DISCREPANCY = .001;
};

_PMI_END

#endif // TILE_FEATURE_FINDER_H
