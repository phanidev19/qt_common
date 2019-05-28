/*
 * Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Ivan Skiba ivan.skiba@redgreenorange.com
 */

#ifndef CACHE_FILE_MANAGER_H
#define CACHE_FILE_MANAGER_H

#include "CacheFileManagerInterface.h"

#include <QMap>

_PMI_BEGIN

class PMI_COMMON_CORE_MINI_EXPORT CacheFileManager
    : public virtual CacheFileManagerInterface
{
public:
    CacheFileManager();
    virtual ~CacheFileManager();

    QStringList searchPaths() const override;
    QString savePath() const override;

    void setSearchPaths(const QStringList &paths) override;
    void setSavePath(const QString &path) override;

    /*!
    * Note: These methods should be called only from MSReader by contract.
    *       That's why they are hidden from the CacheFileManagerInterface
    *       that is shipped outside MSReader.
    */
    void setSourcePath(const QString &sourcePath);
    void resetSourcePath();

    Err findOrCreateCachePath(const QString &suffix, QString *cachePath) const override;

private:
    Q_DISABLE_COPY(CacheFileManager)

    Err findExistingCacheFile(const QString &suffix, QString *existingPath) const;
    Err createCachePath(const QString &suffix, QString *cachePath) const;

    Err findBackwardCompatibleFile(const QString &suffix, QString *oldPath) const;
    Err createBackwardCompatiblePath(const QString &suffix, QString *oldPath) const;

    Err createCacheName(const QString &version, QString *name) const;

    Err createHashV1(QString *hash) const;

private:
    const QStringList m_supportedHashVersions;

    QStringList m_searchPaths;
    QString m_savePath;

    QString m_sourcePath;
    mutable QMap<QString, QString> m_versionToCacheName;
};

_PMI_END

#endif // CACHE_FILE_MANAGER_H
