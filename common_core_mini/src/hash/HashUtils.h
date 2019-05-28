/*
 * Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Ivan Skiba ivan.skiba@redgreenorange.com
 */

#ifndef HASH_UTILS_H
#define HASH_UTILS_H

#include "pmi_common_core_mini_export.h"

#include <common_errors.h>

#include <QCryptographicHash>

_PMI_BEGIN

PMI_COMMON_CORE_MINI_EXPORT QList<QByteArray> supportedHashAlgorithmNames();
PMI_COMMON_CORE_MINI_EXPORT bool isSupportedHashAlgorithmName(const QByteArray &name);
PMI_COMMON_CORE_MINI_EXPORT QCryptographicHash::Algorithm
hashAlgorithmByNameUnsafe(const QByteArray &name);
PMI_COMMON_CORE_MINI_EXPORT Err hashAlgorithmByName(const QByteArray &name,
                                                    QCryptographicHash::Algorithm *algorithm);

PMI_COMMON_CORE_MINI_EXPORT QByteArray hashAlgorithmName(QCryptographicHash::Algorithm algorithm);

_PMI_END

#endif // HASH_UTILS_H
