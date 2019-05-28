/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "FinderFeaturesDao.h"

#include "pmi_common_ms_debug.h"

// common_stable
#include <QtSqlUtils.h>

// Qt
#include <QSqlQuery>

_PMI_BEGIN

FinderFeaturesDao::FinderFeaturesDao(QSqlDatabase *db)
    : m_db(db)
{
}

Err FinderFeaturesDao::createTable() const 
{
    Err e = kNoErr;

    QSqlQuery q = makeQuery(m_db, true);
    const QString createSql
        = QString(R"(CREATE TABLE IF NOT EXISTS FinderFeatures(
                    Id INTEGER PRIMARY KEY
                    ,SamplesId INT
                    ,UnchargedMass REAL
                    ,StartTime REAL
                    ,EndTime REAL
                    ,ApexTime REAL
                    ,Intensity REAL
                    ,GroupNumber INT
                    ,ChargeList TEXT
                    ,Sequence TEXT
                    ,ModificationsPositionList TEXT
                    ,ModificationsIdList TEXT
                    ))");

    e = QEXEC_CMD(q, createSql); ree;

    return e;
}

Err FinderFeaturesDao::insertFeature(const FinderFeaturesDaoEntry &entry) const
{
    Err e = kNoErr;
    QSqlQuery q = makeQuery(m_db, true);

    e = QPREPARE(q,
                 R"(INSERT INTO FinderFeatures(
                                               SamplesId
                                              ,UnchargedMass
                                              ,StartTime
                                              ,EndTime
                                              ,ApexTime
                                              ,Intensity
                                              ,GroupNumber
                                              ,ChargeList
                                              ,Sequence
                                              ,ModificationsPositionList
                                              ,ModificationsIdList
                              ) VALUES(
                                              :SamplesId
                                             ,:UnchargedMass
                                             ,:StartTime
                                             ,:EndTime
                                             ,:ApexTime
                                             ,:Intensity
                                             ,:GroupNumber
                                             ,:ChargeList
                                             ,:Sequence
                                             ,:ModificationsPositionList
                                             ,:ModificationsIdList 
                                       ))"); ree;

    q.bindValue(":SamplesId", entry.samplesId);
    q.bindValue(":UnchargedMass", entry.unchargedMass);
    q.bindValue(":StartTime", entry.startTime);
    q.bindValue(":EndTime", entry.endTime);
    q.bindValue(":ApexTime", entry.apexTime);
    q.bindValue(":Intensity", entry.intensity);
    q.bindValue(":GroupNumber", entry.groupNumber);
    q.bindValue(":ChargeList", entry.chargeList);
    q.bindValue(":Sequence", entry.sequence);
    q.bindValue(":ModificationsPositionList", entry.modificationsPositionList);
    q.bindValue(":ModificationsIdList", entry.modificationsIdList);

    e = QEXEC_NOARG(q); ree;

    return e;
}

Err FinderFeaturesDao::updateGroupNumber(int id, int groupNumber) const
{
    Err e = kNoErr;
    QSqlQuery q = makeQuery(m_db, true);

    const QString updateSql
        = QString("UPDATE FinderFeatures SET GroupNumber = :GroupNumber WHERE Id = :Id");
    e = QPREPARE(q, updateSql); ree;

    q.bindValue(":GroupNumber", groupNumber);
    q.bindValue(":Id", id);

    e = QEXEC_NOARG(q); ree;

    return e;
}

