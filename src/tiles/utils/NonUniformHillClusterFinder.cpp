/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "NonUniformHillClusterFinder.h"

// common_ms
#include "MSDataNonUniform.h"
#include "MSDataNonUniformAdapter.h"
#include "NonUniformFeature.h"
#include "NonUniformFeatureFindingSession.h"
#include "NonUniformHillCluster.h"
#include "ProgressBarInterface.h"
#include "ProgressContext.h"
#include "pmi_common_ms_debug.h"

// common_tiles
#include <MzScanIndexRect.h>
#include <NonUniformTileBuilder.h>
#include <NonUniformTileDevice.h>
#include <NonUniformTileHillFinder.h>
#include <NonUniformTileIntensityIndex.h>
#include <NonUniformTileManager.h>
#include <NonUniformTileMaxIntensityFinder.h>
#include <NonUniformTileStoreMemory.h>
#include <NonUniformTileStoreSqlite.h>
#include <RandomNonUniformTileIterator.h>


// common_core_mini
#include <AbstractChargeDeterminator.h>
#include <AbstractMonoisotopeDeterminator.h>
#include <ChargeDeterminator.h>
#include <CosineCorrelator.h>
#include <MonoisotopeDeterminator.h>

#include <QColor>
#include <QElapsedTimer>
#include <QString>
#include <QtGlobal>


#define SHOW_DEBUG_LOG false
#define NonUniformHillClusterFinderDebug if (!SHOW_DEBUG_LOG) {} else debugMs

_PMI_BEGIN

using namespace msreader;

const double NonUniformHillClusterFinder::INVALID_INTENSITY = -1.0;

class Q_DECL_HIDDEN NonUniformHillClusterFinder::Private
{
public:
    Private(NonUniformFeatureFindingSession *ses)
        : session(ses)

    {
        Q_ASSERT(ses);
    }

    NonUniformFeatureFindingSession *session;

    double percLimit = 1.0; // 100% by default
    double minIntensity = NonUniformHillClusterFinder::INVALID_INTENSITY;

    bool stop = false;

    QScopedPointer<AbstractChargeDeterminator> chargeDeterminator;
    QScopedPointer<AbstractMonoisotopeDeterminator> monoisotopeDeterminator;

public:
    point2dList xicData(const MzScanIndexRect &hillRect, const ScanIndexNumberConverter &converter);

    XICWindow fromMzScanIndexHill(const MzScanIndexRect &hillRect,
                                  const ScanIndexNumberConverter &converter);

    NonUniformHillCluster buildCluster(double mz, int scanIndex,
                                       const ScanIndexNumberConverter &converter,
                                       const QVector<double> &mzFromCharge,
                                       NonUniformTileHillFinder &hillFinder);

    /*!
    * \brief Builds cluster with hill that contains what \a tilePoint points to
    */
    NonUniformHillCluster buildDefaultCluster(const NonUniformTilePoint &tilePoint,
        NonUniformTileHillFinder &hillFinder);
};

NonUniformHillClusterFinder::NonUniformHillClusterFinder(NonUniformFeatureFindingSession *session,
                                                         QObject *parent)
    : QObject(parent)
    , d(new Private(session))
{
    ChargeDeterminator *defaultDeterminator = new ChargeDeterminator();
    defaultDeterminator->setIsotopeSpacing(ISODIFF);
    setChargeDeterminator(defaultDeterminator);

    MonoisotopeDeterminator *defaultMonoDeterminator = new MonoisotopeDeterminator();
    setMonoisotopeDeterminator(defaultMonoDeterminator);
}

NonUniformHillClusterFinder::~NonUniformHillClusterFinder()
{
}

void NonUniformHillClusterFinder::setPercentLimit(double percLimit)
{
    d->percLimit = qBound(0.01, percLimit, 1.0);
}

void NonUniformHillClusterFinder::setIntensityThreshold(double minIntensity)
{
    d->minIntensity = minIntensity;
}

void NonUniformHillClusterFinder::setChargeDeterminator(
    AbstractChargeDeterminator *chargeDeterminator)
{
    Q_ASSERT(chargeDeterminator);
    d->chargeDeterminator.reset(chargeDeterminator);
}

