/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "ClusterFinder.h"
#include "FinderFeaturesDao.h"
#include "FinderSamplesDao.h"
#include "InsilicoPeptidesCsvFormatter.h"
#include "ProgressBarInterface.h"
#include "ProgressContext.h"
#include "pmi_common_ms_debug.h"

// core_mini
#include <CsvWriter.h>

#include <QSqlDatabase>
#include <QVector>

_PMI_BEGIN

class Q_DECL_HIDDEN ClusterFinder::Private
{
public:
    Private(QSqlDatabase *database)
        : db(database)
    {
    }

    QSqlDatabase *db;
    NeighborSettings settings;
};

ClusterFinder::ClusterFinder(QSqlDatabase *db)
    : d(new ClusterFinder::Private(db))
{
}

ClusterFinder::~ClusterFinder()
{
}

Err ClusterFinder::run(QSharedPointer<ProgressBarInterface> progress)
{
    Err e = kNoErr;
    int groupNumber = 0;

    FinderFeaturesDao featuresDao(d->db);
    e = featuresDao.makeIndexes(); ree;

    const int INVALID_FEATURE_ID = -1;
    int firstId = INVALID_FEATURE_ID;
    int lastId = INVALID_FEATURE_ID;

    e = featuresDao.firstItemId(&firstId); ree;
    if (firstId == INVALID_FEATURE_ID) {
        warningMs() << "No features to cluster! We are done.";
        return kNoErr;
    }
    
    e = featuresDao.lastItemId(&lastId); ree;
    Q_ASSERT(lastId != INVALID_FEATURE_ID);

    FinderSamplesDao samplesDao(d->db);

    QList<int> sampleIds;
    e = samplesDao.uniqueIds(&sampleIds); ree;
    if (sampleIds.isEmpty()) {
        warningMs() << "Empty ids for samples";
        rrr(kError);
    }

    const int CACHE_SIZE = 2 * 1024;
    QMap<int, int> featureIdGroupNumberCache;

    {
        const int size = lastId - firstId + 1;
        ProgressContext progressContext(size, progress);
        for (int i = firstId; i <= lastId; ++i, ++progressContext) {
            if (progress && progress->userCanceled()) {
                break;
            }

            FinderFeaturesDaoEntry entry;
            e = featuresDao.loadFeature(i, &entry); ree;
            QVector<FinderFeaturesDaoEntry> cluster;

            if (entry.groupNumber == -1 && !featureIdGroupNumberCache.contains(entry.id)) {
                // determine group number
                cluster.push_back(entry);

                // fetch all neighbor features which are not grouped yet
                // and are from different sample
                QVector<FinderFeaturesDaoEntry> neighbors;
                e = featuresDao.loadNeighborFeatures(entry, d->settings, &neighbors); ree;

                // remove items that are grouped in cache!
                QMutableVectorIterator<FinderFeaturesDaoEntry> it(neighbors);
                while (it.hasNext()) {
                    const FinderFeaturesDaoEntry &feature = it.next();
                    if (featureIdGroupNumberCache.contains(feature.id)) {
                        it.remove();
                    }
                }

                // for each sample select just one, "best" candidate
                for (int sampleId : sampleIds) {
                    // skip the same sample id features, we are not going to add them to cluster
                    if (sampleId == entry.samplesId) {
                        continue;
                    }

                    FinderFeaturesDaoEntry candidate = bestCandidate(sampleId, entry, neighbors);
                    if (!candidate.isNull()) {
                        cluster.push_back(candidate);
                    }
                }

                // update group number
                groupNumber++;
                for (const FinderFeaturesDaoEntry &entry : qAsConst(cluster)) {
                    featureIdGroupNumberCache.insert(entry.id, groupNumber);
                }

                if (featureIdGroupNumberCache.size() > CACHE_SIZE) {
                    e = featuresDao.updateGroupNumbers(featureIdGroupNumberCache); ree;
                    featureIdGroupNumberCache.clear();
                }
            }
        }
    }

    if (!featureIdGroupNumberCache.isEmpty()) {
        e = featuresDao.updateGroupNumbers(featureIdGroupNumberCache); ree;
        featureIdGroupNumberCache.clear();
    }

    return e;
}

void ClusterFinder::setClusteringSettings(const NeighborSettings &settings)
{
    d->settings = settings;
}

NeighborSettings ClusterFinder::clusteringSettings() const
{
    return d->settings;
}