Err FinderFeaturesDao::loadFeature(int id, FinderFeaturesDaoEntry *entry) const
{
    Err e = kNoErr;
    QSqlQuery q = makeQuery(m_db, true);
    e = QPREPARE(
        q,
        QString(R"(
                    SELECT 
                        Id
                       ,SamplesId
                       ,UnchargedMass
                       ,StartTime
                       ,EndTime
                       ,ApexTime
                       ,Intensity
                       ,GroupNumber
                       ,ChargeList
                       ,Sequence
                       ,ModificationsPositionList
                       ,ModificationsIdList 
                    FROM FinderFeatures WHERE Id = :Id)")); ree;

    q.bindValue(":Id", id);

    e = QEXEC_NOARG(q); ree;

    FinderFeaturesDaoEntry result;
    if (q.next()) {
        result.id = id;
        bool ok;
        result.samplesId = q.value(1).toInt(&ok);
        Q_ASSERT(ok);
        result.unchargedMass = q.value(2).toDouble(&ok);
        Q_ASSERT(ok);
        result.startTime = q.value(3).toDouble(&ok);
        Q_ASSERT(ok);
        result.endTime = q.value(4).toDouble(&ok);
        Q_ASSERT(ok);
        result.apexTime = q.value(5).toDouble(&ok);
        Q_ASSERT(ok);
        result.intensity = q.value(6).toDouble(&ok);
        Q_ASSERT(ok);
        result.groupNumber = q.value(7).toInt(&ok);
        Q_ASSERT(ok);
        result.chargeList = q.value(8).toString();
        result.sequence = q.value(9).toString();
        result.modificationsPositionList = q.value(10).toString();
        result.modificationsIdList = q.value(11).toString();
    } else {
        warningMs() << "Cannot find id" << id;
    }

    *entry = result;

    return e;
}

Err FinderFeaturesDao::loadNeighborFeatures(const FinderFeaturesDaoEntry &entry,
                                            const NeighborSettings &settings,
                                            QVector<FinderFeaturesDaoEntry> *entries) const
{
    Q_ASSERT(entries);

    Err e = kNoErr;
    QSqlQuery q = makeQuery(m_db, true);
    QString sql(R"(SELECT   Id, SamplesId, 
                            UnchargedMass, StartTime, EndTime, 
                            ApexTime, Intensity, GroupNumber, ChargeList 
                    FROM FinderFeatures 
                    WHERE   UnchargedMass < (:UnchargedMass + :UnchargedMassEpsilon) AND UnchargedMass > (:UnchargedMass - :UnchargedMassEpsilon) AND 
                            ApexTime < (:ApexTime + :ApexTimeEpsilon) AND ApexTime > (:ApexTime - :ApexTimeEpsilon) AND 
                            Intensity < (:Intensity + :IntensityEpsilon) AND Intensity > (:Intensity - :IntensityEpsilon) AND 
                            SamplesId != :SamplesId AND 
                            GroupNumber == -1
                    ORDER BY SamplesId ASC)");

    e = QPREPARE(q, sql); ree;

    q.bindValue(":UnchargedMass", entry.unchargedMass);
    q.bindValue(":UnchargedMassEpsilon", settings.unchargedMassEpsilon);

    q.bindValue(":ApexTime", entry.apexTime);
    q.bindValue(":ApexTimeEpsilon", settings.apexTimeEpsilon);

    q.bindValue(":Intensity", entry.intensity);
    q.bindValue(":IntensityEpsilon", settings.intensityEpsilon);

    q.bindValue(":SamplesId", entry.samplesId);

    e = QEXEC_NOARG(q); ree;

    while (q.next()) {
        FinderFeaturesDaoEntry result;

        bool ok;
        result.id = q.value(0).toInt(&ok);
        Q_ASSERT(ok);
        result.samplesId = q.value(1).toInt(&ok);
        Q_ASSERT(ok);
        result.unchargedMass = q.value(2).toDouble(&ok);
        Q_ASSERT(ok);
        result.startTime = q.value(3).toDouble(&ok);
        Q_ASSERT(ok);
        result.endTime = q.value(4).toDouble(&ok);
        Q_ASSERT(ok);
        result.apexTime = q.value(5).toDouble(&ok);
        Q_ASSERT(ok);
        result.intensity = q.value(6).toDouble(&ok);
        Q_ASSERT(ok);
        result.groupNumber = q.value(7).toInt(&ok);
        Q_ASSERT(ok);

        entries->push_back(result);
    }

    return e;
}

Err FinderFeaturesDao::firstItemId(int *id) const
{
    return itemId(ItemPosition::FIRST, id);
}

Err FinderFeaturesDao::lastItemId(int *id) const
{
    return itemId(ItemPosition::LAST, id);
}

Err FinderFeaturesDao::updateGroupNumbers(const QVector<FinderFeaturesDaoEntry> &cluster,
                                          int groupNumber) const
{
    Err e = kNoErr;

    QString sql = QString(R"(UPDATE FinderFeatures
        SET GroupNumber = :GroupNumber
        WHERE Id = :Id)");

    QSqlQuery q = makeQuery(m_db, true);
    e = QPREPARE(q, sql); ree;

    for (const FinderFeaturesDaoEntry &item : cluster) {
        q.bindValue(":Id", item.id);
        q.bindValue(":GroupNumber", groupNumber);
        e = QEXEC_NOARG(q); ree;
    }

    return e;
}

Err FinderFeaturesDao::updateGroupNumbers(const QMap<int, int> &idGroupNumbers) const
{
    Err e = kNoErr;

    QString sql = QString(R"(UPDATE FinderFeatures
        SET GroupNumber = :GroupNumber
        WHERE Id = :Id)");

    QSqlQuery q = makeQuery(m_db, true);
    e = QPREPARE(q, sql); ree;

    bool ok = m_db->transaction();
    if (!ok) {
        return kError;
    }

    auto it = idGroupNumbers.constBegin();
    while (it != idGroupNumbers.constEnd()) {
        q.bindValue(":Id", it.key());
        q.bindValue(":GroupNumber", it.value());
        e = QEXEC_NOARG(q); ree;
        ++it;
    }

    ok = m_db->commit();
    if (!ok) {
        return kError;
    }

    return e;
}

Err FinderFeaturesDao::updateIdentificationInfo(const QMap<int, FinderFeaturesIdDaoEntry> &identification) const
{
    Err e = kNoErr;

    QString sql = QString(R"(UPDATE FinderFeatures
        SET Sequence = :Sequence, 
            ModificationsIdList = :ModificationsIdList, 
            ModificationsPositionList = :ModificationsPositionList
        WHERE Id = :Id)");

    QSqlQuery q = makeQuery(m_db, true);
    e = QPREPARE(q, sql); ree;
    
    auto it = identification.constBegin();
    while (it != identification.constEnd()) {
        const FinderFeaturesIdDaoEntry &idDao = it.value();
        q.bindValue(":Id", it.key());
        q.bindValue(":Sequence", idDao.sequence);
        q.bindValue(":ModificationsIdList", idDao.modificationsIdList);
        q.bindValue(":ModificationsPositionList", idDao.modificationsPositionList);

        e = QEXEC_NOARG(q); ree;
        ++it;
    }

    return e;
}

Err FinderFeaturesDao::ungroupedFeatureCount(int *count) const
{
    Q_ASSERT(count);
    Err e = kNoErr;

    QSqlQuery q = makeQuery(m_db, true);
    QString sql(
        "SELECT COUNT(*) as NonGroupedFeatures FROM FinderFeatures WHERE GroupNumber == -1");
    e = QPREPARE(q, sql); ree;
    e = QEXEC_NOARG(q); ree;

    if (!q.next()) {
        rrr(kError);
    }

    bool ok;
    *count = q.value(0).toInt(&ok);
    Q_ASSERT(ok);

    return e;
}

Err FinderFeaturesDao::resetGrouping() const
{
    Err e = kNoErr;
    QSqlQuery q = makeQuery(m_db, true);
    e = QEXEC_CMD(q, "UPDATE FinderFeatures SET GroupNumber = -1"); ree;
    return e;
}

Err FinderFeaturesDao::makeIndexes() const
{
    Err e = kNoErr;

    QSqlQuery q = makeQuery(m_db, true);
    e = QEXEC_CMD(q,
                  "CREATE INDEX IF NOT EXISTS index_FinderFeaturesUnchargedMass ON "
                  "FinderFeatures(UnchargedMass)"); ree;
    e = QEXEC_CMD(q, "CREATE INDEX IF NOT EXISTS index_FinderFeaturesApexTime ON FinderFeatures(ApexTime)"); ree;
    e = QEXEC_CMD(q, "CREATE INDEX IF NOT EXISTS index_FinderFeaturesIntensity ON FinderFeatures(Intensity)"); ree;
    e = QEXEC_CMD(q, "CREATE INDEX IF NOT EXISTS index_FinderFeaturesSamplesId ON FinderFeatures(SamplesId)"); ree;
    e = QEXEC_CMD(q, "CREATE INDEX IF NOT EXISTS index_FinderFeaturesGroupNumber ON "
                  "FinderFeatures(GroupNumber)"); ree;
    return e;
}

Err FinderFeaturesDao::itemId(ItemPosition pos, int *id) const
{
    Err e = kNoErr;
    QSqlQuery q = makeQuery(m_db, true);

    const QString op = (pos == ItemPosition::FIRST) ? QStringLiteral("MIN") : QStringLiteral("MAX");

    e = QPREPARE(q, QString("SELECT %1(Id) FROM FinderFeatures").arg(op)); ree;
    e = QEXEC_NOARG(q); ree;

    if (q.next()) {
        bool ok;
        *id = q.value(0).toInt(&ok);
        e = ok ? kNoErr : kBadParameterError; ree;
    } else {
        *id = -1;
    }

    return e;
}

FinderFeaturesGroupNumberIteratorDao::FinderFeaturesGroupNumberIteratorDao(QSqlDatabase *db)
    : m_db(db)
{
}

Err FinderFeaturesGroupNumberIteratorDao::exec()
{
    Err e = kNoErr;
    m_query = makeQuery(m_db, true);

    e = QPREPARE(
        m_query,
        QString("SELECT Id, SamplesId, UnchargedMass, StartTime, EndTime, ApexTime, Intensity, "
                "GroupNumber, ChargeList,Sequence, ModificationsPositionList, ModificationsIdList FROM FinderFeatures GROUP BY GroupNumber")); ree;

    e = QEXEC_NOARG(m_query); ree;

    m_hasNext = m_query.next();
    
    return e;
}

bool FinderFeaturesGroupNumberIteratorDao::hasNext() const
{
    return m_hasNext;
}

FinderFeaturesDaoEntry FinderFeaturesGroupNumberIteratorDao::next()
{
    FinderFeaturesDaoEntry result;
    if (!m_query.isValid()) {
        return result;
    }

    bool ok;
    result.id = m_query.value(0).toInt(&ok);
    Q_ASSERT(ok);
    result.samplesId = m_query.value(1).toInt(&ok);
    Q_ASSERT(ok);
    result.unchargedMass = m_query.value(2).toDouble(&ok);
    Q_ASSERT(ok);
    result.startTime = m_query.value(3).toDouble(&ok);
    Q_ASSERT(ok);
    result.endTime = m_query.value(4).toDouble(&ok);
    Q_ASSERT(ok);
    result.apexTime = m_query.value(5).toDouble(&ok);
    Q_ASSERT(ok);
    result.intensity = m_query.value(6).toDouble(&ok);
    Q_ASSERT(ok);
    result.groupNumber = m_query.value(7).toInt(&ok);
    Q_ASSERT(ok);
    result.chargeList = m_query.value(8).toString();
    result.sequence = m_query.value(9).toString();
    result.modificationsPositionList = m_query.value(10).toString();
    result.modificationsIdList = m_query.value(11).toString();

    m_hasNext = m_query.next();

    return result;
}

_PMI_END
