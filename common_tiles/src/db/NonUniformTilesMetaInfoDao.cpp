/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/


#include "NonUniformTilesMetaInfoDao.h"

#include "QtSqlUtils.h"

_PMI_BEGIN

NonUniformTilesMetaInfoDao::NonUniformTilesMetaInfoDao(QSqlDatabase * db) :m_db(db)
{
}

Err NonUniformTilesMetaInfoDao::createTable()
{
    QSqlQuery q = makeQuery(m_db, true);

    QString sql = R"(CREATE TABLE IF NOT EXISTS NonUniformTilesPartInfo (
                                Id INTEGER PRIMARY KEY ASC,
                                PositionX INT,
                                PositionY INT,
                                ContentType TEXT
    );)";

    Err e = QEXEC_CMD(q, sql);
    //TODO create index on PositionX = ?) AND (PositionY = ?) AND (ContentType = ?
    e = QEXEC_CMD(q, "CREATE UNIQUE INDEX IF NOT EXISTS index_PositionContentType ON NonUniformTilesPartInfo(PositionX, PositionY, ContentType);"); ree;

    return e;
}


Err NonUniformTilesMetaInfoDao::dropTable()
{
    Err e = kNoErr;
    QSqlQuery q = makeQuery(m_db, true);
    e = QEXEC_CMD(q, "DROP TABLE IF EXISTS NonUniformTilesPartInfo;"); ree;
    e = QEXEC_CMD(q, "VACUUM NonUniformTilesPartInfo;"); ree;
    return e;
}

Err NonUniformTilesMetaInfoDao::saveTileInfo(const QPoint &tilePos, const QString &contentType)
{
    QSqlQuery q = makeQuery(m_db, true);
    static QString sql = R"(INSERT INTO NonUniformTilesPartInfo(PositionX, PositionY, ContentType) 
                                         VALUES (:PositionX, :PositionY, :ContentType);)";
    Err e = QPREPARE(q, sql); ree;

    q.bindValue(":PositionX", tilePos.x());
    q.bindValue(":PositionY", tilePos.y());
    q.bindValue(":ContentType", contentType);

    return QEXEC_NOARG(q);
}


Err NonUniformTilesMetaInfoDao::tileInfoId(const QPoint &position, const QString &contentType, qulonglong * tileId) const
{
    if (!tileId) {
        return kBadParameterError;
    }

    Q_ASSERT(!contentType.isEmpty());

    QSqlQuery q = makeQuery(m_db, true);
    QString sql = R"(SELECT Id FROM NonUniformTilesPartInfo 
                            WHERE (PositionX = ?) AND (PositionY = ?) AND (ContentType = ?);)";


    Err e = QPREPARE(q, sql); ree;
    
    q.bindValue(0, position.x());
    q.bindValue(1, position.y());
    q.bindValue(2, contentType);

    if (!q.exec()) {
        qDebug() << "Error getting Id from tile at:" << position << q.lastError();
        return e;
    }

    if (!q.first()) {
        return kSQLiteMissingContentError; 
    }
    
    *tileId = q.value(0).toULongLong();

    return kNoErr;
}


Err NonUniformTilesMetaInfoDao::allTileInfo(QVector<NonUniformTilesMetaInfoEntry> * all)
{
    QSqlQuery q = makeQuery(m_db, true);

    QString sql = R"(SELECT Id, PositionX, PositionY, ContentType FROM NonUniformTilesPartInfo;)";

    Err e = QPREPARE(q, sql); ree;

    if (!q.exec()) {
        qDebug() << "Error getting all part tiles" << q.lastError();
        return e;
    }

    if (q.size() == 0) {
        return kSQLiteMissingContentError;
    }

    all->clear();
    all->reserve(q.size());
    while (q.next()) {
        bool ok;
        
        NonUniformTilesMetaInfoEntry entry;
        
        entry.id = q.value(0).toULongLong(&ok); Q_ASSERT(ok);
        entry.position.rx() = q.value(1).toInt(&ok);  Q_ASSERT(ok);
        entry.position.ry() = q.value(2).toInt(&ok);  Q_ASSERT(ok);
        entry.contentType = q.value(3).toString(); Q_ASSERT(!entry.contentType.isEmpty());
        all->push_back(entry);
    }

    return e;
}


_PMI_END


