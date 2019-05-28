/*
* Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/


#include "NonUniformTileStoreSqlite.h"

#include "pmi_common_tiles_debug.h"

#include "db\NonUniformTilesDao.h"
#include "db\NonUniformTilesMetaInfoDao.h"
#include "db\NonUniformTilesPartDao.h"

#include "QtSqlUtils.h"

#include <QScopedPointer>
#include <QSharedPointer>
#include <QSqlDatabase>
#include <QUuid>

_PMI_BEGIN

NonUniformTileStoreSqlite::NonUniformTileStoreSqlite(QSqlDatabase *db, bool ownDb)
    : m_db(db)
    , m_ownDb(ownDb)
{

}

NonUniformTileStoreSqlite::~NonUniformTileStoreSqlite()
{
    if (m_ownDb && m_db) {
        delete m_db;
    }
}

const NonUniformTile NonUniformTileStoreSqlite::loadTile(const QPoint &pos, ContentType type)
{
    NonUniformTile result;

    NonUniformTilesDao dao(m_db);
    NonUniformTilesDaoEntry entry;
    
    if (dao.loadTile(pos, contentTypeToDaoString(type), &entry) != kNoErr) {
        //return null tile
        return result;
    }

    result.setPosition(QPoint(entry.posX, entry.posY));
    deserializePartTile(entry.index, entry.mz, entry.intensity, entry.pointCount, &result);
    
    return result;
}


bool NonUniformTileStoreSqlite::savePartialTile(const NonUniformTile &t, ContentType contentType, quint32 writePass)
{
    if (!m_partsDb) {
        qWarning() << "Partial tile storage is not initialized";
        return false;
    }

    // fetch id of the tile first
    NonUniformTilesMetaInfoDao partInfoDao(m_partsDb.data());
    QString contentTypeStr = contentTypeToDaoString(contentType);
    qulonglong tilePartInfoId = 0; 
    Err e = partInfoDao.tileInfoId(t.position(), contentTypeStr, &tilePartInfoId);
    if (e == kSQLiteMissingContentError){
        partInfoDao.saveTileInfo(t.position(), contentTypeStr);
        e = partInfoDao.tileInfoId(t.position(), contentTypeStr, &tilePartInfoId);
        if (e != kNoErr) {
            qWarning() << "Saving the partial tile info failed!";
            return false;
        }
    }

    NonUniformTilesPartEntry entry;
    entry.nonUniformTilesPartInfoId = tilePartInfoId;
    entry.id = writePass;

    serializeTilePart(t, &entry.index, &entry.mz, &entry.intensity, &entry.pointCount);
    entry.scanCount = entry.index.size();

    NonUniformTilesPartDao partDao(m_partsDb.data());
    return (partDao.saveTilePart(entry) == kNoErr);
}


bool NonUniformTileStoreSqlite::initTilePartCache()
{
    bool initSuccessful = false;

    do {
        // init separate db file
        QString dbFilePath = m_db->databaseName() + ".pmi_parts";
        if (QFileInfo(dbFilePath).exists()) {
            qWarning() << "Parts cache already exists; Removing!";
            if (!QFile::remove(dbFilePath)){
                break;
            }
        }
        
        bool ok = false;
        m_partsDb = createTilePartDbConnection(dbFilePath, &ok);
        if (!ok) {
            break;
        }

        NonUniformTilesMetaInfoDao dao(m_partsDb.data());
        Err e = dao.createTable();
        if (e != kNoErr) {
            break;
        }

        NonUniformTilesPartDao partDao(m_partsDb.data());
        e = partDao.createTable();
        if (e != kNoErr) {
            break;
        }

        initSuccessful = true;

    } while (0);

    return initSuccessful;
}

bool NonUniformTileStoreSqlite::dropTilePartCache()
{
    if (!m_partsDb) {
        return false;
    }

    return removePartTileDatabase();
}


bool NonUniformTileStoreSqlite::saveTile(const NonUniformTile &t, ContentType type)
{
    // convert in-memory representation into db representation 
    NonUniformTilesDaoEntry entry;
    const QPoint &position = t.position();
    entry.posX = position.x();
    entry.posY = position.y();
    entry.contentType = contentTypeToDaoString(type);

    serializeTilePart(t, &entry.index, &entry.mz, &entry.intensity, &entry.pointCount);

    if (entry.pointCount == 0) {
        // index contain just zeros, do not store this to full tile in database to save space
        // we can't do this for partial tile, because this information is used so that we know 
        // which scan numbers are already in and which are not
        entry.index.clear();
    }

    NonUniformTilesDao dao(m_db);
    return (dao.saveTile(entry) == kNoErr);
}

NonUniformTileStore::ContentType NonUniformTileStoreSqlite::contentTypeFromDaoString(const QString& daoString) const
{
    if (daoString == NonUniformTilesDaoEntry::CONTENT_MS1_RAW) {
        return NonUniformTileStore::ContentMS1Raw;
    } else if (daoString == NonUniformTilesDaoEntry::CONTENT_MS1_CENTROIDED) {
        return NonUniformTileStore::ContentMS1Centroided;
    }

    warningTiles() << "Unknown DAO content type string" << daoString << "returning default value ContentMS1Raw";
    return NonUniformTileStore::ContentMS1Raw;
}

void NonUniformTileStoreSqlite::serializeTilePart(const NonUniformTile &t, QVector<int> * indexes, QVector<double> * mz, QVector<double> * intensity, qint32 * pointCount)
{
    if (!indexes || !mz || !intensity || !pointCount) {
        return;
    }

    indexes->clear();
    mz->clear();
    intensity->clear();

    const QVector<point2dList> &tileContent = t.data();
    
    indexes->reserve(tileContent.size());
    for (const point2dList& scanNumber : tileContent) {
        indexes->push_back(static_cast<int>(scanNumber.size()));
    }

    *pointCount = std::accumulate(indexes->cbegin(), indexes->cend(), 0);

    mz->reserve(*pointCount);
    intensity->reserve(*pointCount);
    for (const point2dList& scanNumber : tileContent) {
        for (const QPointF &mzIntensity : scanNumber) {
            mz->push_back(mzIntensity.x());
            intensity->push_back(mzIntensity.y());
        }
    }
}

void NonUniformTileStoreSqlite::deserializePartTile(const QVector<int> &indexes, const QVector<double> &mz, const QVector<double> &intensity, qint32 pointCount, NonUniformTile * tile)
{
    QVector<point2dList> tileContent;
    tileContent.reserve(indexes.size());
    int indexPosition = 0;
    for (int size : indexes) {
        point2dList scanNumber;
        scanNumber.reserve(size);
        int startPos = indexPosition;
        int endPos = startPos + size;

        for (; startPos < endPos; ++startPos) {
            double currMz = mz.at(startPos);
            double currIntensity = intensity.at(startPos);
            scanNumber.push_back(QPointF(currMz, currIntensity));
        }
        tileContent.push_back(scanNumber);

        indexPosition += size;
    }

    tile->setData(tileContent);
}

QSharedPointer<QSqlDatabase> NonUniformTileStoreSqlite::createTilePartDbConnection(const QString &dbFilePath, bool * ok)
{
    QSharedPointer<QSqlDatabase> result = QSharedPointer<QSqlDatabase>(new QSqlDatabase);
    static int connectCount = 0;
    connectCount++;
    Err e = addDatabaseAndOpen(QString("NonUniformTileStoreSqlite_createTilePartDbConnection_%1").arg(connectCount), dbFilePath, *result);
    if (ok) {
        *ok = (e == kNoErr);
    }

    return result;
}

bool NonUniformTileStoreSqlite::removePartTileDatabase()
{
    QString dbFilePath = m_partsDb->databaseName();
    m_partsDb->close(); 
    return QFile::remove(dbFilePath);
}


bool NonUniformTileStoreSqlite::defragmentTiles(NonUniformTileStore * dstStore)
{

    QVector<NonUniformTilesMetaInfoEntry> allPartTilesInfo;
    NonUniformTilesMetaInfoDao infoDao(m_partsDb.data());
    infoDao.allTileInfo(&allPartTilesInfo);

    QVector<NonUniformTilesPartEntry> parts;
    NonUniformTilesPartDao partDao(m_partsDb.data());
    
    dstStore->start();
    for (const NonUniformTilesMetaInfoEntry &entry : allPartTilesInfo) {
        //TODO: I think every content type (Profile, Centroided) belongs to separate table? in this use-case
        ContentType type = contentTypeFromDaoString(entry.contentType);

        partDao.loadTileParts(entry.id, &parts);
        // join parts together, they are already sorted correctly in dao!
        if (parts.size() != 0) {
            NonUniformTilesPartEntry first = parts[0];
            for (int i = 1; i < parts.size(); ++i) {
                first.append(parts.at(i));
            }

            // convert part to the tile
            NonUniformTile tile;
            tile.setPosition(entry.position);
            deserializePartTile(first.index, first.mz, first.intensity, first.pointCount, &tile);

            // save to db
            dstStore->saveTile(tile, type);
        }
    }
    dstStore->end();

    return true;
}

QString NonUniformTileStoreSqlite::contentTypeToDaoString(ContentType type) const
{
    switch (type) {
        case ContentMS1Raw: {
            return NonUniformTilesDaoEntry::CONTENT_MS1_RAW;
        }
        case ContentMS1Centroided: {
            return NonUniformTilesDaoEntry::CONTENT_MS1_CENTROIDED;
        }
        default: {
            warningTiles() << "Unknown type" << type << "returning default value ContentMS1Raw";
            return NonUniformTilesDaoEntry::CONTENT_MS1_RAW; 
        }
    }
}

void NonUniformTileStoreSqlite::clear()
{
    //TODO: do we need this?
    throw std::logic_error("The method or operation is not implemented.");
}

bool NonUniformTileStoreSqlite::init()
{
    NonUniformTilesDao dao(m_db);
    return dao.createTable() == kNoErr;
}

bool NonUniformTileStoreSqlite::isSchemaValid() const
{
    NonUniformTilesDao dao(m_db);
    return dao.isSchemaValid();
}

int NonUniformTileStoreSqlite::count(ContentType type)
{
    NonUniformTilesDao dao(m_db);
    int count = 0;
    Err e = dao.count(contentTypeToDaoString(type), &count); eee_absorb;
    return count;
}

NonUniformTileStore *NonUniformTileStoreSqlite::clone() const
{
    // create new connection
    QString dbFilePath = m_db->databaseName();
    QUuid uuid = QUuid::createUuid();
    QString connectionName = m_db->connectionName() + uuid.toString() + QStringLiteral("clone");
    
    QScopedPointer<QSqlDatabase> db(new QSqlDatabase());
    Err e = addDatabaseAndOpen(connectionName, dbFilePath, *db);
    if (e != kNoErr) {
        return nullptr;
    }

    bool ownDb = true;
    return new NonUniformTileStoreSqlite(db.take(), ownDb);
}

quint32 NonUniformTileStoreSqlite::pointCount(ContentType type, const QRect &tileRect)
{
    NonUniformTilesDao dao(m_db);
    quint32 count = -1;
    Err e = dao.pointCount(contentTypeToDaoString(type), tileRect, &count);
    return (e == kNoErr) ? count : -1;
}

bool NonUniformTileStoreSqlite::startPartial()
{
    return m_partsDb->transaction();
}

bool NonUniformTileStoreSqlite::endPartial()
{
    return m_partsDb->commit();
}

bool NonUniformTileStoreSqlite::start()
{
    return m_db->transaction();
}

bool NonUniformTileStoreSqlite::end()
{
    return m_db->commit();
}

bool NonUniformTileStoreSqlite::contains(const QPoint &pos, ContentType type)
{
    NonUniformTilesDao dao(m_db);
    return dao.contains(pos, contentTypeToDaoString(type));
}

_PMI_END


