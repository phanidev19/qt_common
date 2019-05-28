/*
 * Copyright (C) 2012-2014 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 */

#include "MS1PrefixSum.h"
#include "pmi_common_ms_debug.h"
#include "ProgressBarInterface.h"
#include "ProgressContext.h"

#include <CacheFileManagerInterface.h>
#include <DateTimeUtils.h>
#include <PmiQtCommonConstants.h>

#include <QtSqlUtils.h>
#include <sqlite_utils.h>

#include <ctime>

_PMI_BEGIN

#define ree_silent(silent) { if (e != kNoErr) { if (silent) { return e; } ree; } }

//Note(2015-02-22): Made to version 2.  Just to make sure these are re-created
#define kGridUniformVersionNumber 2

static const QLatin1String CACHE_SUFFIX(".ms1.cache");

MS1PrefixSum::MS1PrefixSum(MSReader * ms) : m_ms(ms) {
    m_db = new QSqlDatabase;
}

MS1PrefixSum::~MS1PrefixSum() {
    if (m_db) {
        if(m_db->isOpen()) {
            m_db->close();
        }
        delete m_db;
        QSqlDatabase::removeDatabase(m_databaseConnectionName);
    }
}

Err MS1PrefixSum::openDatabase()
{
    Err e = kNoErr;

    if (!m_db || !m_ms) {
        rrr(kBadParameterError);
    }

    QString cacheFileName;

    e = m_ms->cacheFileManager()->findOrCreateCachePath(QLatin1String(CACHE_SUFFIX),
                                                        &cacheFileName); ree;

    if (m_db->isOpen()) {
        if (m_db->databaseName() == cacheFileName) {
            return e;
        }

        m_db->close();
    }

    // TODO(Ivan Skiba 2017-06-19): should not use static
    static int s_databaseCount = 0;

    m_filename = cacheFileName;
    m_databaseConnectionName = QString("MS1Cache_%1").arg(s_databaseCount++);

    e = addDatabaseAndOpen(m_databaseConnectionName, m_filename, *m_db); ree;

    return e;
}

Err MS1PrefixSum::_isValidInfoTable() {
    Err e = kNoErr;

    QSqlQuery q = makeQuery(*m_db, true);

    e = QEXEC_CMD(q, QString("SELECT Key, Value FROM GridUniformInfo WHERE Key = '%1'").arg(kGridUniformVersion)); ree;
    // If nothing could be selected from table GridUniformInfo
    if (!q.next()) {
        rrr(kBadParameterError);
    }

    // If name string doesn't match
    if (q.value(0).toString().compare(kGridUniformVersion) != 0) {
        warningMs() << "Name didn't match in Info table of cache file.";
        warningMs() << "Name in file: " << q.value(0).toString() << ", name expected: " << kGridUniformVersion;
        e = kError; ree;
    }
    //  If version number is less than kMS1PrefixSumVersionNumber
    // commonMsWarning() << "m_db file" << m_db->databaseName();
    // commonMsWarning() << "Version in file is: " << q.value(1).toInt() << ", version expected to be at least: " << kGridUniformVersionNumber;
    if (q.value(1).toInt() < kGridUniformVersionNumber) {
        warningMs() << "Version number noted in cache file is invalid.";
        warningMs() << "Version in file is: " << q.value(1).toInt() << ", version expected to be at least: " << kGridUniformVersionNumber;
        e = kError; ree;
    }

    // Validate Scans exist
    e = QEXEC_CMD(q, "SELECT * FROM GridUniformScans");
    // If GridUniformScans is empty
    if (!q.next()) {
        warningMs() << "GridUniformScans is empty";
        rrr(kBadParameterError);
    }

    return e;
}

