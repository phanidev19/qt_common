/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "TileFeatureFinder.h"
#include "CsvWriter.h"
#include "TileManager.h"
#include "TilePositionConverter.h"
#include "TileRange.h"
#include "TileStoreSqlite.h"

#include "pmi_common_ms_debug.h"

#include "ScanIndexTimeConverter.h"
#include "ScanInfoDao.h"

#include "RandomTileIterator.h"
#include "SequentialTileIterator.h"

#include <algorithm>
#include <vector>
#include <QFuture>
#include <QThread>
#include <QtConcurrent/QtConcurrentRun>
#include <QtMath>

const int DEFAULT_TOP_K_INTENSITIES = 1000;
const int DEFAULT_LEVEL = 1;
const double INVALID_INTENSITY = -1024.0;


_PMI_BEGIN

ScanIndexTimeConverter loadConverter(QSqlDatabase *db){
    ScanInfoDao dao;
    Err e = dao.load(db);
    //Q_ASSERT(e == kNoErr);

    std::vector<ScanInfoRow> data = dao.data();

    std::vector<double> indices;
    std::vector<double> retTimes;
    for (const ScanInfoRow &row : data) {
        indices.push_back(row.scanIndex);
        retTimes.push_back(row.retTimeMinutes);
    }

    return ScanIndexTimeConverter(retTimes, indices);
}

class IntensityPeak
{
public:
    IntensityPeak() : m_peakIntensity(0), m_peakMz(0), m_peakTime(0),
        m_tileX(0), m_tileY(0), m_tileStartX(0), m_tileEndX(0), m_tileStartY(0),
        m_tileEndY(0), m_matchDiff(0), m_label(""), m_capturedInOtherPeak(false) {}
public:
    double m_peakIntensity;
    double m_peakMz;
    double m_peakTime;
    int m_tileX;
    int m_tileY;
    int m_tileStartX;
    int m_tileEndX;
    int m_tileStartY;
    int m_tileEndY;
    double m_matchDiff;
    QString m_label;
    bool m_capturedInOtherPeak;
};

class TileFeatureFinderPrivate
{
public:
    TileFeatureFinderPrivate(const QString &filePath)
        : topk(DEFAULT_TOP_K_INTENSITIES)
        , store(filePath)
    {
        TileRange::loadRange(&store.db(), &range);
        setLevel(DEFAULT_LEVEL);
    }

    ~TileFeatureFinderPrivate() {}

    void setLevel(int levelOfInterest)
    {
        level = QPoint(levelOfInterest, levelOfInterest); // mip-map level
    }

public:
    int topk;
    QPoint level;

    TileStoreSqlite store;
    TileRange range;

    IntensityPriorityQueue queue;

    bool applyLevelOffset = true;

    std::vector<IntensityPeak> intensityPeaks;
    std::vector<double> peaksToIgnore;
};

TileFeatureFinder::TileFeatureFinder(const QString &filePath)
    : d(new TileFeatureFinderPrivate(filePath))
{
}

TileFeatureFinder::~TileFeatureFinder()
{
}

void TileFeatureFinder::setTopKIntensities(int count)
{
    d->topk = count;
}

int TileFeatureFinder::topKIntensities() const
{
    return d->topk;
}

void TileFeatureFinder::setLevelOfInterest(int level)
{
    d->setLevel(level);
}

int TileFeatureFinder::findLocalMaxima()
{
    const QRect tileIndexArea = wholeDomainTileIndexArea();
    IntensityPriorityQueue queue = searchTileIndexArea(tileIndexArea);

    d->queue = queue;
    return queue.size();
}

int TileFeatureFinder::findLocalMaximaNG()
{
    const QRect tileArea = wholeDomainTileArea();
    IntensityPriorityQueue queue = searchTileArea(tileArea);
    
    d->queue = queue;
    return d->queue.size();
}

void TileFeatureFinder::allData(MSEquispacedData *rd)
{
    qDebug() << "Getting data & filling in matrix of equispaced data";
    QRect tileArea = wholeDomainTileArea();
    readTileArea(tileArea, rd);
}

void TileFeatureFinder::findMinMaxIntensity(double *minIntensity, double *maxIntensity, bool fastIterator)
{
    int threadCount = QThread::idealThreadCount();

    // results from threads are stored here
    QVector<double> minIntensities(threadCount);
    QVector<double> maxIntensities(threadCount);
    
    // every thread will be assigned similar amount of tiles
    QRect tileIndexArea = wholeDomainTileIndexArea();
    const int rowsOfTilesPerThread = qCeil(tileIndexArea.height() / qreal(threadCount));

    const int rowsOfScanIndexes = d->range.size().height();
    int rowsOfScanIndexesPerThread = rowsOfTilesPerThread * Tile::HEIGHT;
    
    TilePositionConverter convert(d->range);
    QRectF wholeArea = d->range.area();
    QList<QFuture<void>> futures;

    auto functionPtr = fastIterator ? &TileFeatureFinder::findMinMaxIntensityInAreaFast
                                    : &TileFeatureFinder::findMinMaxIntensityInArea;

    for (int i = 0; i < threadCount; i++) {
    
        int scanIndexesToProcess = rowsOfScanIndexes - i * rowsOfTilesPerThread * Tile::HEIGHT;
        scanIndexesToProcess = qMin(scanIndexesToProcess, rowsOfScanIndexesPerThread);

        // tile-index to tile position
        int tileY = (i * rowsOfTilesPerThread) * Tile::HEIGHT;
        
        double scanIndexStart = convert.levelToWorldPositionY(tileY, QPoint(1, 1));
        double scanIndexEnd = scanIndexStart + scanIndexesToProcess;

        QRectF area;
        area.setLeft(wholeArea.left());
        area.setRight(wholeArea.right());

        area.setTop(scanIndexStart);
        area.setBottom(scanIndexEnd);

        int id = i;
        
        futures += QtConcurrent::run(
            this, functionPtr,
            area, id, &minIntensities, &maxIntensities);
    }

    for (int i = 0; i < threadCount; ++i) {
        futures[i].waitForFinished();
    }

    auto minIter = std::min_element(minIntensities.begin(), minIntensities.end());
    *minIntensity = *minIter;

    auto maxIter = std::max_element(maxIntensities.begin(), maxIntensities.end());
    *maxIntensity = *maxIter;
}

