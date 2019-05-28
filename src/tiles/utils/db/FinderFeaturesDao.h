/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef FINDER_FEATURES_DAO_H
#define FINDER_FEATURES_DAO_H

#include "pmi_common_ms_export.h"

#include <common_errors.h>
#include <pmi_core_defs.h>

#include <QMap>
#include <QSqlQuery>

class QSqlDatabase;

_PMI_BEGIN

struct FinderFeaturesIdDaoEntry {
    QString sequence;
    QString modificationsPositionList;
    QString modificationsIdList;
};

struct PMI_COMMON_MS_EXPORT FinderFeaturesDaoEntry {
    int id = -1;
    int samplesId = -1;
    double unchargedMass = 0.0;
    double startTime = 0.0;
    double endTime = 0.0;
    double apexTime = 0.0;
    double intensity = 0.0;
    int groupNumber = -1;
    QString chargeList;
    QString sequence;
    QString modificationsPositionList;
    QString modificationsIdList;

    bool isNull() {
        return id == -1;
    }

};

struct PMI_COMMON_MS_EXPORT NeighborSettings {
    double unchargedMassEpsilon = 0.01; // arbitrary chosen from particular cluster
    // apexTime is in minutes, here we want 10 second, arbitrary chosen from particular cluster
    double apexTimeEpsilon = 10.0 / 60.0; 
    double intensityEpsilon = 2e6; // arbitrary chosen from particular cluster
};

class PMI_COMMON_MS_EXPORT FinderFeaturesDao
{

public:
    explicit FinderFeaturesDao(QSqlDatabase *db);

    Err createTable() const;

    Err insertFeature(const FinderFeaturesDaoEntry &entry) const;

    Err updateGroupNumber(int id, int groupNumber) const;

    Err loadFeature(int id, FinderFeaturesDaoEntry *entry) const;

    Err loadNeighborFeatures(const FinderFeaturesDaoEntry &entry, const NeighborSettings &settings,
                             QVector<FinderFeaturesDaoEntry> *entries) const;

    Err firstItemId(int *id) const;
    Err lastItemId(int *id) const;

    Err updateGroupNumbers(const QVector<FinderFeaturesDaoEntry> &cluster, int groupNumber) const;
    
    Err updateGroupNumbers(const QMap<int, int> &idGroupNumbers) const;

    // for given ids updates identification (Sequence, modificationsPositionList, modificationsIdList)
    Err updateIdentificationInfo(const QMap<int, FinderFeaturesIdDaoEntry> &identification) const;

    Err ungroupedFeatureCount(int *count) const;
    
    Err resetGrouping() const;

    Err makeIndexes() const;

private:
    enum class ItemPosition { FIRST, LAST };
    Err itemId(ItemPosition pos, int *id) const;
private:
    QSqlDatabase *m_db;
};

class PMI_COMMON_MS_EXPORT FinderFeaturesGroupNumberIteratorDao 
{
public:
    explicit FinderFeaturesGroupNumberIteratorDao(QSqlDatabase *db);

    Err exec();
    bool hasNext() const;
    FinderFeaturesDaoEntry next();

private:
    QSqlDatabase *m_db;
    QSqlQuery m_query;
    bool m_hasNext = false;
};

_PMI_END

#endif // FINDER_FEATURES_DAO_H