Err MS1PrefixSum::removeAllTablesRelatedToMS1Cache() {
    Err e = kNoErr;

    debugMs() << "Removing all GridUniform tables from cache.";
    QStringList allTableNames;
    QSqlQuery q = makeQuery(*m_db, true);
    e = GetSQLiteTableNames(*m_db, allTableNames); ree;

    QString dropCommand = QString("DROP TABLE IF EXISTS %1").arg(kGridUniformInfo);
    debugMs() << "Executing command: " + dropCommand;
    e = QEXEC_CMD(q, dropCommand); ree;

    dropCommand = QString("DROP TABLE IF EXISTS %1").arg(kGridUniformScans);
    debugMs() << "Executing command: " + dropCommand;
    e = QEXEC_CMD(q, dropCommand); ree;

    dropCommand = QString("DROP TABLE IF EXISTS %1").arg(kGridUniformMeta);
    debugMs() << "Executing command: " + dropCommand;
    e = QEXEC_CMD(q, dropCommand); ree;

    return e;
}

bool MS1PrefixSum::deleteCacheFile() {
    debugMs() << "Deleting cache file.";
    bool cacheFileDeletedCorrectly = true;
    if (m_db->isOpen()) {
        m_db->close();
    }
    if (QFile::remove(m_filename) != true) {
        cacheFileDeletedCorrectly = false;
    }
    if (!cacheFileDeletedCorrectly) {
        warningMs() << "Could not delete sum cache file: " << m_filename;
    }
    return cacheFileDeletedCorrectly;
}

Err MS1PrefixSum::_createInfoTable() {
    Err e = kNoErr;
    QSqlQuery q = makeQuery(*m_db, true);

    e = QEXEC_CMD(q, "CREATE TABLE IF NOT EXISTS GridUniformInfo(Id INTEGER PRIMARY KEY, Key TEXT, Value TEXT)"); ree;
    e = QPREPARE(q, "INSERT INTO GridUniformInfo(Key,Value) VALUES(?,?)"); ree;
    q.bindValue(0, kGridUniformVersion);
    q.bindValue(1, kGridUniformVersionNumber);
    e = QEXEC_NOARG(q); ree;

    q.bindValue(0, kGridUniformCreateTime);
    q.bindValue(1, DateTimeUtils::toString(QTime::currentTime()));
    e = QEXEC_NOARG(q); ree;
    return e;
}

bool MS1PrefixSum::_infoTableExists() {
    bool tableExists = containsTable(*m_db, kGridUniformInfo);
    return tableExists;
}

Err MS1PrefixSum::_checkValidAndInitialize(bool silent)
{
    Err e = kNoErr;

    m_cachedScanNumbers.clear();

    QSqlQuery q = makeQuery(*m_db, true);
    e = QEXEC_CMD(q, "SELECT Start, Scale FROM GridUniformMeta"); ree_silent(silent);
    if (!q.next()) {
        e = kBadParameterError;
        ree_silent(silent)
    }

    m_gridUniformTemplate.start_x = q.value(0).toDouble();
    m_gridUniformTemplate.scale_x = q.value(1).toDouble();

    //Note: SELECT ... Content ... <-- The reading of Content causes significant slowdown
    //as this requires reading large amount of data from disk. Uncertain if this is at
    //sqlite level or if QtSql is actually doing the reading.  Regardless, avoid adding
    //columns not required, espcially large blobs.
    //Also note that this only occurs the first time it reads from disk.  Once saved into memory, it's
    //fast again.  To reproduce this, reboot the machine to wipe any cache in memory.
    e = QEXEC_CMD(q, "SELECT ScanNumber FROM GridUniformScans"); ree_silent(silent);
    while (q.next()) {
        // toInt because toLong doesn't exist
        m_cachedScanNumbers.insert(q.value(0).toInt(), 1);
    }

    return e;
}

bool MS1PrefixSum::isValid() {
    if (m_ms == NULL || m_db == NULL)
        return false;
    Err e = openDatabase();
    if (e != kNoErr)
        return false;
    if (!m_db->isOpen()) {
        return false;
    }
    e = _checkValidAndInitialize(true);
    if (e != kNoErr) {
        return false;
    }

    // If there isn't an info table or the info table is not valid
    if (!_infoTableExists() || _isValidInfoTable()!=kNoErr) {
        return false;
    }

    //TODO: check sqlite content more carefully; add a flag (or checksum) if properly populated.

    return true;
}