void TileFeatureFinder::initializePeakArray()
{
    qDebug() << "Initializing Peak Array...";

    ScanIndexTimeConverter scanIndexTimeConvert = loadConverter(&d->store.db());
    TilePositionConverter converter(d->range);
    IntensityPriorityQueue queue = d->queue;

    // Resized to hold all the peaks that are currently in queue
    d->intensityPeaks.resize(queue.size());
    double mzWidth = d->range.levelStepX(d->level);
    
    std::vector<IntensityPeak>::reverse_iterator rpi = d->intensityPeaks.rbegin();

    while (!queue.empty()) {
        const IntensityPoint point = queue.top();

        rpi->m_peakIntensity = point.intensity;
        rpi->m_tileX = point.globalTilePos.x();
        rpi->m_tileY = point.globalTilePos.y();

        // Make sure the start and end tile positions are set to a good initial value.
        rpi->m_tileStartX = rpi->m_tileX;
        rpi->m_tileEndX = rpi->m_tileX;
        rpi->m_tileStartY = rpi->m_tileY;
        rpi->m_tileEndY = rpi->m_tileY;

        double mz;
        double scanIndex;
        if (d->applyLevelOffset) {
            mz = converter.levelToWorldPositionX(point.globalTilePos.x(), d->level);
            scanIndex = converter.levelToWorldPositionY(point.globalTilePos.y(), d->level);
        } else {
            mz = converter.mzAt(point.globalTilePos.x());
            scanIndex = converter.scanIndexAt(point.globalTilePos.y());
        }

        double mzStart = mz - (mzWidth * 0.5);
        double scanIndexNext = scanIndex + d->range.levelStepY(d->level);
        double scanWidth = std::abs(scanIndex - scanIndexNext);
        double peakScanIndex = scanIndex - .5 * scanWidth;

        rpi->m_peakMz = mzStart;
        rpi->m_peakTime = scanIndexTimeConvert.scanIndexToTime(peakScanIndex);

        queue.pop();
        ++rpi;
    }
}

void TileFeatureFinder::updateTimeMzRanges()
{
    qDebug() << "Updating Time and Mz Ranges...";

    ScanIndexTimeConverter scanIndexTimeConvert = loadConverter(&d->store.db());
    double mzWidth = d->range.levelStepX(d->level);
    int counter = 0;

    std::vector<IntensityPeak>::iterator ip = d->intensityPeaks.begin();
    for (; ip != d->intensityPeaks.end(); ++ip) {

        std::vector<double>::iterator ifd =
            std::find(d->peaksToIgnore.begin(), d->peaksToIgnore.end(), ip->m_peakIntensity);

        if (ifd == d->peaksToIgnore.end()) {
            // Just pass in the IntensityPeak
            adjustGlobalXRangeWithMinima(*ip);
            adjustGlobalYRangeWithMinima(*ip);
        } else {
            ip->m_capturedInOtherPeak = true;
        }
        ++counter;
    }
}

int TileFeatureFinder::searchMzUp(int startIndex, int timeIndexIn, int maxPoints,
    double compareIntensity, RandomTileIterator *const iterator) const
{
    int index = startIndex;
    const int timeIndex = timeIndexIn;
    double intensityCurrent;
    double intensityNext;

    if (index >= maxPoints - 2) {
        // No better point, if we're already next tp the edge of image
        return maxPoints - 1;
    }

    ++index;

    iterator->moveTo(d->level, index, timeIndex);
    intensityCurrent = iterator->value();
    
    while (index <= maxPoints - 1) {

        if (index == maxPoints - 1) {
            // We are at the edge.
            return index;
        }
        if (intensityCurrent < compareIntensity) {
            // Seems like this should be needed for symmetry (see below), but apparently not...
            //--index;
            return index;
        }
        iterator->moveTo(d->level, index+1, timeIndex);
        intensityNext = iterator->value();
        // See if we are at a local minimum.
        if (intensityNext > intensityCurrent) {
            // If reach a minima, then shift back toward the center by one.
            --index;
            return index;
        }
        intensityCurrent = intensityNext;
        ++index;
    }
    return startIndex;
}

int TileFeatureFinder::searchMzDown(int startIndex, int timeIndexIn, double compareIntensity,
    RandomTileIterator *const iterator) const
{
    int index = startIndex;
    const int timeIndex = timeIndexIn;
    double intensityCurrent;
    double intensityNext;

    if (index <= 1) {
        // No better point if already at the edge of image.
        return 0;
    }
    --index;

    iterator->moveTo(d->level, index, timeIndex);
    intensityCurrent = iterator->value();

    while (index >= 0) {
        if (index == 0) {
            // We are at the edge.
            return index;
        }
        if (intensityCurrent < compareIntensity){
            ++index;
            return index;
        }
        iterator->moveTo(d->level, index, timeIndex);
        intensityNext = iterator->value();
        // See if we are at a local minimum.
        if (intensityNext > intensityCurrent) {
            // If reach a minima, then shift back toward the center by one.
            ++index;
            return index;
        }
        intensityCurrent = intensityNext;
        --index;
    }
    return startIndex;
}

void TileFeatureFinder::adjustGlobalXRangeWithMinima(IntensityPeak &ip)
{
    TilePositionConverter converter(d->range);
    const QRect domainTileArea = wholeDomainTileArea();
    TileManager manager(&d->store);
    RandomTileIterator iterator(&manager);

    int tileInfoX = TileManager::normalizeX(ip.m_tileStartX);
    int tileInfoY = TileManager::normalizeY(ip.m_tileStartY);

    iterator.setMainTile(TileInfo(d->level, QPoint(tileInfoX, tileInfoY)));
    iterator.moveTo(d->level, ip.m_tileStartX, ip.m_tileStartY);
    
    const double myIntensity = iterator.value();
    double mzWidth = d->range.levelStepX(d->level);

    if (std::abs(myIntensity - ip.m_peakIntensity) <= MAX_PEAK_VAL_DISCREPANCY) {

        const double compareToPeak = FRACTION_OF_PEAK_MZ * ip.m_peakIntensity;

        // Search up
        ip.m_tileEndX = searchMzUp(ip.m_tileX, ip.m_tileY, domainTileArea.width(), 
            compareToPeak, &iterator);

        // Search Down
        ip.m_tileStartX = searchMzDown(ip.m_tileX, ip.m_tileY, compareToPeak, &iterator);
    } else {
        qDebug() << "(*) Peak intensity " << myIntensity
                 << "does not match: in adjustMzRangeWithMinima()";
    }
}

