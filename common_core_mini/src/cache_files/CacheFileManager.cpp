/*
 * Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Ivan Skiba ivan.skiba@redgreenorange.com
 */

#include "CacheFileManager.h"
#include "FastFileFolderHash.h"

#include <QDir>
#include <QFileInfo>

_PMI_BEGIN

static const QLatin1String CURRENT_HASH_VERSION("1");

CacheFileManager::CacheFileManager()
    : m_supportedHashVersions({ CURRENT_HASH_VERSION })
{
}

CacheFileManager::~CacheFileManager()
{
}

QStringList CacheFileManager::searchPaths() const
{
    return m_searchPaths;
}

QString CacheFileManager::savePath() const
{
    return m_savePath;
}

void CacheFileManager::setSearchPaths(const QStringList &paths)
{
    m_searchPaths = paths;
}

void CacheFileManager::setSavePath(const QString &path)
{
    m_savePath = path;
}

void CacheFileManager::setSourcePath(const QString &sourcePath)
{
    if (m_sourcePath != sourcePath) {
        m_sourcePath = sourcePath;
        m_versionToCacheName.clear();
    }
}

void CacheFileManager::resetSourcePath()
{
    return setSourcePath(QString());
}

Err CacheFileManager::findOrCreateCachePath(const QString &suffix, QString *cachePath) const
{
    Err e = kNoErr;

    QString existingPath;
    QString newPath;

    e = findExistingCacheFile(suffix, &existingPath); ree;
    e = createCachePath(suffix, &newPath); ree;

    *cachePath = !existingPath.isEmpty() ? existingPath : newPath;

    return e;
}

Err CacheFileManager::findExistingCacheFile(const QString &suffix, QString *existingPath) const
{
    Err e = kNoErr;

    QString oldPath;

    e = findBackwardCompatibleFile(suffix, &oldPath); ree;

    if (!oldPath.isEmpty()) {
        *existingPath = oldPath;

        return e;
    }

    // If m_searchPaths is empty then additional preparations like cache calculation aren't
    // required.
    if (m_searchPaths.empty()) {
        return e;
    }

    for (const auto &hashVersion : m_supportedHashVersions) {
        QString cacheName;

        e = createCacheName(hashVersion, &cacheName); ree;

        for (const auto &searchPath : m_searchPaths) {
            const QDir searchDir(searchPath);
            const QString path = searchDir.absoluteFilePath(cacheName + suffix);

            if (QFileInfo(path).exists()) {
                *existingPath = path;

                return e;
            }
        }
    }

    *existingPath = QString();

    return e;
}

Err CacheFileManager::findBackwardCompatibleFile(const QString &suffix, QString *oldPath) const
{
    Err e = kNoErr;

    QString path;

    e = createBackwardCompatiblePath(suffix, &path); ree;
    *oldPath = QFileInfo(path).exists() ? path : QString();

    return e;
}

Err CacheFileManager::createCachePath(const QString &suffix, QString *cachePath) const
{
    Err e = kNoErr;

    if (m_savePath.isEmpty()) {
        e = createBackwardCompatiblePath(suffix, cachePath); ree;

        return e;
    }

    const QDir saveDir(m_savePath);
    QString cacheName;

    e = createCacheName(CURRENT_HASH_VERSION, &cacheName); ree;

    *cachePath = saveDir.absoluteFilePath(cacheName + suffix);

    return e;
}

Err CacheFileManager::createBackwardCompatiblePath(const QString &suffix, QString *oldPath) const
{
    if (m_sourcePath.isEmpty()) {
        rrr(kBadParameterError);
    }

    *oldPath = m_sourcePath + suffix;

    return kNoErr;
}

Err CacheFileManager::createCacheName(const QString &version, QString *name) const
{
    Err e = kNoErr;

    if (m_versionToCacheName.find(version) != std::end(m_versionToCacheName)) {
        *name = m_versionToCacheName[version];

        return e;
    }

    if (version == CURRENT_HASH_VERSION) {
        QString hash;

        e = createHashV1(&hash); ree;
        m_versionToCacheName[version] = version + QLatin1String("-") + hash;
        *name = m_versionToCacheName[version];

        return e;
    }

    rrr(kBadParameterError);
}

static Err createHash(const QString &path, QCryptographicHash::Algorithm algorithm, int sampleCount,
                      qint64 sampleSize, int minSizeForFastHashMultiplier,
                      int maxFileCountInFolderToUse, QString *name)
{
    Err e = kNoErr;

    QByteArray hashValue;

    e = FastFileFolderHash::hash(path, algorithm, sampleCount, sampleSize,
                                 minSizeForFastHashMultiplier, maxFileCountInFolderToUse,
                                 &hashValue); ree;
    *name = QLatin1String(hashValue.toHex());

    return e;
}

Err CacheFileManager::createHashV1(QString *hash) const
{
    Err e = kNoErr;

    const QCryptographicHash::Algorithm algorithm = QCryptographicHash::Sha1;
    const int sampleCount = 10;
    const qint64 sampleSize = 64 * 1024;
    const int minSizeForFastHashMultiplier = 10;
    const int maxFileCountInFolderToUse = 10;

    e = createHash(m_sourcePath, algorithm, sampleCount, sampleSize, minSizeForFastHashMultiplier,
                   maxFileCountInFolderToUse, hash); ree;

    return e;
}

_PMI_END