// Writing PrefixSum information/cache to SQLite file
Err MS1PrefixSum::createPrefixSumCache(QSharedPointer<ProgressBarInterface> progress) {

    clock_t start = clock();

    Err e = kNoErr;
    QList<msreader::ScanInfoWrapper> list;
    PlotBase plot;
    GridUniform grid;
    QSqlQuery q;
    double startMz=0, endMz = 1;
    static double mz_bin_space = 0.005;
    QByteArray ba;
    TransactionInstance ta(m_db);
    int maxNumberOfScansToCache = m_maxNumberOfScansToCache;

    ta.setRollbackOnDestruction(true);

    if (!m_db || !m_ms)
        return kError;

    //Some MS data do not have MS1 but only MS2; we'll assume that is MS1
    int msLevel = m_ms->bestMSLevelOne();

    e = m_ms->getScanInfoListAtLevel(msLevel, &list); ree;
    if (list.size() <= 0) {
        debugMs() << "There's nothing to cache.";
        return e;
    }

    e = openDatabase(); ree;

    e = m_ms->getDomainInterval_sampleContent(msLevel, &startMz, &endMz); ree;
    grid.initGridByMzBinSpace(startMz, endMz, mz_bin_space);
    m_gridUniformTemplate.initGridByMzBinSpace(startMz, endMz, mz_bin_space);

    q = makeQuery(*m_db, true);
    ta.beginTransaction();

    e = QEXEC_CMD(q, "CREATE TABLE IF NOT EXISTS GridUniformMeta(Id INTEGER PRIMARY KEY \
                  , Start REAL \
                  , Scale REAL \
                  )"); ree;

    //ta.beginTransaction();
    e = QPREPARE(q, "INSERT INTO GridUniformMeta(Start,Scale) VALUES(?,?)"); ree;
    q.bindValue(0, grid.start_x);
    q.bindValue(1, grid.scale_x);
    e = QEXEC_NOARG(q); ree;

    e = QEXEC_CMD(q, "CREATE TABLE IF NOT EXISTS GridUniformScans(ScanNumber INTEGER PRIMARY KEY \
                  , Content BLOB \
                  )"); ree;
    e = QPREPARE(q, "INSERT INTO GridUniformScans(ScanNumber,Content) VALUES(?,?)"); ree;

    maxNumberOfScansToCache = std::min(list.size(), maxNumberOfScansToCache);
    // Make sure maxNumberOfScans is a valid number (not zero or negative)
    if (maxNumberOfScansToCache < 1) {
        maxNumberOfScansToCache = 1;
    }

    int kth = list.size() / maxNumberOfScansToCache;
    if (kth <= 0) {
        debugMs() << "Note: list.size(), maxNumberOfScansToCache, kth=" << list.size() << "," << maxNumberOfScansToCache << "," << kth << ". kth now changed to 1";
        kth = 1;
    }

    if (progress) {
        QString filename = m_ms->getFilename();
        progress->setText("Computing MS cache " + filename);
    }
    {
        ProgressContext progressContext(list.size(), progress);
        for (int i = 0; i < list.size(); ++i, ++progressContext) {
            e = m_ms->getScanData(list[i].scanNumber, &plot.getPointList()); ree;
            if (!plot.isSortedAscendingX()) {
                plot.sortPointListByX();
            }
            // adds data from plot to grid
            grid.accumulate(plot);

            //cache the first and last
            if ((i % kth == 0 && m_maxNumberOfScansToCache >= 0) || (i == list.size() - 1)) {
                grid.toByteArray_float(ba);
                //QByteArray compressed_ba = qCompress(ba, 9);
                q.bindValue(0, list[i].scanNumber);
                q.bindValue(1, ba);
                e = QEXEC_NOARG(q); ree;
            }
        }
    }

    e = _createInfoTable(); ree;

    ta.endTransaction();

    e = _checkValidAndInitialize(); ree;

    clock_t end = clock();

    debugMs() << "time of prefix sum cache creation: " << double(end - start) / CLOCKS_PER_SEC;

    return e;
}