int TileFeatureFinder::searchTimeUp(int startIndex, int mzIndexIn, int maxPoints,
    double compareIntensity, double peakIntensity, RandomTileIterator * const iterator)
{
    int index = startIndex;
    const int mzIndex = mzIndexIn;
    double intensityCurrent;
    double intensityNext;
    bool foundMinima = false;
    double lastPeak = peakIntensity;
    int lastEdgeIndex = 0;

    if (index >= maxPoints - 2) {
        // No better point, if we're right on the edge of image.
        return maxPoints - 1;
    }

    ++index;

    iterator->moveTo(d->level, mzIndex, index);
    intensityCurrent = iterator->value();
    
    while (index <= maxPoints - 1) {
        if (index == maxPoints - 1) {
            // We are at the edge.
            return index;
        }
        if (intensityCurrent < compareIntensity){
            --index;
            return index;
        }

        iterator->moveTo(d->level, mzIndex, index);
        intensityNext = iterator->value();

        // See if we are at a local minimum OR maxima.
        if (!foundMinima) {
            if (intensityNext > intensityCurrent) {
                if (std::abs(intensityCurrent / lastPeak) < UPPER_REAL_MINIMA_FRACTION_SEARCH_UP) {
                    // If reach a minima, then shift back toward the center by one.
                    --index;
                    return index;
                }
                // Otherwise keep serching.
                foundMinima = true;
                lastEdgeIndex = index - 1; // Analogout to --index
            }
        } else {
            if (intensityCurrent > lastPeak * LAST_PEAK_MULTIPLIER) {
                return lastEdgeIndex;
            }
            // Look for local maxima and add them to ignore list.
            if (intensityNext < intensityCurrent) { // at a local maxima
                foundMinima = false;
                lastPeak = std::min(lastPeak, intensityCurrent);
                d->peaksToIgnore.push_back(intensityCurrent);
            }
        }

        intensityCurrent = intensityNext;
        ++index;
    }
    return startIndex;
}

int TileFeatureFinder::searchTimeDown(int startIndex, int mzIndex, double compareIntensity,
    double peakIntensity, RandomTileIterator *const iterator)
{
    int index = startIndex;
    double intensityCurrent;
    double intensityNext;
    bool foundMinima = false;
    double lastPeak = peakIntensity;
    int lastEdgeIndex = 0;

    if (index <= 1) {
        // No better point if right on the edge of image.
        return 0;
    }

    --index;
    
    iterator->moveTo(d->level, mzIndex, index);
    intensityCurrent = iterator->value();
    
    while (index >= 0) {
        if (index == 0) {
            // We are at the edge.
            return index;
        }
        if (intensityCurrent < compareIntensity){
            ++index;
            return index;
        }
        iterator->moveTo(d->level, mzIndex, index);
        intensityNext = iterator->value();
        // See if we are at a local minimum OR Maxima.
        if (!foundMinima) {
            if (intensityNext > intensityCurrent) { // at a local minima
                if (std::abs(intensityCurrent / lastPeak) < UPPER_REAL_MINIMA_FRACTION_SEARCH_DOWN) {
                    // If reach a minima, then shift back toward the center by one.
                    return index;
                }
                // Otherwise we need to keep looking.
                foundMinima = true;
                lastEdgeIndex = index + 1; // Analogout to ++index
            }
        } else {
            // If we hit an intensity value greater than a certain size use last minima.
            if (intensityCurrent > lastPeak * LAST_PEAK_MULTIPLIER) {
                return lastEdgeIndex;
            }
            // Look for local maxima and add them to ignore list.
            if (intensityNext < intensityCurrent) { // at a local maxima
                foundMinima = false; // found maxima!
                lastPeak = std::min(lastPeak, intensityCurrent);
                d->peaksToIgnore.push_back(intensityCurrent);
            }
        }

        intensityCurrent = intensityNext;
        --index;
    }
    return startIndex;
}

void TileFeatureFinder::adjustGlobalYRangeWithMinima(IntensityPeak &ip)
{
    ScanIndexTimeConverter scanIndexTimeConvert = loadConverter(&d->store.db());

    TilePositionConverter converter(d->range);
    const QRect domainTileArea = wholeDomainTileArea();
    TileManager manager(&d->store);
    RandomTileIterator iterator(&manager);

    int tileInfoX = TileManager::normalizeX(ip.m_tileX);
    int tileInfoY = TileManager::normalizeY(ip.m_tileY);
    iterator.setMainTile(TileInfo(d->level, QPoint(tileInfoX, tileInfoY)));
    iterator.moveTo(d->level, ip.m_tileX, ip.m_tileY);

    const double myIntensity = iterator.value();

    if (std::abs(myIntensity - ip.m_peakIntensity) <= MAX_PEAK_VAL_DISCREPANCY) {

        const double compareToPeak = FRACTION_OF_PEAK_TIME * ip.m_peakIntensity;
        double scanIndexTemp = 0;

        // Search up
        ip.m_tileEndY = searchTimeUp(ip.m_tileY, ip.m_tileX, domainTileArea.height(),
            compareToPeak, ip.m_peakIntensity, &iterator);

        // Search down
        ip.m_tileStartY = searchTimeDown(ip.m_tileY, ip.m_tileX, compareToPeak,
            ip.m_peakIntensity, &iterator);
    } else {
        qDebug() << "(*) Peak intensity " << myIntensity 
            << "does not match: in adjustMzRangeWithMinima()";
    }
}

QString nextLabel(unsigned int count) {

    static const QStringList labels{ "A", "B", "C", "D", "E", "F", "G", "H", "I", "J",
        "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z"};

    unsigned int labelSize = labels.size();

    if (count < (labelSize * labelSize + labelSize)) {
        if (count < labelSize) {
            return QString(labels[count]);
        } else {
            int div = (count / labelSize) - 1;
            int remainder = count % labelSize;
            return QString(labels[div] + labels[remainder]);
        }
    } else {
        return QLatin1String("zz");
    }
}

class IsLessThanMinPeak
{
public:
    IsLessThanMinPeak(double minPeakSize) : minPeakSize_(minPeakSize) {}
    bool operator()(IntensityPeak &ip) {
        return (ip.m_peakIntensity < minPeakSize_);
    }
    double minPeakSize_;
};