void NonUniformHillClusterFinder::setMonoisotopeDeterminator(
    AbstractMonoisotopeDeterminator *monoisotopeDeterminator)
{
    Q_ASSERT(monoisotopeDeterminator);
    d->monoisotopeDeterminator.reset(monoisotopeDeterminator);
}

// prototype temporary code: if you see this in 6 months used, remove it
// TODO: use averigine distribution properly
void searchRadiusFromMass(int mass, int *left, int *right)
{
    // taken manually from Averigine CSV file
    // take the row in csv file, find maximum intensity, count items before that column and count
    // items after that max column to determine left and right count
    /*
        < interval >, items on left, items on right
        < 100 - 299 >, 0, 2
        < 300 - 799 >, 0, 3
        < 800 - 1399 >, 0, 4
        < 1400 - 1699 >, 0, 5
        < 1700 - 3099 >, 0, 6
        < 3100 - 3999 >, 1, 6
        < 4000 - 4499 >, 2, 6
        < 4500 - 6299 >, 2, 7
        < 6300 - 6799 >, 3, 7
        < 6800 - 7499 >, 4, 7
        < 7500 - 9399 > 5, 7
        < 94 - 9899 >, 5, 8
        < 9900 - 12199 >, 6, 8
        < 12200, 124099 >, 7, 8,
        ...
    */

    // int - mass , QPair <isotope count on left , isotope count on right>
    QMap<int, QPair<int, int> > intervalMap = 
    {
        {99, { 0 , 0 }},
        {299,{ 0 , 2 }},
        {799,{ 0 , 3 } },
        {1399,{ 0 , 4 } },
        {1699,{ 0 , 5 } },
        { 3099,{ 0 , 6 } },
        { 3999,{ 1 , 6 } },
        { 4499,{ 2 , 6 } },
        { 6299,{ 2 , 7 } },
        { 6799,{ 3 , 7 } },
        { 7499,{ 4 , 7 } },
        { 9399,{ 5 , 7 } },
        { 9899,{ 5 , 8 } },
        { 12199,{ 6 , 8 } },
        { 12499,{ 7 , 8 } },
        { 99900, {9 , 9} }
    };

    auto iter = intervalMap.lowerBound(mass);
    if (iter != intervalMap.end()) {
        *left = iter->first;
        *right = iter->second;
    } else {
        warningMs() << "For mass" << mass << "value wasn't found!";
        *left = 3;
        *right = 3;
    }
}

NonUniformHillCluster NonUniformHillClusterFinder::Private::buildDefaultCluster(
    const NonUniformTilePoint &tilePoint, NonUniformTileHillFinder &hillFinder)
{
    NonUniformHillCluster cluster;

    NonUniformHill parentHill = hillFinder.makeDefaultHill(tilePoint);
    if (parentHill.isNull()) {
        return cluster;
    }

    parentHill.setCorrelation(1.0);
    parentHill.setId(hillFinder.nextId());
    cluster.push_back(parentHill);
    hillFinder.markPointsAsProcessed(parentHill.points(), session->selectionTileManager());

    return cluster;
}