/*
. - not cached
| - cached

0123456789012345678901234567890012345678900123456789001234567890
.....|.......|........|....|...|||........|.....
      [                   ]

prefix(26) - prefix(6)
*/

// Reads SQLite file/cache
Err MS1PrefixSum::getScanData(long scanNumber, GridUniform * grid) const {
    Err e = kNoErr;
    if (m_db == NULL) {
        rrr(kBadParameterError);
    }

    QSqlQuery q = makeQuery(*m_db, true);

    // Only select scan numbers that exist by automatically finding closest first
    const long closest_scan_number = findClosestAvailableScanNumber(scanNumber);
    //check it is valid. if it.key != .end()
    if (closest_scan_number == LONG_MIN) {
        rrr(kBadParameterError);
    }
    e = QEXEC_CMD(q, QString("SELECT Content FROM GridUniformScans WHERE ScanNumber = %1").arg(closest_scan_number)); ree;

    if (!q.next()) {
        rrr(kBadParameterError);
    }

    QByteArray ba = q.value(0).toByteArray();
    //QByteArray ba_decompressed = qUncompress(ba);
    *grid = m_gridUniformTemplate;
    grid->fromByteArray_float(ba);
    //debugMs() << grid;

    if (closest_scan_number == scanNumber) {
        return e;
    }

    long manual_start_scan, manual_end_scan;
    MSReader * ms = (MSReader*)m_ms;
    int msLevel = ms->bestMSLevelOne();
    if (closest_scan_number < scanNumber) {
        manual_start_scan = closest_scan_number;
        manual_end_scan = scanNumber;
        e = ms->_getScanDataMS1SumManualScanNumber(msLevel, manual_start_scan, manual_end_scan, GridUniform::ArithmeticType_Add, grid); ree;
    }
    else {
        manual_start_scan = scanNumber;
        manual_end_scan = closest_scan_number;
        e = ms->_getScanDataMS1SumManualScanNumber(msLevel, manual_start_scan, manual_end_scan, GridUniform::ArithmeticType_Subtract, grid); ree;
    }

    return e;
}

long MS1PrefixSum::findClosestAvailableScanNumber(long scanNumber) const
{
    if (m_cachedScanNumbers.isEmpty()) {
        return LONG_MIN;
    }
    QMap<long, bool>::const_iterator returnValue = m_cachedScanNumbers.end();

    int differenceBetweenLowerAndNumber = INT_MAX, differenceBetweenGreaterAndNumber = INT_MAX;

    // lowerBound returns key that is strictly greater-or-equal
    // upperBound returns key that is strictly greater

    // Find the key that matches exactly or is greater
    auto it_greater = m_cachedScanNumbers.lowerBound(scanNumber);
    QMap<long, bool>::const_iterator it_end = m_cachedScanNumbers.end();
    QMap<long, bool>::const_iterator it_begin = m_cachedScanNumbers.begin();
    QMap<long, bool>::const_iterator it_lesser;
    // Get key that is strictly lower
    // check for end
    if (it_greater != m_cachedScanNumbers.begin()) {
        // Since it_greater contains a key that is strictly the same or greater, the one directly before will be strictly lesser
        it_lesser = it_greater - 1;
    }
    else {
        it_lesser = m_cachedScanNumbers.begin();
    }

    // Rounds up
    if (it_lesser != it_end && it_greater != it_end) {
        differenceBetweenLowerAndNumber = std::abs(scanNumber - it_lesser.key());
        differenceBetweenGreaterAndNumber = std::abs(scanNumber - it_greater.key());

        if (differenceBetweenLowerAndNumber < differenceBetweenGreaterAndNumber) {
            returnValue = it_lesser;
        }
        else {
            returnValue = it_greater;
        }
    }

    if (it_greater == it_end) {
        returnValue = it_greater - 1;
    }

    if (returnValue != m_cachedScanNumbers.end()) {
        //debugMs() << "closest scan number found for scanNumber,foundScanNumber=" << scanNumber << "," << returnValue.key();
    }
    else {
        debugMs() << "closest scan number not found for scanNumber=" << scanNumber;
    }

    return returnValue.key();
}

_PMI_END
