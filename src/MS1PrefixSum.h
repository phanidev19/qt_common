/*
 * Copyright (C) 2012-2014 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 */

#ifndef __MS_MS1PREFIXSUM_H__
#define __MS_MS1PREFIXSUM_H__

#include "MSReader.h"


class MSReaderBrukerTest;

_PMI_BEGIN

class ProgressBarInterface;

class PMI_COMMON_MS_EXPORT MS1PrefixSum final
{
public:
    explicit MS1PrefixSum(MSReader * ms);
    ~MS1PrefixSum();

    bool isValid();

    /// generate sqlite file with prefix sum
    Err createPrefixSumCache(QSharedPointer<ProgressBarInterface> progress);

    Err getScanData(long scanNumber, GridUniform * grid) const;

    //! Rounds up, @returns LONG_MIN when not found
    long findClosestAvailableScanNumber(long scanNumber) const;

    void setMaxNumberOfScansToCache(int maxNumberOfScansToCache) {
        m_maxNumberOfScansToCache = maxNumberOfScansToCache;
    }

    int getMaxNumberOfScansToCache() const {
        return m_maxNumberOfScansToCache;
    }

    // Returns true if info table is not corrupted and is valid for this version of MS1PrefixSum and false otherwise
    // Returns true if ran correctly and false if didn't run correctly
    Err removeAllTablesRelatedToMS1Cache();

    bool deleteCacheFile();

private:
    Err _validateAndWriteVersionNumberInCache();
    Err _createInfoTable();
    bool _infoTableExists();
    Err _isValidInfoTable();

    Err openDatabase();
    Err _checkValidAndInitialize(bool silent = false);

private:
    QMap<long, bool> m_cachedScanNumbers;
    int m_maxNumberOfScansToCache = 100;
    MSReader * m_ms = NULL;
    QString m_filename; ///sqlite file
    QSqlDatabase * m_db = NULL;
    GridUniform m_gridUniformTemplate; //contains to world mapping (e.g. start_x and scale_x)
    friend class MS1PrefixSumTest;
    QString m_databaseConnectionName;
};

_PMI_END

#endif
