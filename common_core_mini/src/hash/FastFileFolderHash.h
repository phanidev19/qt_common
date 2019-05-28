/*
 * Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Ivan Skiba ivan.skiba@redgreenorange.com
 */

#ifndef FAST_FILE_FOLDER_HASH_H
#define FAST_FILE_FOLDER_HASH_H

#include "pmi_common_core_mini_export.h"

#include <common_errors.h>

#include <QCryptographicHash>
#include <QFile>

_PMI_BEGIN

class PMI_COMMON_CORE_MINI_EXPORT FastFileFolderHash final
{
public:
    static Err hash(const QString &path, QCryptographicHash::Algorithm algorithm, int sampleCount,
                    qint64 sampleSize, int minSizeForFastHashMultiplier,
                    int maxFileCountInFolderToUse, QByteArray *result);

public:
    FastFileFolderHash(QCryptographicHash::Algorithm algorithm, int sampleCount, qint64 sampleSize,
                       int minSizeForFastHashMultiplier, int maxFileCountInFolderToUse);
    ~FastFileFolderHash();

    void setExtensionPartsToSkip(const QStringList &extensionPartsToSkip);

    Err addPath(const QString &path);

    void reset();

    QByteArray result() const;

private:
    Err addFile(const QString &path);
    Err addDir(const QString &path);

private:
    const int m_sampleCount;
    const qint64 m_sampleSize;
    const int m_minSizeForFastHashMultiplier;
    const int m_maxFileCountInFolderToUse;

    QCryptographicHash m_hash;
    QStringList m_extensionPartsToSkip;
};

_PMI_END

#endif // FAST_FILE_FOLDER_HASH_H
