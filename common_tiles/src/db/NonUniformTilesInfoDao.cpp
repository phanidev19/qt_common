/*
* Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/


#include "NonUniformTilesInfoDao.h"
#include "QtSqlUtils.h"

#include "NonUniformTileRange.h"

_PMI_BEGIN

NonUniformTilesInfoDao::NonUniformTilesInfoDao(QSqlDatabase * db) :m_db(db)
{

}

Err NonUniformTilesInfoDao::createTable()
{
    QSqlQuery q = makeQuery(m_db, true);
    QString sql = R"(CREATE TABLE IF NOT EXISTS NonUniformTilesInfo (
                        Id INTEGER PRIMARY KEY, 
                        MzMin REAL, 
                        MzMax REAL, 
                        MzTileLength REAL, 
                        ScanIndexMin INT, 
                        ScanIndexMax INT, 
                        ScanIndexTileLength INT
                    );)";

    Err e = QEXEC_CMD(q, sql); ree;
    return e;
}

Err NonUniformTilesInfoDao::dropTable()
{
    QSqlQuery q = makeQuery(m_db, true);
    Err e = QEXEC_CMD(q, "DROP TABLE IF EXISTS NonUniformTilesInfo;"); ree;
    return e;
}

Err NonUniformTilesInfoDao::load(NonUniformTileRange * range)
{
    if (!range) {
        return kBadParameterError;
    }

    QSqlQuery q = makeQuery(m_db, true);
    Err e = QPREPARE(q, "SELECT MzMin, MzMax, MzTileLength, ScanIndexMin, ScanIndexMax, ScanIndexTileLength FROM NonUniformTilesInfo LIMIT 1;");

    if (!q.exec()) {
        qDebug() << "Error getting NonUniformTilesInfo" << q.lastError();
        return e;
    }

    if (!q.first()) {
        return kSQLiteMissingContentError; //TODO: really?
    }

    bool ok;
    double mzMin = q.value(0).toDouble(&ok); Q_ASSERT(ok);
    double mzMax = q.value(1).toDouble(&ok); Q_ASSERT(ok);
    double mzTileLength = q.value(2).toDouble(&ok); Q_ASSERT(ok);

    int scanNumberMin = q.value(3).toInt(&ok); Q_ASSERT(ok);
    int scanNumberMax = q.value(4).toInt(&ok); Q_ASSERT(ok);
    int scanIndexLength = q.value(5).toInt(&ok); Q_ASSERT(ok);

    range->setMz(mzMin, mzMax);
    range->setMzTileLength(mzTileLength);
    
    range->setScanIndex(scanNumberMin, scanNumberMax);
    range->setScanIndexLength(scanIndexLength);

    return kNoErr;
}

Err NonUniformTilesInfoDao::save(const NonUniformTileRange &range)
{
    {
        //TODO: we could use key-value table, or use this to ensure only one entry 
        QSqlQuery q = makeQuery(m_db, true);
        Err e = QEXEC_CMD(q, "DELETE FROM NonUniformTilesInfo;"); ree;
    }

    QSqlQuery q = makeQuery(m_db, true);
    Err e = QPREPARE(q, "INSERT INTO NonUniformTilesInfo(MzMin, MzMax, MzTileLength, ScanIndexMin, ScanIndexMax, ScanIndexTileLength) \
                                                VALUES(:MzMin, :MzMax, :MzTileLength, :ScanIndexMin, :ScanIndexMax, :ScanIndexTileLength);"); ree;

    q.bindValue(":MzMin", range.mzMin());
    q.bindValue(":MzMax", range.mzMax());
    q.bindValue(":MzTileLength", range.mzTileLength());

    q.bindValue(":ScanIndexMin", range.scanIndexMin());
    q.bindValue(":ScanIndexMax", range.scanIndexMax());
    q.bindValue(":ScanIndexTileLength", range.scanIndexTileLength());

    e = QEXEC_NOARG(q);
    return e;
}



_PMI_END


