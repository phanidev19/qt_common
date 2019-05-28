/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/


#include "NonUniformTilesPartDao.h"

#include <QSqlDatabase>
#include "QtSqlUtils.h"
#include "NonUniformTilesSerialization.h"

_PMI_BEGIN

NonUniformTilesPartDao::NonUniformTilesPartDao(QSqlDatabase * db) : m_db(db)
{

}

Err NonUniformTilesPartDao::createTable()
{
    QSqlQuery q = makeQuery(m_db, true);

    QString sql = R"(CREATE TABLE IF NOT EXISTS NonUniformTilesPart (
                                NonUniformTilesPartInfoId INT,
                                WriteOrder INT,
                                Indexes BLOB,
                                Mz BLOB,
                                Intensity BLOB,
                                PointCount INT,
                                ScanCount INT,
                                PRIMARY KEY (NonUniformTilesPartInfoId, WriteOrder),
                                FOREIGN KEY(NonUniformTilesPartInfoId) REFERENCES NonUniformTilesPartInfo(Id)
    );)";

    Err e = QEXEC_CMD(q, sql); ree;
    return e;
}

Err NonUniformTilesPartDao::dropTable()
{
    Err e = kNoErr;
    QSqlQuery q = makeQuery(m_db, true);
    e = QEXEC_CMD(q, "DROP TABLE IF EXISTS NonUniformTilesPart;"); ree;
    e = QEXEC_CMD(q, "VACUUM NonUniformTilesPart;"); ree;
    return e;
}

Err NonUniformTilesPartDao::saveTilePart(const NonUniformTilesPartEntry &entry)
{
    QSqlQuery q = makeQuery(m_db, true);
    static QString sql = R"(INSERT INTO NonUniformTilesPart(
                                NonUniformTilesPartInfoId, 
                                WriteOrder, 
                                Indexes, 
                                Mz, 
                                Intensity, 
                                PointCount, 
                                ScanCount
                            ) 
                             VALUES (
                               :NonUniformTilesPartInfoId,
                               :WriteOrder, 
                               :Indexes, 
                               :Mz, 
                               :Intensity, 
                               :PointCount, 
                               :ScanCount);)";

    Err e = QPREPARE(q, sql); ree;

    q.bindValue(":NonUniformTilesPartInfoId", entry.nonUniformTilesPartInfoId);
    q.bindValue(":WriteOrder", entry.id);
    q.bindValue(":Indexes", NonUniformTilesSerialization::serializeIndex(entry.index));
    q.bindValue(":Mz", NonUniformTilesSerialization::serializeMz(entry.mz));
    q.bindValue(":Intensity", NonUniformTilesSerialization::serializeIntensity(entry.intensity));
    q.bindValue(":PointCount", entry.pointCount);
    q.bindValue(":ScanCount", entry.scanCount);

    return QEXEC_NOARG(q);
}

Err NonUniformTilesPartDao::loadTileParts(qulonglong nonUniformTilesPartInfoId, QVector<NonUniformTilesPartEntry> * entries)
{
    if (!entries) {
        return kBadParameterError;
    }

    entries->clear();

    QSqlQuery q = makeQuery(m_db, true);

    QString sql = R"(SELECT WriteOrder, 
                            Indexes, 
                            Mz, 
                            Intensity, 
                            PointCount, 
                            ScanCount 
                       FROM 
                            NonUniformTilesPart
                       WHERE 
                            (NonUniformTilesPartInfoId = ?) 
                       ORDER BY 
                            WriteOrder ASC;)";

    Err e = QPREPARE(q, sql); ree;
    
    q.bindValue(0, nonUniformTilesPartInfoId);

    if (!q.exec()) {
        qDebug() << "Error getting NonUniformTilesPart parts for" << nonUniformTilesPartInfoId << q.lastError();
        return e;
    }

    while (q.next()) {
        NonUniformTilesPartEntry entry;

        entry.nonUniformTilesPartInfoId = nonUniformTilesPartInfoId;
        
        bool ok;
        entry.id = q.value(0).toULongLong(&ok); Q_ASSERT(ok);
        entry.pointCount = q.value(4).toInt(&ok); Q_ASSERT(ok);
        entry.scanCount = q.value(5).toInt(&ok); Q_ASSERT(ok);
        entry.index = NonUniformTilesSerialization::deserializeIndex(q.value(1).toByteArray(), entry.scanCount);
        entry.mz = NonUniformTilesSerialization::deserializeMz(q.value(2).toByteArray(), entry.pointCount);
        entry.intensity = NonUniformTilesSerialization::deserializeIntensity(q.value(3).toByteArray(), entry.pointCount);
        entry.scanCount;

        entries->push_back(entry);
    }

    return kNoErr;
}


NonUniformTilesPartEntry &NonUniformTilesPartEntry::append(const NonUniformTilesPartEntry &rhs)
{
    if (rhs.nonUniformTilesPartInfoId == nonUniformTilesPartInfoId) {
        //QVector<int> index;
        index.append(rhs.index);
        mz.append(rhs.mz);
        intensity.append(rhs.intensity);
        scanCount += rhs.scanCount;
        pointCount += rhs.pointCount;

        Q_ASSERT(index.size() == scanCount);
        Q_ASSERT(pointCount == mz.size());
        Q_ASSERT(pointCount == intensity.size());

    } else {
        qWarning() << "Appending to wrong tile part!";
    }


    return *this;
}

_PMI_END


