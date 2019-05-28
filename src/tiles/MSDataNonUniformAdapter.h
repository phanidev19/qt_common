/*
* Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef MSDATA_NONUNIFORM_ADAPTER_H
#define MSDATA_NONUNIFORM_ADAPTER_H

#include <QSqlDatabase>

#include <common_math_types.h>
#include <pmi_core_defs.h>

#include "config-pmi_qt_common.h"
#include "MsReaderTypes.h"
#include "pmi_common_ms_export.h"
#include "ScanIndexNumberConverter.h"

#include "NonUniformTileStore.h"
#include "NonUniformTileRange.h"

class ProgressBarInterface;

_PMI_BEGIN

class MSDataNonUniform;
class MSReader;
class NonUniformTileStoreSqlite;

class PMI_COMMON_MS_EXPORT MSDataNonUniformAdapter final
{
public:
    explicit MSDataNonUniformAdapter(const QString &filePath);
    ~MSDataNonUniformAdapter();

    //! \brief Creates the tiles for the database file
    //!
    //! @note: Call MSDataNonUniformAdapter::setContentType() if you want to create different
    //! content type.By default centroided data are created.
    //! 
    //! @param reader MSReader with opened raw file
    //! @param range Settings for the tile system, @see MSDataNonUniformAdapter::createRange
    //! function
    //! @param scanInfo scanInfo scan info related to opened MS file
    //! @param progress unused currently
    Err createNonUniformTiles(MSReader * reader, const NonUniformTileRange &range, const QList<msreader::ScanInfoWrapper> &scanInfo, QSharedPointer<ProgressBarInterface> progress);
    
    //! \brief follows MSReader API to provide XICData, @see MSReader::getXICData
    Err getXICData(const msreader::XICWindow &win, point2dList *points, int ms_level);

    //! \brief Checks if the provided MSData has existing valid cache file
    //! Note: opens the database to check the schema
    bool hasValidCacheDbFile(); //TODO const?
    
    //! \brief @return Returns true if the database cache is loaded and getXICData is ready to work
    //! Call @see MSDataNonUniformAdapter::load 
    bool isLoaded() const;

    //! \brief load the cache file for MS Data provided in constructor
    Err load(const QList<msreader::ScanInfoWrapper> &scanInfo);

    //! \brief expose the convertion of the scan numbers to scan times to MSReader
    //! better design needed here 
    //@return -1 if converter is not ready, @see MSDataNonUniformAdapter::load
    double scanNumberToScanTime(long scanNumber) const;

    //! @scanInfoCount - count of scan numbers in MS Data
    //! @param centroided - true if you want to create the range on centroided data, false for non-centroided data
    static NonUniformTileRange createRange(MSReader * reader, bool centroided, int scanInfoCount, bool * ok);

    //! @return returns file path to used cache sqlite database 
    QString cacheFilePath() const;
    
    Err closeDatabase();

    //! \brief loads tile system configuration
    //! @return current tile system configuration from the cache file
    NonUniformTileRange loadRange(bool * ok);

    MSDataNonUniform *nonUniformData() const { return m_data; }
    NonUniformTileRange range() const { return m_range; };

    //! \brief Sets the content type used during building types
    //  @see createNonUniformTiles function
    void setContentType(NonUniformTileStore::ContentType contentType) { m_contentType = contentType; }

    bool hasData(NonUniformTileStore::ContentType type);

    NonUniformTileStoreSqlite *store() const { return m_store; } 

    //! \brief Returns the suffix (extension) for NonUniform cache files
    static QString formatSuffix();

private:
    Err openDatabase();
    void cleanUp();

    bool verifyDatabaseContent();

private:
    QSqlDatabase m_db;
    QString m_dbFilePath;
    NonUniformTileRange m_range;
    NonUniformTileStoreSqlite * m_store;
    MSDataNonUniform * m_data;
    ScanIndexNumberConverter m_converter;
    NonUniformTileStore::ContentType m_contentType;


#ifdef PMI_QT_COMMON_BUILD_TESTING
    friend class MSDataNonUniformAdapterTest;
#endif
};

_PMI_END

#endif // MSDATA_NONUNIFORM_ADAPTER_H