void TileFeatureFinder::associatePeaks()
{
    qDebug() << "Getting associations...";
    // md => mass difference between C13 and C12.
    const double md = C12C13_SPACING;
    // Check for isotope separations, up to charge equal 10 (==> 0.1*md)
    const QVector<double> ISOTOPE_SEPARATIONS { md, .5*md, md / 3.0, .25*md, .2*md, 
        md / 6.0, md / 7.0, .125*md, md / 9.0, 0.1*md };
    std::vector<int> spacingCount;
    std::vector<unsigned int> possibleMatchesAbove;
    std::vector<unsigned int> possibleMatchesBelow;
    double startingIntensity;
    int count = 0;
    double mzDiffSigned = 0.0;
    double mzDiff = 0.0;
    QString nxLabel = nextLabel(count);
    double mzSpacing = d->range.levelStepX(d->level);

    // If the pixels are big, make the spacing difference bigger to match.
    const double maxMzSpacingDiff = std::max((PIXEL_SIZE_MULTIPLIER * mzSpacing),
        C12C13_SPACING / SPACING_RESOLUTION_DIVISOR);

    // Find the index of the first peak with an intensity less than MIN_PEAK_SIZE
    IsLessThanMinPeak iltmp(MIN_PEAK_SIZE);
    std::vector<IntensityPeak>::iterator ipFinal =
        std::find_if(d->intensityPeaks.begin(), d->intensityPeaks.end(), iltmp);

    const unsigned int isotopeSepSize = ISOTOPE_SEPARATIONS.size();
    spacingCount.resize(isotopeSepSize);

    std::vector<IntensityPeak>::iterator ip = d->intensityPeaks.begin();
    // For each peak - greater than MIN_PEAK_SIZE.
    for (; ip != ipFinal; ++ip) {

        // See if already identified by OR captured in another peak.
        if (ip->m_label.size() > 0 || ip->m_capturedInOtherPeak) {
            continue;
        }

        // Note: intensity peaks are sorted in descending order.
        startingIntensity = ip->m_peakIntensity;
        double invStartingIntensity = 1.0 / startingIntensity;

        // look at all peaks after this one, i.e. at lower intensities.
        std::vector<IntensityPeak>::iterator iq = ip + 1;

        possibleMatchesAbove.clear();
        possibleMatchesBelow.clear();
        // Zero out the spacing count array.
        for (int &ii : spacingCount) {
            ii = 0;
        }

        for (; iq != d->intensityPeaks.end(); ++iq) {

            // Already identified this one - as associated with another peak, or as one to ignore.
            // TODO: Might we want to to relax this, based on how reliable labeling is.
            if (iq->m_label.size() > 0 || iq->m_capturedInOtherPeak) {
                continue;
            }

            double timeDiff = std::abs(ip->m_peakTime - iq->m_peakTime);

            if (timeDiff < MAX_PEAK_TIME_DIFF) {

                double intensity = iq->m_peakIntensity;

                if ((intensity * invStartingIntensity) > MIN_PEAK_INTENSITY_FRACTION) {

                    mzDiffSigned = iq->m_peakMz - ip->m_peakMz;
                    mzDiff = std::abs(mzDiffSigned);

                    // See if this is in the range of time or mz to be a possible 
                    // match for the isotope sequence.
                    if (mzDiffSigned > 0.0 && mzDiff < MAX_PEAK_MZ_DIFF_ABOVE) {
                        possibleMatchesAbove.push_back(
                            std::distance(d->intensityPeaks.begin(), iq));
                    } else if (mzDiffSigned < 0.0 && mzDiff < MAX_PEAK_MZ_DIFF_BELOW) {
                        possibleMatchesBelow.push_back(
                            std::distance(d->intensityPeaks.begin(), iq));
                    } else {
                        continue;
                    }

                    // Before scanning for all the charge spacing, make sure it's in,
                    // range first, so we are only spending time when needed.
                    if (mzDiff <= (C12C13_SPACING + maxMzSpacingDiff)) {
                        for (int i = 0; i < ISOTOPE_SEPARATIONS.size(); ++i) {
                            if (std::abs(mzDiff - ISOTOPE_SEPARATIONS[i]) <= maxMzSpacingDiff) {
                                spacingCount[i] += 1;
                                break;
                            }
                        }
                    }
                }
            }
        }

        // The plan:
        // a) Find minimum spacing in spacingCount.
        // b) search through possibleMatches with best/min spacing
        // b1) search down, then up.
        std::vector<int>::reverse_iterator irv = spacingCount.rbegin();
        double minSpacing = 1e8;
        int minSpacingCount = 0;
        // TODO: May also want to check for a spacing with a higher frequency count and see if 
        //       that's a 'better' match.
        // TODO: Consider better way to index/organize this to make sub-search faster.
        for (; irv < spacingCount.rend(); ++irv) {
            if (*irv > 0) {
                int index = isotopeSepSize - 1 - std::distance(spacingCount.rbegin(), irv);
                if (index >= 0) {
                    minSpacing = ISOTOPE_SEPARATIONS [index];
                    minSpacingCount = *irv;
                    break;
                }
            }
        }

        nxLabel = nextLabel(count);
        // Of course we better label the main peak in the group.
        ip->m_label = nxLabel;

        if (minSpacingCount > 0) {
            int matchCount = 0;
            
            // Search up
            bool foundOne = true;
            IntensityPeak *startPeak = &(*ip);
            IntensityPeak *izp = nullptr;
            while (foundOne == true) {
                foundOne = false;
                for (unsigned int pm : possibleMatchesAbove) {
                    izp = &(d->intensityPeaks[pm]);
                    mzDiff = izp->m_peakMz - startPeak->m_peakMz;
                    if (std::abs(minSpacing - mzDiff) < maxMzSpacingDiff) {
                        izp->m_label = nxLabel;
                        izp->m_matchDiff = minSpacing;
                        foundOne = true;
                        ++matchCount;
                        break;
                    }
                }
                startPeak = izp;
            }

            // Search down
            startPeak = &(*ip);
            foundOne = true;
            izp = nullptr;
            while (foundOne == true) {
                foundOne = false;
                for (unsigned int pm : possibleMatchesBelow) {
                    izp = &(d->intensityPeaks[pm]);
                    mzDiff = startPeak->m_peakMz - izp->m_peakMz;
                    if (std::abs(minSpacing - mzDiff) < maxMzSpacingDiff) {
                        izp->m_label = nxLabel;
                        izp->m_matchDiff = minSpacing;
                        foundOne = true;
                        ++matchCount;
                        break;
                    }
                }
                startPeak = izp;
            }
        }
        ++count;
    }
}