Err ClusterFinder::exportToInsilicoPeptideCsv(const QString &csvFilePath) const
{
    Err e = kNoErr;
    if (!d->db->isOpen()) {
        warningMs() << "Database is not opened!";
        rrr(kBadParameterError);
    }

    CsvWriter writer(csvFilePath, "\r\n", ',');
    bool csvFileIsOpened = writer.open();
    if (!csvFileIsOpened) {
        warningMs() << "Csv file %1 cannot be opened!";
        rrr(kFileOpenError);
    }

    bool ok = writer.writeRow(InsilicoPeptidesCsvFormatter::header());
    if (!ok) {
        warningMs() << "Failed to write CSV header to" << csvFilePath;
        rrr(kError);
    }

    QStringList emptyRow = InsilicoPeptidesCsvFormatter::header();
    std::generate(emptyRow.begin(), emptyRow.end(), []() { return QString(); });

    FinderFeaturesGroupNumberIteratorDao dao(d->db);
    e = dao.exec(); ree;
    int rowCounter = 0;
    const QString QUOTED_CELL = QStringLiteral("\"%1\"");
    const QString UNKNOWN_TEMPLATE = QStringLiteral("UNKNOWN_%1");
    const QString EMPTY_CELL;
    while (dao.hasNext()) {
        FinderFeaturesDaoEntry entry = dao.next();
        Q_ASSERT(!entry.isNull());
        
        QStringList row = emptyRow;
        
        row[InsilicoPeptidesCsvFormatter::EntryColumns::UnchargedMass]
            = QString::number(entry.unchargedMass);
        row[InsilicoPeptidesCsvFormatter::EntryColumns::ChargeList] = entry.chargeList;
        row[InsilicoPeptidesCsvFormatter::EntryColumns::StartTime]
            = QString::number(entry.startTime);
        row[InsilicoPeptidesCsvFormatter::EntryColumns::EndTime] = QString::number(entry.endTime);
        row[InsilicoPeptidesCsvFormatter::EntryColumns::ApexTime] = QString::number(entry.apexTime);
        row[InsilicoPeptidesCsvFormatter::EntryColumns::Intensity]
            = QString::number(entry.intensity);

        row[InsilicoPeptidesCsvFormatter::EntryColumns::Sequence]
            = entry.sequence.isEmpty() ? UNKNOWN_TEMPLATE.arg(rowCounter) : entry.sequence;

        row[InsilicoPeptidesCsvFormatter::EntryColumns::ModificationsNameList]
            = entry.modificationsIdList.isEmpty()
            ? EMPTY_CELL
            : QUOTED_CELL.arg(entry.modificationsIdList);

        row[InsilicoPeptidesCsvFormatter::EntryColumns::ModificationsPositionList]
            = entry.modificationsPositionList.isEmpty()
            ? EMPTY_CELL
            : QUOTED_CELL.arg(entry.modificationsPositionList);

        bool ok = writer.writeRow(row);
        if (!ok) {
            warningMs() << "Failed to write row" << rowCounter;
            rrr(kError);
        }

        rowCounter++;
    }

    return e;
}

FinderFeaturesDaoEntry
ClusterFinder::bestCandidate(int samplesId, const FinderFeaturesDaoEntry &parent,
                             const QVector<FinderFeaturesDaoEntry> &neighbors)
{
    FinderFeaturesDaoEntry result;

    // extract entries by samplesId
    FinderFeaturesDaoEntry searchItem = parent;
    searchItem.samplesId = samplesId;
    auto first = std::lower_bound(
        neighbors.begin(), neighbors.end(), searchItem,
        [](const FinderFeaturesDaoEntry &left, const FinderFeaturesDaoEntry &right) {
            return left.samplesId < right.samplesId;
        });

    double minUnchargedMassDiff = std::numeric_limits<double>::max();
    double minApexTimeDiff = std::numeric_limits<double>::max();
    double minIntensityDiff = std::numeric_limits<double>::max();

    const FinderFeaturesDaoEntry *currentEntry = nullptr;

    // find minimum distance now
    while (first != neighbors.end()) {
        if (first->samplesId != samplesId) {
            break;
        }

        double unchargeMassDiff = std::abs(first->unchargedMass - parent.unchargedMass);
        if (unchargeMassDiff <= minUnchargedMassDiff) {
            if (unchargeMassDiff < minUnchargedMassDiff) {
                // found candidate
                currentEntry = first;
                minUnchargedMassDiff = unchargeMassDiff;

            } else if (unchargeMassDiff == minUnchargedMassDiff) {

                double apexTimeDiff = std::abs(first->apexTime - parent.apexTime);
                if (apexTimeDiff <= minApexTimeDiff) {
                    // found candidate with better apex time
                    if (apexTimeDiff < minApexTimeDiff) {
                        currentEntry = first;
                        minApexTimeDiff = apexTimeDiff;
                    } else if (apexTimeDiff == minApexTimeDiff) {

                        double intensityDiff = std::abs(first->intensity - parent.intensity);
                        if (intensityDiff <= minIntensityDiff) {

                            // found candidate with better intensity
                            if (intensityDiff < minIntensityDiff) {
                                currentEntry = first;
                                minIntensityDiff = intensityDiff;
                            } else if (intensityDiff == minIntensityDiff) {
                                // well, we will take first that matches best than
                                warningMs() << "Ignoring close candidate!";
                            }
                        }
                    }
                }
            }
        }

        first++;
    }

    if (currentEntry) {
        result = *currentEntry;
    }

    return result;
}

_PMI_END