NonUniformHillCluster NonUniformHillClusterFinder::Private::buildCluster(
    double mz, int scanIndex, const ScanIndexNumberConverter &converter,
    const QVector<double> &mzFromCharge, NonUniformTileHillFinder &hillFinder)
{
    NonUniformHillCluster cluster;
    // detect parent hill
    NonUniformHill parentHill = hillFinder.explainPeak(session->selectionTileManager(), mz, scanIndex);
    if (parentHill.isNull()) {
        // we don't have cluster here
        return cluster;
    }

    

    parentHill.setCorrelation(1.0);
    parentHill.setId(hillFinder.nextId());
    cluster.push_back(parentHill);
    hillFinder.markPointsAsProcessed(parentHill.points(), session->selectionTileManager());
    
    // detect children now and take those whose XIC correlation is nice
    
    // get parent XIC
    point2dList parentXicData = xicData(parentHill.area(), converter);
    CosineCorrelator correlator(parentXicData);

    for (double neighborMz : qAsConst(mzFromCharge)) {
        NonUniformHill neighborHill
            = hillFinder.explainNeighbor(session->selectionTileManager(), neighborMz, parentHill.area());

        if (!neighborHill.isNull()) {
            // check cosine correlation
            const double cosineCorrelationLimit = 0.90;

            // compute the correlations
            point2dList neighborXIC = xicData(neighborHill.area(), converter);

            double similarity = 0.0;
            if (!neighborXIC.empty()) {
                similarity = correlator.cosineSimilarityTo(neighborXIC);
            } else {
                warningMs() << "Neighbor XIC is empty! Not expected!";
            }

            if (similarity < cosineCorrelationLimit) {
                // not part of our cluster
                NonUniformHillClusterFinderDebug() << "Excluding" << neighborHill.points().size()
                                                   << "points with similarity" << similarity;
            } else {
                neighborHill.setId(hillFinder.nextId());
                neighborHill.setCorrelation(similarity);
                cluster.push_back(neighborHill);
                hillFinder.markPointsAsProcessed(neighborHill.points(), session->selectionTileManager());
            }
        }
    }

    return cluster;
}

point2dList NonUniformHillClusterFinder::extractCrossSection(double mz, int scanIndex)
{
    point2dList result;
    // share the size of the scan for both determinators
    double crossSectionRadius
        = std::max(ChargeDeterminator::searchRadius(), MonoisotopeDeterminator::searchRadius());

    double mzStart = mz - crossSectionRadius;
    double mzEnd = mz + crossSectionRadius;

    // bound it
    const MzScanIndexRect &searchArea = d->session->searchArea();
    mzStart = qBound(searchArea.mz.start(), mzStart, searchArea.mz.end());
    mzEnd = qBound(searchArea.mz.start(), mzEnd, searchArea.mz.end());

    
    auto scanExtractor = d->session->document()->nonUniformData();

    Err e = scanExtractor->getScanDataPart(scanIndex, mzStart, mzEnd, &result,
                                           d->session->device()->doCentroiding());
    if (e != kNoErr) {
        warningMs() << "getScanDataPart failed with error" << e;
    }

    return result;
}