int TileFeatureFinder::filterLocalMaxima(double mzDistance, double mzWidth, double timeWidth,
                                         double intensityTolerance, SearchDirection direction)
{
    if (d->queue.empty()) {
        return 0;
    }

    TilePositionConverter converter(d->range);
    ScanIndexTimeConverter scanIndexTimeConvert = loadConverter(&d->store.db());

    IntensityPriorityQueue filteredMaxima;
    
    // check every point
    while (!d->queue.empty()) {
        const IntensityPoint pt = d->queue.top();
        d->queue.pop();

        double mz = converter.levelToWorldPositionX(pt.globalTilePos.x(),d->level);
        
        // build up search area rectangle

        double searchAreaCenterMz;

        if (direction == SearchDirectionLeft) {
            searchAreaCenterMz = mz - mzDistance;
        } else if (direction == SearchDirectionRight) {
            searchAreaCenterMz = mz + mzDistance;
        }

        // mz part 
        double mzStart = searchAreaCenterMz - mzWidth;
        double mzEnd = searchAreaCenterMz + mzWidth;

        // ensure we are within valid domain range
        mzStart = qBound(d->range.minX(), mzStart, d->range.maxX());
        mzEnd = qBound(d->range.minX(), mzEnd, d->range.maxX());

        // time part
        double scanIndex = converter.levelToWorldPositionY(pt.globalTilePos.y(), d->level);
        double scanTime = scanIndexTimeConvert.scanIndexToTime(scanIndex);
        
        double timeStart = scanTime - timeWidth;
        double timeEnd = scanTime + timeWidth;

        // back from world coordinates (mz, scan index to global tile positions)

        // world coordinate is scan index, not scan time
        double scanIndexStart = scanIndexTimeConvert.timeToScanIndex(timeStart);
        double scanIndexEnd = scanIndexTimeConvert.timeToScanIndex(timeEnd);

        // ensure we are within valid domain range
        scanIndexStart = qBound<double>(d->range.minY(), scanIndexStart, d->range.maxY());
        scanIndexEnd = qBound<double>(d->range.minY(), scanIndexEnd, d->range.maxY());

        // region in global tile positions
        QRect tileArea = toTileArea(mzStart, mzEnd, scanIndexStart, scanIndexEnd);
        
        IntensityPriorityQueue queue = searchTileArea(tileArea);
        // we found some maxima there too
        if (queue.size() > 0) {
            // check found maxima if they are within tolerance
            double toleranceWidth = intensityTolerance * pt.intensity;
            double minIntensity = pt.intensity - toleranceWidth;
            
            bool foundMinIntensity = false;
            while (!queue.empty()) {
                IntensityPoint other = queue.top();
                queue.pop();
                
                if (other.intensity >= minIntensity) {
                    foundMinIntensity = true;
                    break;
                }
            }

            filteredMaxima.push(pt);
        }
    }

    d->queue = filteredMaxima;
    return d->queue.size();
}

void TileFeatureFinder::saveToCsv(const QString &csvFilePath, bool showOriginal, bool showAll)
{
    CsvWriter writer(csvFilePath, "\r\n", ',');
    if (!writer.open()) {
        warningMs() << "Cannot open destination file" << csvFilePath;
        return;
    }
    qDebug() << "saving to csv file: " << csvFilePath;

    // Header Row.
    writer.writeRow({ "mzStart", "mzEnd", "timeStart", "timeEnd", "label", // requested columns
        "intensity", "tileX", "tileY", "scanIndex" });  // debugging columns

    qDebug() << "total number of intensity peaks is: " << d->intensityPeaks.size();

    ScanIndexTimeConverter scanIndexTimeConvert = loadConverter(&d->store.db());
    TilePositionConverter converter(d->range);

    double mzWidth = d->range.levelStepX(d->level);

    for (IntensityPeak iPeak : d->intensityPeaks) {

        double intensity = iPeak.m_peakIntensity;

        // For now the label is intensity
        double labelIntensity = round(intensity * 10.) / 10.;
        QString intensityLabel = QString::number(labelIntensity, 'f', 1);
        QString label = iPeak.m_label + " (" + intensityLabel + ")";
        const QString intensityStr = QString::number(intensity, 'f', 6);

        const QString gptx = QString::number(iPeak.m_tileX);
        const QString gpty = QString::number(iPeak.m_tileY);

        double scanIndex;
        if (d->applyLevelOffset) {
            scanIndex = converter.levelToWorldPositionY(iPeak.m_tileY, d->level);
        } else {
            scanIndex = converter.scanIndexAt(iPeak.m_tileY);
        }

        const QString scanIndexStr = QString::number(scanIndex);

        // Get mzStart/mzEnd and timeStart/timeEnd from tileXStart, etc.
        double mzTemp, mzTempEnd;
        if (d->applyLevelOffset) {
            mzTemp = converter.levelToWorldPositionX(iPeak.m_tileStartX, d->level);
            mzTempEnd = converter.levelToWorldPositionX(iPeak.m_tileEndX, d->level);
        } else {
            mzTemp = converter.mzAt(iPeak.m_tileStartX);
            mzTempEnd = converter.mzAt(iPeak.m_tileEndX);
        }
        double mzStart = mzTemp - (mzWidth * 0.5);
        double mzEnd = mzTempEnd + (mzWidth * 0.5);

        // width of cell in 'units' of scanIndex.
        double scanIndexHalfWidth = 0.5 * d->range.levelStepY(d->level);

        double scanIndexTemp, scanIndexTempEnd;
        if (d->applyLevelOffset) {
            scanIndexTemp = converter.levelToWorldPositionY(iPeak.m_tileStartY, d->level);
            scanIndexTempEnd = converter.levelToWorldPositionY(iPeak.m_tileEndY, d->level);
        } else {
            scanIndexTemp = converter.scanIndexAt(iPeak.m_tileStartY);
            scanIndexTempEnd = converter.scanIndexAt(iPeak.m_tileEndY);
        }
        double timeStart =
            scanIndexTimeConvert.scanIndexToTime(scanIndexTemp - scanIndexHalfWidth);
        double timeEnd =
            scanIndexTimeConvert.scanIndexToTime(scanIndexTempEnd + scanIndexHalfWidth);
        
        // Get initial cell start/end in mz and time dimension.
        if (d->applyLevelOffset) {
            mzTemp = converter.levelToWorldPositionX(iPeak.m_tileX, d->level);
            scanIndexTemp = converter.levelToWorldPositionY(iPeak.m_tileY, d->level);
        } else {
            mzTemp = converter.mzAt(iPeak.m_tileX);
            scanIndexTemp = converter.scanIndexAt(iPeak.m_tileY);
        }
        double mzStartInit = mzTemp - (mzWidth * 0.5);
        double mzEndInit = mzTemp + (mzWidth * 0.5);
        double timeStartInit =
            scanIndexTimeConvert.scanIndexToTime(scanIndexTemp - scanIndexHalfWidth);
        double timeEndInit =
            scanIndexTimeConvert.scanIndexToTime(scanIndexTemp + scanIndexHalfWidth);

        // If it's a big peak and its not included in another peak, or
        // if it's labled as part of a sequence, then show it.
        if ((intensity >= MIN_PEAK_SIZE && !iPeak.m_capturedInOtherPeak)
            || iPeak.m_label.size() > 0) {

            const QString mzStartStr = QString::number(mzStart, 'g', 12);
            const QString mzEndStr = QString::number(mzEnd, 'g', 12);
            const QString timeStartStr = QString::number(timeStart, 'g', 12);
            const QString timeEndStr = QString::number(timeEnd, 'g', 12);

            writer.writeRow({ mzStartStr, mzEndStr, timeStartStr, timeEndStr, label, intensityStr,
                              gptx, gpty, scanIndexStr });

            if (showOriginal || showAll) {
                const QString mzStartInitStr = QString::number(mzStartInit, 'g', 12);
                const QString mzEndInitStr = QString::number(mzEndInit, 'g', 12);
                const QString timeStartInitStr = QString::number(timeStartInit, 'g', 12);
                const QString timeEndInitStr = QString::number(timeEndInit, 'g', 12);

                label = QString("");

                writer.writeRow({ mzStartInitStr, mzEndInitStr, timeStartInitStr, timeEndInitStr,
                                  label, intensityStr, gptx, gpty, scanIndexStr });
            }
        }
        else if (showAll) {
            const QString mzStartInitStr = QString::number(mzStartInit, 'g', 12);
            const QString mzEndInitStr = QString::number(mzEndInit, 'g', 12);
            const QString timeStartInitStr = QString::number(timeStartInit, 'g', 12);
            const QString timeEndInitStr = QString::number(timeEndInit, 'g', 12);

            label = QString("");

            writer.writeRow({ mzStartInitStr, mzEndInitStr, timeStartInitStr, timeEndInitStr, label,
                              intensityStr, gptx, gpty, scanIndexStr });
        }
    }
}

