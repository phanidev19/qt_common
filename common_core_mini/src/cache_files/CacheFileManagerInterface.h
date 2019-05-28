/*
 * Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Ivan Skiba ivan.skiba@redgreenorange.com
 */

#ifndef CACHE_FILE_MANAGER_INTERFACE_H
#define CACHE_FILE_MANAGER_INTERFACE_H

#include "pmi_common_core_mini_export.h"

#include <common_errors.h>

#include <QStringList>

_PMI_BEGIN

/*!
 * \brief Provides functionality for finding existing cache files (like *.ms1.cache,
 *        *.NonUniform.cache, *.chromatogram_only.byspec2 and etc) or determining where new cache
 *        file should be located.
 *
 * This is a part of the MSReader. The CacheFileManagerInterface implementation lifetime should be
 * sticked to the MSReader lifetime if the MSReader exists.
 * Can be accessed via MSReader::cacheFileManager().
 */
class CacheFileManagerInterface
{
public:
    /*!
    * \brief Gets paths ordered by priority where to search the cache files.
    * \return search paths list
    */
    virtual QStringList searchPaths() const = 0;
    /*!
    * \brief Gets path where to save new cache files.
    * \return save path
    */
    virtual QString savePath() const = 0;

    /*!
    * \brief Sets paths ordered by priority where to search the cache files.
    * \param paths [in]
    */
    virtual void setSearchPaths(const QStringList &paths) = 0;
    /*!
    * \brief Sets path where to save new cache files.
    * \param path [in]
    */
    virtual void setSavePath(const QString &path) = 0;

    /*!
     * \brief Finds existing cache file or determine where to locate new cache file for the current
     *        opened in MSReader data file.
     * \param suffix [in] (examples: ".ms1.cache", ".NonUniform.cache" and etc)
     * \param cachePath [out] is an existing or new cache path
     * \return Err
     */
    virtual Err findOrCreateCachePath(const QString &suffix, QString *cachePath) const = 0;

protected:
    ~CacheFileManagerInterface() {}
};

_PMI_END

#endif // CACHE_FILE_MANAGER_INTERFACE_H