void NonUniformHillClusterFinder::run(QSharedPointer<ProgressBarInterface> progress)
{
    if (d->session->searchAreaTile().isNull()) {
        return;
    }

    d->stop = false;

    bool ok = d->session->begin();
    if (!ok) {
        warningMs() << "Failed to start the session!";
        return;
    }

    if (d->session->hillIndexManager()) {
        connect(this, &NonUniformHillClusterFinder::hillItemDetails, this,
                &NonUniformHillClusterFinder::markHills);
    }

    NonUniformTilePoint pt; // output from find&mark

    // Hill finder
    NonUniformTileHillFinder hillFinder(d->session);
    // limit by tile rect, but in world coordinates
    hillFinder.setMzTolerance(0.05);
    hillFinder.resetId();

    int featureId = 0;
    
    // find the maximum and mark in the shared selection store, return the point location
    int groupId = 0;
    ScanIndexNumberConverter converter = d->session->converter();

    NonUniformTileDevice *device = d->session->device();

    // TODO: fix debt, separate charge math/algebra from determinator classes 
    ChargeDeterminator chargeDeterminator;
    chargeDeterminator.setIsotopeSpacing(ISODIFF); // hdx is 1.006

    MonoisotopeDeterminator monoDeterminator; 

    int pointsProcessedCount = 0;
    int totalPointCount = static_cast<int>(d->session->totalPointCount());


// allows you to stop after certain count of hill clusters are processed 
//#define LIMIT_GROUP_COUNT
#ifdef LIMIT_GROUP_COUNT
    int topGroupLimit = 6;
#endif

    if (progress) {
        progress->setText(QString("Finding features..."));
    }

    ProgressContext progressContext(totalPointCount, progress);
    while (pointsProcessedCount < totalPointCount) {
        if (d->stop) {
            break;
        }
        
        if (d->percLimit < 1.0) {
            double percentageDone = pointsProcessedCount / double(totalPointCount);
            if (percentageDone >= d->percLimit) {
                break;
            }
        }

        if (progress) {
            if (progress->userCanceled()) {
                warningMs() << "Finding features canceled";
                break;
            }
        }

        QVector<NonUniformHillCluster> chargeClusters;

        // take the most intense point
        point2d mzIntensity;
        d->session->maxIntensity(&mzIntensity, &pt);
        if (pt.tilePos == NonUniformTilePoint::INVALID_TILE_POSITION) {
            // we are done if we don't have valid maximum
            break;
        }
        
        double mz = mzIntensity.x();
        double topIntensity = mzIntensity.y();
        
        if (d->minIntensity != NonUniformHillClusterFinder::INVALID_INTENSITY) {
            if (topIntensity < d->minIntensity) {
                warningMs() << "Stopped feature finding with min intensity" << d->minIntensity
                            << "when peak intensity is" << topIntensity;
                break;
            }
        }

        // extract cross section
        point2dList scanPart = extractCrossSection(mz, pt.scanIndex);
        
        int charge = d->chargeDeterminator->determineCharge(scanPart, mz);
        
        // determine mono isotope here 
        double score = 0.0;
        int offset
            = d->monoisotopeDeterminator->determineMonoisotopeOffset(scanPart, mz, charge, &score);

        double monoisotopicMz = monoDeterminator.fromOffsetToMz(offset, mz, charge);

        // FYI see mr_from_mz, similar function
        double unchargedMass = (monoisotopicMz * charge) - (charge * HYDROGEN);

        // how many places we look at on the right and on the left
        // by default we look 3 items left, 3 item right
        int left = 3;
        int right = 3;
        searchRadiusFromMass(qRound(unchargedMass), &left, &right);

        // make the hint from averagine distribution slightly bigger and let the cosine similarity
        // exclude others
        const int hintExtensionSize = 1;
        left += hintExtensionSize;
        right += hintExtensionSize;
        Interval<int> searchRadius = Interval<int>(-left, right);

        QVector<qreal> mzFromCharge;
        if (charge > 0) {
            mzFromCharge = chargeDeterminator.generateMzForCharge(mz, charge, searchRadius);
        } else {
            // failed to determine charge
        }

        NonUniformHillCluster cluster = d->buildCluster(mz, pt.scanIndex, converter, mzFromCharge, hillFinder);
        if (cluster.isEmpty()) {
            DEBUG_WARNING_LIMIT(warningMs() << "Main charge cluster is empty!", 5);
            cluster = d->buildDefaultCluster(pt, hillFinder);
            if (cluster.isEmpty()) {
                qFatal("Unexpected: default cluster cannot be empty!");
            }
        }

        featureId++;
        groupId++;
        cluster.setId(groupId);
        cluster.setCharge(charge);
        cluster.setMonoisotopicMz(monoisotopicMz);
        cluster.setScanIndex(pt.scanIndex);
        cluster.setMaximumIntensity(topIntensity);

        chargeClusters.append(cluster);

        // compute unique set of tile coordinates which are marked as processed by hill finder
        QSet<QPoint> tilePositionsToReindex;

        publishHillCluster(cluster);
        
        pointsProcessedCount += cluster.totalPoints();
        if (progress) {
            progress->incrementProgress(cluster.totalPoints());
        }

        tilePositionsToReindex.unite(cluster.uniqueTilePositions());
        // compute mz, scanIndex stays

        // most abundant ion 
        double parentMz = mz;
        int parentScanIndex = pt.scanIndex;

        const int firstCharge = 1;
        const int lastCharge = 10;

        for (int i = firstCharge; i <= lastCharge; ++i) {
            if (i == cluster.charge()) {
                continue;
            }
            
            int neighborCharge = i;
            double nextMz = ChargeDeterminator::nextChargeState(charge, i, parentMz);
            QVector<double> localMzFromCharge = chargeDeterminator.generateMzForCharge(nextMz, neighborCharge, searchRadius);
            
            NonUniformHillCluster neighborCluster
                = d->buildCluster(nextMz, parentScanIndex, converter, localMzFromCharge, hillFinder);

            if (!neighborCluster.isNull()) {
                groupId++;
                neighborCluster.setId(groupId);
                neighborCluster.setCharge(neighborCharge);
                const double invalidIntensity = -1.0;
                neighborCluster.setMaximumIntensity(invalidIntensity);
                
                chargeClusters.append(neighborCluster);
                
                publishHillCluster(neighborCluster);

                pointsProcessedCount += neighborCluster.totalPoints();
                if (progress) {
                    progress->incrementProgress(neighborCluster.totalPoints());
                }
                tilePositionsToReindex.unite(neighborCluster.uniqueTilePositions());
            }
        }

        NonUniformFeature feature(chargeClusters);
        feature.setId(featureId);
        feature.setUnchargedMass(unchargedMass);
        double apexTime = d->session->converter().scanIndexToScanTime(pt.scanIndex);
        feature.setApexTime(apexTime);

        emit featureFound(feature);

#ifdef LIMIT_GROUP_COUNT
        if (groupId > topGroupLimit) {
            break;
        }
#endif

        // update index
        d->session->updateIndexForTiles(tilePositionsToReindex.toList());
    } // end of find & mark
}