void TileFeatureFinder::saveToCsv(const QString &csvFilePath)
{
    CsvWriter writer(csvFilePath, "\r\n", ',');
    if (!writer.open()) {
        warningMs() << "Cannot open destination file" << csvFilePath;
        return;
    }

    writer.writeRow({ "mzStart", "mzEnd", "timeStart", "timeEnd", "label", // requested columns
                      "intensity", "tileX", "tileY", "scanIndex" }); // debugging columns

    ScanIndexTimeConverter scanIndexTimeConvert = loadConverter(&d->store.db());

    TilePositionConverter converter(d->range);
    IntensityPriorityQueue queue = d->queue;

    // TODO: feature finder will define the rectangle per feature, right now
    double mzWidth = d->range.levelStepX(d->level);

    int count = 0;
    int size = queue.size();

    while (!queue.empty()) {
        const IntensityPoint point = queue.top();
        const double intensity = point.intensity;

        double mz;
        double scanIndex;
        if (d->applyLevelOffset) {
            mz = converter.levelToWorldPositionX(point.globalTilePos.x(), d->level);
            scanIndex = converter.levelToWorldPositionY(point.globalTilePos.y(), d->level);
        } else {
            mz = converter.mzAt(point.globalTilePos.x());
            scanIndex = converter.scanIndexAt(point.globalTilePos.y());
        }

        double mzStart = mz - (mzWidth * 0.5);
        double mzEnd = mz + (mzWidth * 0.5);

        double scanIndexNext = scanIndex + d->range.levelStepY(d->level);

        double time_start = scanIndexTimeConvert.scanIndexToTime(scanIndex);
        double tsp1 =
            scanIndexTimeConvert.scanIndexToTime(scanIndex + d->range.levelStepY(d->level));
        double time_end = scanIndexTimeConvert.scanIndexToTime(scanIndexNext);

        double timeStepWidth = tsp1 - time_start;

        double timeStart = time_start - (timeStepWidth * 0.5);
        double timeEnd = time_end + (timeStepWidth * 0.5);

        const QString mzStartStr = QString::number(mzStart, 'g', 12);
        const QString mzEndStr = QString::number(mzEnd, 'g', 12);
        const QString timeStartStr = QString::number(timeStart, 'g', 12);
        const QString timeEndStr = QString::number(timeEnd, 'g', 12);

        const QString intensityStr = QString::number(intensity, 'g', 12);
        const QString gptx = QString::number(point.globalTilePos.x());
        const QString gpty = QString::number(point.globalTilePos.y());
        const QString scanIndexStr = QString::number(scanIndex);

        // For now the label is intensity
        double labelIntensity = round(intensity * 10.) / 10.;
        const QString label = QString::number(labelIntensity, 'g', 12);

        writer.writeRow({ mzStartStr, mzEndStr, timeStartStr, timeEndStr, label,
            intensityStr, gptx, gpty, scanIndexStr });

        ++count;
        queue.pop();
    }
}


QVector<IntensityPoint> TileFeatureFinder::localMaximaForTile(const Tile &t, const int radiusX,
                                                              const int radiusY)
{
    QVector<IntensityPoint> result;

    int localTileYStart = 0 + radiusY;
    int localTileYEnd = Tile::HEIGHT - radiusY - 1;

    int localTileXStart = 0 + radiusX;
    int localTileXEnd = Tile::WIDTH - radiusX - 1;

    for (int y = localTileYStart; y <= localTileYEnd; ++y) {
        for (int x = localTileXStart; x <= localTileXEnd; ++x) {
            bool isLocalMaximum = true;
            double localMaximumCandidate = t.value(x, y);

            // check all the neighbor values
            for (int ry = -radiusY; ry <= radiusY; ++ry) {
                for (int rx = -radiusX; rx <= radiusX; ++rx) {
                    if (rx == 0 && ry == 0) {
                        // don't check with self-value
                        continue;
                    }

                    int neighborX = x + rx;
                    int neighborY = y + ry;
                    double neighborValue = t.value(neighborX, neighborY);
                    if (neighborValue >= localMaximumCandidate) {
                        isLocalMaximum = false;
                        break;
                    }
                }

                if (!isLocalMaximum) {
                    break;
                }
            }

            if (isLocalMaximum) {

                const QPoint tilePos = t.tileInfo().pos();

                IntensityPoint item;
                item.globalTilePos
                    = QPoint(x + tilePos.x(),
                             y + tilePos.y()); // translate from local tile pos to global tile pos
                item.intensity = localMaximumCandidate;
                result.push_back(item);
            }
        }
    }

    return result;
}

