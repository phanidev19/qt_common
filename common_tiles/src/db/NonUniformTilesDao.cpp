/*
* Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/


#include "NonUniformTilesDao.h"

#include <QSqlDatabase>
#include "QtSqlUtils.h"
#include "NonUniformTilesSerialization.h"

_PMI_BEGIN

const QString NonUniformTilesDaoEntry::CONTENT_MS1_RAW = QStringLiteral("ContentMS1Raw");
const QString NonUniformTilesDaoEntry::CONTENT_MS1_CENTROIDED = QStringLiteral("ContentMS1Centroided");

NonUniformTilesDao::NonUniformTilesDao(QSqlDatabase * db) : m_db(db)
{
}


Err NonUniformTilesDao::createTable()
{
    QSqlQuery q = makeQuery(m_db, true);
    
    QString sql = R"(CREATE TABLE IF NOT EXISTS NonUniformTiles (
                                PositionX INT,
                                PositionY INT,
                                Indexes BLOB,
                                Mz BLOB,
                                Intensity BLOB,
                                ContentType TEXT, 
                                PointCount INT,
                                DebugText TEXT,
                                PRIMARY KEY (PositionX, PositionY, ContentType)
    );)";

    Err e = QEXEC_CMD(q, sql); ree;
    return e;
}

Err NonUniformTilesDao::dropTable()
{
    Err e = kNoErr;
    QSqlQuery q = makeQuery(m_db, true);
    e = QEXEC_CMD(q, "DROP TABLE IF EXISTS NonUniformTiles;"); ree;
    return e;
}

Err NonUniformTilesDao::loadTile(const QPoint &position, const QString &contentType, NonUniformTilesDaoEntry * entry)
{
    if (!entry) {
        return kBadParameterError;
    }

    QSqlQuery q = makeQuery(m_db, true);
    QString sql = R"(SELECT PositionX, PositionY, Indexes, Mz, Intensity, ContentType, PointCount, DebugText 
                            FROM NonUniformTiles 
                            WHERE (PositionX = ?) AND (PositionY = ?) AND (ContentType = ?);)";

    Err e = QPREPARE(q, sql);
    q.bindValue(0, position.x());
    q.bindValue(1, position.y());
    q.bindValue(2, contentType);

    if (!q.exec()) {
        qDebug() << "Error getting NonUniform tile at:" << position << q.lastError();
        return e;
    }

    if (!q.first()) {
        return kSQLiteMissingContentError; //TODO: really?
    }

    bool ok;
    entry->posX = q.value(0).toInt(&ok); Q_ASSERT(ok);
    entry->posY = q.value(1).toInt(&ok); Q_ASSERT(ok);
    
    entry->pointCount = q.value(6).toInt(&ok); Q_ASSERT(ok);
    entry->index = NonUniformTilesSerialization::deserializeIndex(q.value(2).toByteArray(), entry->pointCount);
    entry->mz = NonUniformTilesSerialization::deserializeMz(q.value(3).toByteArray(), entry->pointCount);
    entry->intensity = NonUniformTilesSerialization::deserializeIntensity(q.value(4).toByteArray(), entry->pointCount);
    entry->contentType = q.value(5).toString();
    entry->debugText = q.value(7).toString();

    return kNoErr;
}

Err NonUniformTilesDao::saveTile(const NonUniformTilesDaoEntry &entry)
{
    QSqlQuery q = makeQuery(m_db, true);
    static QString sql = R"(INSERT INTO NonUniformTiles(PositionX, PositionY, Indexes, Mz, Intensity, ContentType, PointCount, DebugText) 
                                         VALUES (:PositionX, :PositionY, :Indexes, :Mz, :Intensity, :ContentType, :PointCount, :DebugText);)";

    Err e = QPREPARE(q, sql); ree;

    q.bindValue(":PositionX", entry.posX);
    q.bindValue(":PositionY", entry.posY);
    q.bindValue(":Indexes", NonUniformTilesSerialization::serializeIndex(entry.index));
    q.bindValue(":Mz", NonUniformTilesSerialization::serializeMz(entry.mz));
    q.bindValue(":Intensity", NonUniformTilesSerialization::serializeIntensity(entry.intensity));
    q.bindValue(":ContentType", entry.contentType);
    q.bindValue(":PointCount", entry.pointCount);
    q.bindValue(":DebugText", entry.debugText);

    return QEXEC_NOARG(q);
}

Err NonUniformTilesDao::count(const QString &contentType, int * count)
{
    if (!count) {
        return kBadParameterError;
    }

    QSqlQuery q = makeQuery(m_db, true);
    QString sql = QStringLiteral("SELECT COUNT(*) FROM NonUniformTiles WHERE ContentType = ?;");
    Err e = QPREPARE(q, sql); ree;
    
    q.bindValue(0, contentType);
    if (!q.exec()) {
        qDebug() << "Error quering NonUniform tile with content type:" << contentType << q.lastError();
        return e;
    }

    if (!q.first())
    {
        return kSQLiteMissingContentError;
    }

    bool ok;
    *count = q.value(0).toInt(&ok); Q_ASSERT(ok);
    return e;
}


Err NonUniformTilesDao::pointCount(const QString &contentType, const QRect &tileRect, quint32 *count)
{
    if (!count) {
        return kBadParameterError;
    }

    QString rectCondition;
    if (!tileRect.isNull()) {
        rectCondition = QStringLiteral(R"(AND 
            PositionX >= ? AND PositionY >= ? AND
            PositionX <= ? AND PositionY <= ?)");
    }

    QSqlQuery q = makeQuery(m_db, true);
    QString sql = QStringLiteral(
        R"(SELECT SUM(PointCount) FROM NonUniformTiles 
                                  WHERE 
                                        ContentType = ? %1;)");
    sql = sql.arg(rectCondition);

    Err e = QPREPARE(q, sql); ree;

    q.bindValue(0, contentType);
    if (!tileRect.isNull()) {
        q.bindValue(1, tileRect.left());
        q.bindValue(2, tileRect.top());
        q.bindValue(3, tileRect.right());
        q.bindValue(4, tileRect.bottom());
    }

    if (!q.exec()) {
        qDebug() << "Error quering NonUniform tile with content type:" << contentType
            << q.lastError();
        return e;
    }

    if (!q.first()) {
        return kSQLiteMissingContentError;
    }

    bool ok;
    *count = q.value(0).toUInt(&ok);
    Q_ASSERT(ok);
    return e;
}

bool NonUniformTilesDao::contains(const QPoint &position, const QString &contentType)
{
    QSqlQuery q = makeQuery(m_db, true);
    QString sql = R"(SELECT PositionX, PositionY, ContentType FROM NonUniformTiles 
                            WHERE (PositionX = ?) AND (PositionY = ?) AND (ContentType = ?);)";

    Err e = QPREPARE(q, sql);
    if (e != kNoErr) {
        return false;
    }
    
    q.bindValue(0, position.x());
    q.bindValue(1, position.y());
    q.bindValue(2, contentType);

    if (!q.exec()) {
        qDebug() << "Error quering NonUniform tile at:" << position << q.lastError();
        return e;
    }

    bool hasFirst = q.first();
    if (hasFirst) {
        Q_ASSERT(q.value(0).toInt() == position.x());
        Q_ASSERT(q.value(1).toInt() == position.y());
        Q_ASSERT(q.value(2).toString() == contentType);
    }

    return hasFirst;
}

bool NonUniformTilesDao::isSchemaValid() const
{
    QSqlRecord record = m_db->record("NonUniformTiles");
    if (record.isEmpty()) {
        return false;
    }

    return record.contains("PositionX") &&
           record.contains("PositionY") &&
           record.contains("Indexes") &&
           record.contains("Mz") &&
           record.contains("Intensity") &&
           record.contains("ContentType") &&
           record.contains("PointCount") &&
           record.contains("DebugText");
}

_PMI_END
