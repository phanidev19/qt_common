/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/


#ifndef NONUNIFORM_TILES_SERIALIZATION_H
#define NONUNIFORM_TILES_SERIALIZATION_H

#include "pmi_core_defs.h"

#include <QVector>
#include <QByteArray>
#include <QDataStream>
#include <QBuffer>

_PMI_BEGIN

class NonUniformTilesSerialization 
{

public:
    static QVector<int> deserializeIndex(const QByteArray &index, qint32 pointCount);
    static QVector<double> deserializeMz(const QByteArray &mz, qint32 pointCount);
    static QVector<double> deserializeIntensity(const QByteArray &intensity, qint32 pointCount);

    static QByteArray serializeIndex(const QVector<int> &index);
    static QByteArray serializeMz(const QVector<double> &mz);
    static QByteArray serializeIntensity(const QVector<double> &intensity);

    template <typename T>
    static QByteArray serializeVector(const QVector<T> &data)
    {
        QByteArray blob;
        QBuffer inBuffer(&blob);
        inBuffer.open(QIODevice::WriteOnly);

        QDataStream ds(&inBuffer);
        for (T item : data) {
            ds << item;
        }
        inBuffer.close();
        return blob;
    }

    template <typename T>
    static QVector<T> deserializeVector(const QByteArray &data, qint32 pointCount)
    {
        QDataStream ds(data);
        QVector<T> result;
        result.reserve(pointCount);
        while (!ds.atEnd()) {
            T item;
            ds >> item;
            result.push_back(item);
        }

        return result;
    }
};

_PMI_END

#endif // NONUNIFORM_TILES_SERIALIZATION_H