double TileFeatureFinder::localMaximaForTilePos(const QRect &domainTileArea, const QPoint &tilePos,
                                                RandomTileIterator *const iterator)
{
    double intensity = INVALID_INTENSITY;
    // checked area radius (in manhattan distance)
    const int radiusX = 1;
    const int radiusY = 1;

    iterator->moveTo(d->level, tilePos.x(), tilePos.y());
    const double localMaximumCandidate = iterator->value();
    
    bool isLocalMaximum = true;
    // check if is local maxima within this area
    for (int ry = -radiusY; ry <= radiusY; ++ry) {
        for (int rx = -radiusX; rx <= radiusX; ++rx) {
            if (rx == 0 && ry == 0) {
                // don't check with self-value
                continue;
            }

            const int neighborX = tilePos.x() + rx;
            const int neighborY = tilePos.y() + ry;

            // if contains
            double neighborValue = 0.0;
            if (domainTileArea.contains(neighborX, neighborY)) {
                iterator->moveTo(d->level, neighborX, neighborY);
                neighborValue = iterator->value();
            } else {
                // behind the borders of the whole domain we assume 0.0 signal
                neighborValue = 0.0;
            }

            if (neighborValue >= localMaximumCandidate) {
                isLocalMaximum = false;
                break;
            }
        }

        if (!isLocalMaximum) {
            break;
        }
    }

    if (isLocalMaximum) {
        intensity = localMaximumCandidate;
    }

    return intensity;
}

QRect TileFeatureFinder::wholeDomainTileIndexArea() const
{
    const double mzStart = d->range.minX();
    const double mzEnd = d->range.maxX();

    const int scanIndexStart = d->range.minY();
    const int scanIndexEnd = d->range.maxY();

    return toTileIndexArea(mzStart, mzEnd, scanIndexStart, scanIndexEnd);
}

QRect TileFeatureFinder::wholeDomainTileArea() const
{
    const double mzStart = d->range.minX();
    const double mzEnd = d->range.maxX();

    const int scanIndexStart = d->range.minY();
    const int scanIndexEnd = d->range.maxY();

    return toTileArea(mzStart, mzEnd, scanIndexStart, scanIndexEnd);
}

IntensityPriorityQueue TileFeatureFinder::searchTileIndexArea(const QRect &tileIndexArea)
{
    // todo: think about iterative search tile areas
    IntensityPriorityQueue queue;

    // TODO: refactor and do just like with NonUniform; store column indexes, not tile positions
    for (int row = tileIndexArea.top(); row < tileIndexArea.bottom(); ++row) {
        int tilePosY = row * Tile::HEIGHT; // translate from row to tile position
        for (int col = tileIndexArea.left(); col < tileIndexArea.right(); ++col) {
            int tilePosX = col * Tile::WIDTH;

            Tile t = d->store.loadTile(d->level, QPoint(tilePosX, tilePosY));

            if (t.isNull()) {
                qWarning() << "null tile loaded!";
            }

            QVector<IntensityPoint> result = localMaximaForTile(t, 1, 1);
            for (const IntensityPoint &point : result) {
                IntensityPriorityQueueUtils::pushWithLimit(&queue, point, d->topk);
            }
        }
    }

    return queue;
}

IntensityPriorityQueue TileFeatureFinder::searchTileArea(const QRect &tileArea)
{
    const QRect domainTileArea = wholeDomainTileArea();

    IntensityPriorityQueue queue;

    TileManager manager(&d->store);
    RandomTileIterator iterator(&manager);
    // now iterate the tiles in a complicated way: visit every tile just once
    int rowsRemaining = tileArea.height();
    int tileY = tileArea.y();
    
    while (rowsRemaining > 0) {
        int tileX = tileArea.x();

        qint32 columnsRemaining = tileArea.width();
        qint32 numContiguousTileRows = iterator.numContiguousRows(tileY);

        qint32 rowsToWork = qMin(numContiguousTileRows, rowsRemaining);

        while (columnsRemaining > 0) {
            qint32 numContiguousColumns = iterator.numContiguousCols(tileX);
            qint32 columnsToWork = qMin(numContiguousColumns, columnsRemaining);

            for (int rows = 0; rows < rowsToWork; ++rows) {
                int globalTilePositionY = tileY + rows;
                for (int cols = 0; cols < columnsToWork; ++cols) {
                    // tile coordinate
                    int globalTilePositionX = tileX + cols;

                    int tileInfoX = TileManager::normalizeX(globalTilePositionX);
                    int tileInfoY = TileManager::normalizeY(globalTilePositionY);
                    iterator.setMainTile(TileInfo(d->level, QPoint(tileInfoX, tileInfoY)));

                    QPoint globalTilePos(globalTilePositionX, globalTilePositionY);
                    double intensity = localMaximaForTilePos(domainTileArea, globalTilePos, &iterator);
                    if (intensity != INVALID_INTENSITY) {
                        IntensityPoint item;
                        item.globalTilePos = globalTilePos;
                        item.intensity = intensity;
                        IntensityPriorityQueueUtils::pushWithLimit(&queue, item, d->topk);
                    }
                }
            }

            tileX += columnsToWork;
            columnsRemaining -= columnsToWork;
        }

        tileY += rowsToWork;
        rowsRemaining -= rowsToWork;
    }

    return queue;
}

void TileFeatureFinder::readTileArea(const QRect &tileArea, MSEquispacedData *rd)
{
    const QRect domainTileArea = wholeDomainTileArea();
    TileManager manager(&d->store);
    RandomTileIterator iterator(&manager);

    int tileY = tileArea.y();
    double intensity = 0.0;
    int x = 0;

    rd->setSize(tileArea.width(), tileArea.height(), d->range.minX(), d->range.stepX(), 
        d->range.minY(), d->range.stepY());

    // now iterate the tiles in a complicated way: visit every tile just once
    int rowsRemaining = tileArea.height();

    while (rowsRemaining > 0) {

        int tileX = tileArea.x();
        qint32 numContiguousTileRows = iterator.numContiguousRows(tileY);
        qint32 rowsToWork = qMin(numContiguousTileRows, rowsRemaining);

        qint32 columnsRemaining = tileArea.width();
        
        while (columnsRemaining > 0) {
            qint32 numContiguousColumns = iterator.numContiguousCols(tileX);
            qint32 columnsToWork = qMin(numContiguousColumns, columnsRemaining);

            for (int rows = 0; rows < rowsToWork; ++rows) {
                int globalTilePositionY = tileY + rows;
                for (int cols = 0; cols < columnsToWork; ++cols) {
                    // tile coordinate
                    int globalTilePositionX = tileX + cols;

                    int tileInfoX = TileManager::normalizeX(globalTilePositionX);
                    int tileInfoY = TileManager::normalizeY(globalTilePositionY);
                    iterator.setMainTile(TileInfo(d->level, QPoint(tileInfoX, tileInfoY)));

                    QPoint globalTilePos(globalTilePositionX, globalTilePositionY);

                    iterator.moveTo(d->level, globalTilePos.x(), globalTilePos.y());
                    intensity = iterator.value();
                 
                    rd->setPoint(globalTilePositionX, globalTilePositionY, intensity);
                }
            }
            tileX += columnsToWork;
            columnsRemaining -= columnsToWork;
        }
        tileY += rowsToWork;
        rowsRemaining -= rowsToWork;
    }
}