void NonUniformHillClusterFinder::stop()
{
    d->stop = true;
}

XICWindow
NonUniformHillClusterFinder::Private::fromMzScanIndexHill(const MzScanIndexRect &hillRect,
                                                          const ScanIndexNumberConverter &converter)
{
    XICWindow result;
    result.mz_start = hillRect.mz.start();
    result.mz_end = hillRect.mz.end();
    // convert scan index to scan time
    // but hills are <start, end)
    result.time_start = converter.scanIndexToScanTime(hillRect.scanIndex.start());
    result.time_end = converter.scanIndexToScanTime(hillRect.scanIndex.end() - 1);

    return result;
}

point2dList NonUniformHillClusterFinder::Private::xicData(const MzScanIndexRect &hillRect,
                                                          const ScanIndexNumberConverter &converter)
{
    XICWindow parentXIC = fromMzScanIndexHill(hillRect, converter);
    point2dList result;
    // uniform contains just ms1 level
    const int msLevel = 1;
    session->document()->getXICData(parentXIC, &result, msLevel);
    return result;
}

void NonUniformHillClusterFinder::publishHillCluster(const NonUniformHillCluster &hillCluster)
{
    QVector<NonUniformHill> hills = hillCluster.hills();
    for (int i = 0; i < hills.size(); ++i) {
        const NonUniformHill &hill = hillCluster.hills().at(i);
        emit hillItemDetails(hill.points(), hill.id());
    }
}

void NonUniformHillClusterFinder::markHills(const QVector<NonUniformTilePoint> &hillPoints,
                                            int featureId)
{
    NonUniformTileHillIndexManager *hillIndexManager = d->session->hillIndexManager();

    Q_ASSERT(hillIndexManager);

    NonUniformTileDevice *device = d->session->device();
    RandomNonUniformTileHillIndexIterator hillIndexIterator(
        hillIndexManager, device->range(),
        device->doCentroiding());

    hillIndexIterator.setCacheSize(0);

    NonUniformTileHillIndexStoreMemory *memoryStore
        = dynamic_cast<NonUniformTileHillIndexStoreMemory *>(hillIndexManager->store());

    if (!memoryStore) {
        qFatal("For now only in-memory selection is implemented");
        return;
    }

    for (int i = 0; i < hillPoints.size(); ++i) {
        const NonUniformTilePoint &hillPoint = hillPoints.at(i);

        hillIndexIterator.moveTo(hillPoint.tilePos.x(), hillPoint.tilePos.y(), hillPoint.scanIndex);
        QVector<int> hillIndex = hillIndexIterator.value();

        hillIndex[hillPoint.internalIndex] = featureId;

        // write to store directly, we don't have Write iterators right now
        // so update the tile in the store now
        int tileOffset = device->range().tileOffset(hillPoint.scanIndex);
        memoryStore->tile(hillPoint.tilePos, device->content()).setValue(tileOffset, hillIndex);
    }
}

_PMI_END
