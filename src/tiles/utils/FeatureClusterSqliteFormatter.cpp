/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "FeatureClusterSqliteFormatter.h"

// common_ms
#include "FinderFeaturesDao.h"
#include "FinderSamplesDao.h"
#include "NonUniformFeature.h"
#include "NonUniformFeatureFindingSession.h"
#include "ScanIndexNumberConverter.h"
#include "pmi_common_ms_debug.h"

// common_tiles
#include "NonUniformTileDevice.h"
#include "NonUniformTileRange.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QString>

_PMI_BEGIN

const int INVALID_SAMPLE_ID = -1;

class Q_DECL_HIDDEN FeatureClusterSqliteFormatter::Private
{
public:
    Private(QSqlDatabase *database, NonUniformFeatureFindingSession *ses)
        : db(database)
        , session(ses)
    {
    }

    QSqlDatabase *db;
    NonUniformFeatureFindingSession *session;
    FinderFeaturesDaoEntry entry;
    QString sampleName;
    int samplesId = INVALID_SAMPLE_ID;
    bool transactionStarted = false;
};

FeatureClusterSqliteFormatter::FeatureClusterSqliteFormatter(
    QSqlDatabase *db, NonUniformFeatureFindingSession *session, QObject *parent)
    : QObject(parent)
    , d(new Private(db, session))
{
}

FeatureClusterSqliteFormatter::~FeatureClusterSqliteFormatter()
{
    if (d->transactionStarted) {
        end();
    }
}

Err FeatureClusterSqliteFormatter::init()
{
    Err e = kNoErr;
    FinderSamplesDao samples(d->db);
    FinderFeaturesDao features(d->db);

    e = samples.createTable(); ree;
    e = features.createTable(); ree;

    return e;
}

Err FeatureClusterSqliteFormatter::writeSamples(const QStringList &sampleNames) const
{
    Err e = kNoErr;
    
    FinderSamplesDao dao(d->db);
    e = dao.insertSamples(sampleNames); ree;

    return e;
}

void FeatureClusterSqliteFormatter::setCurrentSample(const QString &sampleName)
{
    d->sampleName = sampleName;
    
    FinderSamplesDao dao(d->db);
    Err e = dao.samplesId(sampleName, &d->samplesId);
    if (e != kNoErr) {
        d->samplesId = INVALID_SAMPLE_ID;
    }

    if (d->transactionStarted) {
        end();
    } 
    
    begin();
}

Err FeatureClusterSqliteFormatter::begin()
{
    Err e = kNoErr;
    
    bool ok = d->db->transaction();
    if (!ok) {
        warningMs() << "Failed to start transaction" << d->db->lastError().text();
        rrr(kError);
    }

    d->transactionStarted = true;

    return e; 
}

Err FeatureClusterSqliteFormatter::end()
{
    Err e = kNoErr;
    bool ok = d->db->commit();
    if (!ok) {
        warningMs()  << "Failed to commit data!" << d->db->lastError().text();
        rrr(kError);
    }

    d->transactionStarted = false;

    return e;
}

void FeatureClusterSqliteFormatter::acceptFeature(const NonUniformFeature &feature)
{
    FinderFeaturesDaoEntry newEntry;

    newEntry.samplesId = d->samplesId;
    newEntry.unchargedMass = feature.unchargedMass();

    const ScanIndexNumberConverter &converter = d->session->converter();
    NonUniformTileRange range = d->session->device()->range();

    ScanIndexInterval minMaxScanIndex = feature.minMaxScanIndex();
    double timeStart = converter.scanIndexToScanTime(minMaxScanIndex.start());
    double timeEnd
        = converter.scanIndexToScanTime(std::min(minMaxScanIndex.end(), range.scanIndexMax()));

    newEntry.startTime = timeStart;
    newEntry.endTime = timeEnd;
    newEntry.apexTime = feature.apexTime();
    newEntry.intensity = feature.maximumIntensity();
    newEntry.chargeList = feature.chargeList();
    
    d->entry = newEntry;

    emit rowFormatted();
}

Err FeatureClusterSqliteFormatter::saveFeature() const
{
    Err e = kNoErr;
    
    if (d->samplesId == INVALID_SAMPLE_ID) {
        warningMs() << "Select some sample first!";
        rrr(kBadParameterError);
    }

    FinderFeaturesDao dao(d->db);
    e = dao.insertFeature(d->entry); ree;

    return e;
}

Err FeatureClusterSqliteFormatter::flush()
{
    Err e = kNoErr;
    if (d->transactionStarted) {
        e = end(); ree;
    }
    return e;
}

_PMI_END


