/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/


#include "NonUniformTilesSerialization.h"

_PMI_BEGIN

QVector<int> NonUniformTilesSerialization::deserializeIndex(const QByteArray &index, qint32 pointCount)
{
    return deserializeVector<int>(index, pointCount);
}

QVector<double> NonUniformTilesSerialization::deserializeMz(const QByteArray &mz, qint32 pointCount)
{
    return deserializeVector<double>(mz, pointCount);
}

QVector<double> NonUniformTilesSerialization::deserializeIntensity(const QByteArray &intensity, qint32 pointCount)
{
    return deserializeVector<double>(intensity, pointCount);
}

QByteArray NonUniformTilesSerialization::serializeIndex(const QVector<int> &index)
{
    return serializeVector(index);
}

QByteArray NonUniformTilesSerialization::serializeMz(const QVector<double> &mz)
{
    return serializeVector(mz);
}

QByteArray NonUniformTilesSerialization::serializeIntensity(const QVector<double> &intensity)
{
    return serializeVector(intensity);
}


_PMI_END