QRect TileFeatureFinder::toTileIndexArea(double mzStart, double mzEnd, int scanIndexStart,
                                       int scanIndexEnd) const
{
    /*
    Types of coordinates:
    local tile coordinate:
    x: 0..63 (Tile::WIDTH)
    y: 0..63 (Tile::HEIGHT)

    global tile coordinates: (on set of tiles)
    x: 0..TileRange::size().x()
    y: 0..TileRange::size().y()

    tile index (on set of tiles)
    columns : 0..max global tile coordinateX / Tile::Width
    rows    : 0..max global tile coordinateY / Tile::Height

    world coordinate
    mz: TilesRange::minX..TilesRange::maxX
    scanIndex: TilesRange::minY..TilesRange::maxY

    scanTime: toScanIndex(scanTime)
    */
    TilePositionConverter converter(d->range);

    QRect result;
    int globalTileStartX = 0;
    int globalTileEndX = 0;
    
    int globalTileStartY = 0;
    int globalTileEndY = 0;

    if (d->applyLevelOffset) {
        // from mz to global tile coordinate
        globalTileStartX = converter.worldToLevelPositionX(mzStart, d->level);
        globalTileEndX = converter.worldToLevelPositionX(mzEnd, d->level);

        // from scan index  to global tile coordinate
        globalTileStartY = converter.worldToLevelPositionY(scanIndexStart, d->level);
        globalTileEndY = converter.worldToLevelPositionY(scanIndexEnd, d->level);
    } else {
        // from mz to global tile coordinate
        globalTileStartX = converter.globalTileX(mzStart);
        globalTileEndX = converter.globalTileX(mzEnd);

        // from scan index  to global tile coordinate
        globalTileStartY = converter.globalTileY(scanIndexStart);
        globalTileEndY = converter.globalTileY(scanIndexEnd);
    }

    // to tile index (cols, rows)
    int firstColumn = TileManager::xToColumn(globalTileStartX);
    int lastColumn = TileManager::xToColumn(globalTileEndX);

    int firstRow = TileManager::yToRow(globalTileStartY);
    int lastRow = TileManager::yToRow(globalTileEndY);

    return QRect(QPoint(firstColumn, firstRow), QPoint(lastColumn, lastRow));
}

QRect TileFeatureFinder::toTileArea(double mzStart, double mzEnd, int scanIndexStart, int scanIndexEnd) const
{
    TilePositionConverter converter(d->range);
    // convert it all to global tile positions
    int globalTileStartX = converter.worldToLevelPositionX(mzStart, d->level);
    int globalTileEndX = converter.worldToLevelPositionX(mzEnd, d->level);

    // from scan index  to global tile coordinate
    int globalTileStartY = converter.worldToLevelPositionY(scanIndexStart, d->level);
    int globalTileEndY = converter.worldToLevelPositionY(scanIndexEnd, d->level);

    // rectangle in global tile coordinates
    return QRect(QPoint(globalTileStartX, globalTileStartY), QPoint(globalTileEndX, globalTileEndY));
}

void TileFeatureFinder::findMinMaxIntensityInArea(const QRectF &area, int id, QVector<double> *minIntensity, QVector<double> *maxIntensity)
{
    TileStoreSqlite store(d->store.filePath());

    TileRange range = d->range;
    TilePositionConverter converter(range);
    const QPoint baseLevel(1, 1);
    converter.setCurrentLevel(baseLevel);

    TileManager manager(&store);
    RandomTileIterator iterator(&manager);

    //TODO: fix converter
    QPointF workAroundPoint(d->range.levelStepX(baseLevel) * 0.5, d->range.levelStepY(baseLevel) * 0.5);
    QRect tileRect = converter.worldToLevelRect(area, baseLevel).translated(workAroundPoint).toAlignedRect();

    iterator.moveTo(baseLevel, tileRect.left(), tileRect.top());
    double min = iterator.value();
    double max = min;

    for (int y = tileRect.top(); y <= tileRect.bottom(); ++y) {
        for (int x = tileRect.left(); x <= tileRect.right(); ++x) {
            iterator.moveTo(baseLevel, x, y);

            double intensity = iterator.value();
            if (intensity > max) {
                max = intensity;
            }

            if (intensity < min) {
                min = intensity;
            }
        }
    }

    minIntensity->data()[id] = min;
    maxIntensity->data()[id] = max;
}

void TileFeatureFinder::findMinMaxIntensityInAreaFast(const QRectF &area, int id,
                                                      QVector<double> *minIntensity,
                                                      QVector<double> *maxIntensity)
{
    TileStoreSqlite store(d->store.filePath());

    TileRange range = d->range;
    TilePositionConverter converter(range);
    const QPoint baseLevel(1, 1);
    converter.setCurrentLevel(baseLevel);

    TileManager manager(&store);

    // TODO: fix converter
    QPointF workAroundPt(d->range.levelStepX(baseLevel) * 0.5,
                         d->range.levelStepY(baseLevel) * 0.5);
    QRect tileRect
        = converter.worldToLevelRect(area, baseLevel).translated(workAroundPt).toAlignedRect();

    SequentialTileIterator iterator(&manager, tileRect, baseLevel);

    double min = 0;
    double max = 0;

    while (iterator.hasNext()) {
        double intensity = iterator.next();
        if (intensity > max) {
            max = intensity;
        }

        if (intensity < min) {
            min = intensity;
        }
    }

    minIntensity->data()[id] = min;
    maxIntensity->data()[id] = max;
}

_PMI_END
