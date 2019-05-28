/*
* Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/


#include "MSDataNonUniformAdapter.h"
#include "MSReader.h"
#include "NonUniformTileStoreSqlite.h"
#include "NonUniformTileRange.h"
#include "QtSqlUtils.h"
#include "MSReaderInfo.h"
#include "db\NonUniformTilesInfoDao.h"
#include "ScanIndexNumberConverter.h"
#include "MSDataNonUniform.h"
#include "NonUniformTileBuilder.h"
#include "ProgressBarInterface.h"
#include "db\NonUniformTilesDao.h"
#include "pmi_common_ms_debug.h"

_PMI_BEGIN

using namespace msreader;

static const QLatin1String CACHE_SUFFIX(".NonUniform.cache");

MSDataNonUniformAdapter::MSDataNonUniformAdapter(const QString &filePath)
    : m_dbFilePath(filePath)
    , m_store(nullptr)
    , m_data(nullptr)
    , m_contentType(NonUniformTileStore::ContentMS1Centroided)

{
    debugMs() << "NonUniform tiles at" << m_dbFilePath;
}

MSDataNonUniformAdapter::~MSDataNonUniformAdapter()
{
    m_db.close();
    cleanUp();
}

NonUniformTileRange MSDataNonUniformAdapter::createRange(MSReader * reader, bool centroided, int scanInfoCount, bool * ok)
{
    NonUniformTileRange range;
    MSReaderInfo msInfo(reader);

    bool mzOk = false;
    // TODO: fix database schema, we can have multiple ranges for centroided data, non-centroided data, ms1 data, ms2 data etc.
    // Currently we use only centroided MS1 data
    auto minMax = msInfo.mzMinMax(centroided, &mzOk);
    
    if (ok) {
        *ok = mzOk;
    }

    if (!mzOk) {
        return range;
    }

    double mzMin = minMax.first;
    double mzMax = minMax.second;

    // TODO: performance: determine so that ideally tiles occupies same amount of memory
    const double MZ_TILE_WIDTH = 30;
    const double SCAN_INDEX_TILE_HEIGHT = 64;
    debugMs() << "Creating cache with tile settings" << "MZ_TILE_WIDTH" << MZ_TILE_WIDTH << "SCAN_INDEX_TILE_HEIGHT" << SCAN_INDEX_TILE_HEIGHT;

    range.setMz(mzMin, mzMax);
    range.setMzTileLength(MZ_TILE_WIDTH);

    range.setScanIndex(0, scanInfoCount - 1);
    range.setScanIndexLength(SCAN_INDEX_TILE_HEIGHT);

    return range;
}

QString MSDataNonUniformAdapter::cacheFilePath() const
{
    return m_dbFilePath;
}

Err MSDataNonUniformAdapter::createNonUniformTiles(MSReader * reader, const NonUniformTileRange &range, const QList<ScanInfoWrapper> &scanInfo, QSharedPointer<ProgressBarInterface> progress)
{
    Err e = openDatabase(); ree;

    // serialize to db
    NonUniformTilesInfoDao rangeDao(&m_db);
    e = rangeDao.createTable(); ree;
    e = rangeDao.save(range); ree;

    NonUniformTileStoreSqlite store(&m_db);
    if (!store.init()) {
        return kSQLiteExecError;
    }

    m_converter = ScanIndexNumberConverter::fromMSReader(scanInfo);
    NonUniformTileBuilder builder(range);
    builder.buildNonUniformTilesNG(reader, m_converter, &store, m_contentType, progress);
    return e;
}

Err MSDataNonUniformAdapter::getXICData(const XICWindow &win, point2dList *points, int ms_level)
{
    points->clear();
    if (!m_data) {
        return kError;
    }

    if (ms_level != 1){
        return kFunctionNotImplemented;
    }

    // fix the window first!
    return m_data->getXICDataNG(win, points);
}

bool MSDataNonUniformAdapter::hasValidCacheDbFile()
{
    Err e = openDatabase(); 
    if (e != kNoErr) {
        warningMs() << "Cannot open database!";
        return false;
    }

    if (!verifyDatabaseContent()) {
        return false;
    }

    //TODO: more checks if needed
    return true;
}

bool MSDataNonUniformAdapter::isLoaded() const
{
    return (m_data != nullptr);
}

Err MSDataNonUniformAdapter::load(const QList<ScanInfoWrapper> &scanInfo)
{
    Err e = kNoErr;
    if (m_store || m_data) {
        return e;
    }

    e = openDatabase(); ree;
    
    NonUniformTilesInfoDao infoDao(&m_db);
    e = infoDao.load(&m_range); ree;
    
    m_store = new NonUniformTileStoreSqlite(&m_db);
    qDebug() << "File" << m_db.databaseName();
    m_converter = ScanIndexNumberConverter::fromMSReader(scanInfo);
    m_data = new MSDataNonUniform(m_store, m_range, m_converter);

    return e;
}

void MSDataNonUniformAdapter::cleanUp()
{
    //TODO think about ownership
    delete m_data;
    m_data = nullptr;
    delete m_store;
    m_store = nullptr;
}

double MSDataNonUniformAdapter::scanNumberToScanTime(long scanNumber) const
{
    if (!isLoaded()) {
        warningMs() << "Bad API usage: call load() first! Returning -1.0";
        return -1.0;
    }

    double scanTime = m_data->converter().toScanTime(scanNumber);
    return scanTime;
}

bool MSDataNonUniformAdapter::verifyDatabaseContent()
{
    NonUniformTileStoreSqlite store(&m_db);
    if (!store.isSchemaValid()) {
        return false;
    }

    return (hasData(NonUniformTileStore::ContentMS1Centroided)
            || hasData(NonUniformTileStore::ContentMS1Raw));
}

Err MSDataNonUniformAdapter::openDatabase()
{
    Err e = kNoErr;
    if (m_db.isOpen() && m_db.databaseName() == m_dbFilePath) {
        return e;
    } else {
        m_db.close();
    }
    
    
    static int connectCount = 0;
    connectCount++;
    
    // move to constructor
    e = addDatabaseAndOpen(QString("MSDataNonUniformAdapter_%1").arg(connectCount), m_dbFilePath, m_db); ree;
    return e;
}

Err MSDataNonUniformAdapter::closeDatabase()
{
    m_db.close();
    return kNoErr;
}

NonUniformTileRange MSDataNonUniformAdapter::loadRange(bool * ok)
{
    NonUniformTileRange range;
    Err e = openDatabase();
    
    if (e != kNoErr) {
        qWarning() << "Failed to load range!";
        *ok = false;
        return range;
    }

    NonUniformTilesInfoDao infoDao(&m_db);
    e = infoDao.load(&range); 

    if (e != kNoErr) {
        qWarning() << "Failed to load range!";
        *ok = false;
        return range;
    }

    *ok = true;
    closeDatabase();

    return range;
}

bool MSDataNonUniformAdapter::hasData(NonUniformTileStore::ContentType type)
{
    NonUniformTileStoreSqlite store(&m_db);
    return store.count(type) > 0;
}

QString MSDataNonUniformAdapter::formatSuffix()
{
    return CACHE_SUFFIX;
}

_PMI_END
