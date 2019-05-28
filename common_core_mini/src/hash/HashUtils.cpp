/*
 * Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Ivan Skiba ivan.skiba@redgreenorange.com
 */

#include "HashUtils.h"

#include <QMap>

_PMI_BEGIN

static const char *MD4_NAME = "Md4";
static const char *MD5_NAME = "Md5";
static const char *SHA1_NAME = "Sha1";
static const char *SHA224_NAME = "Sha224";
static const char *SHA256_NAME = "Sha256";
static const char *SHA384_NAME = "Sha384";
static const char *SHA512_NAME = "Sha512";
static const char *SHA3_224_NAME = "Sha3_224";
static const char *SHA3_256_NAME = "Sha3_256";
static const char *SHA3_384_NAME = "Sha3_384";
static const char *SHA3_512_NAME = "Sha3_512";

QList<QByteArray> supportedHashAlgorithmNames()
{
    return { MD4_NAME,    MD5_NAME,      SHA1_NAME,     SHA224_NAME,   SHA256_NAME,  SHA384_NAME,
             SHA512_NAME, SHA3_224_NAME, SHA3_256_NAME, SHA3_384_NAME, SHA3_512_NAME };
}

bool isSupportedHashAlgorithmName(const QByteArray &name)
{
    const QList<QByteArray> names = supportedHashAlgorithmNames();

    return std::find(std::cbegin(names), std::cend(names), name) != std::cend(names);
}

QCryptographicHash::Algorithm hashAlgorithmByNameUnsafe(const QByteArray &name)
{
    const QMap<QByteArray, QCryptographicHash::Algorithm> NAME_TO_ALGORITHM
        = { { MD4_NAME, QCryptographicHash::Md4 },
            { MD5_NAME, QCryptographicHash::Md5 },
            { SHA1_NAME, QCryptographicHash::Sha1 },
            { SHA224_NAME, QCryptographicHash::Sha224 },
            { SHA256_NAME, QCryptographicHash::Sha256 },
            { SHA384_NAME, QCryptographicHash::Sha384 },
            { SHA512_NAME, QCryptographicHash::Sha512 },
            { SHA3_224_NAME, QCryptographicHash::Sha3_224 },
            { SHA3_256_NAME, QCryptographicHash::Sha3_256 },
            { SHA3_384_NAME, QCryptographicHash::Sha3_384 },
            { SHA3_512_NAME, QCryptographicHash::Sha3_512 } };

    return NAME_TO_ALGORITHM[name];
}

Err hashAlgorithmByName(const QByteArray &name, QCryptographicHash::Algorithm *algorithm)
{
    if (!isSupportedHashAlgorithmName(name)) {
        return kBadParameterError;
    }

    *algorithm = hashAlgorithmByNameUnsafe(name);

    return kNoErr;
}

QByteArray hashAlgorithmName(QCryptographicHash::Algorithm algorithm)
{
    const QMap<QCryptographicHash::Algorithm, QByteArray> ALGORITHM_TO_NAME
        = { { QCryptographicHash::Md4, MD4_NAME },
            { QCryptographicHash::Md5, MD5_NAME },
            { QCryptographicHash::Sha1, SHA1_NAME },
            { QCryptographicHash::Sha224, SHA224_NAME },
            { QCryptographicHash::Sha256, SHA256_NAME },
            { QCryptographicHash::Sha384, SHA384_NAME },
            { QCryptographicHash::Sha512, SHA512_NAME },
            { QCryptographicHash::Sha3_224, SHA3_224_NAME },
            { QCryptographicHash::Sha3_256, SHA3_256_NAME },
            { QCryptographicHash::Sha3_384, SHA3_384_NAME },
            { QCryptographicHash::Sha3_512, SHA3_512_NAME } };

    if (ALGORITHM_TO_NAME.find(algorithm) != std::cend(ALGORITHM_TO_NAME)) {
        return ALGORITHM_TO_NAME[algorithm];
    }

    return "Unknown";
}

_PMI_END
