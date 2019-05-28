/*
* Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "Tile.h"
#include <QBuffer>
#include <QDataStream>
#include <QFile>

_PMI_BEGIN

const int Tile::WIDTH = 64;
const int Tile::HEIGHT = 64;
const double Tile::DEFAULT_TILE_VALUE = 0.0;

QDebug operator<<(QDebug debug, const TileInfo &ti)
{
    debug.nospace() << "TileInfo(" << ti.level().x() << ", " << ti.level().y() << ", ";
    debug.nospace() << "QPoint(" << ti.pos().x() << "," << ti.pos().y() << "));";
    return debug.space();
}


bool saveDataToFile(const Tile& tile, const QString & filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    QTextStream out(&file);
    out.setRealNumberPrecision(15);

    for (int y = 0; y < Tile::HEIGHT; ++y) {
        for (int x = 0; x < Tile::WIDTH; ++x) {
            out << tile.data().at(y * Tile::WIDTH + x) << ";";
        }
        out << "\n";
    }

    out.flush();
    file.close();

    return true;
}

Tile::Tile()
{
}

Tile::Tile(const QPoint &level, const QPoint &pos, const QVector<double> &data)
    : m_tileInfo(level,pos),
      m_data(data)

{
}

Tile::Tile(const TileInfo &ti, const QVector<double> &data) 
    : m_tileInfo(ti), 
      m_data(data)
{

}

Tile::~Tile()
{

}

QByteArray Tile::serializedData(SerializeFormat format) const
{
    QByteArray blob;
    QBuffer inBuffer(&blob);
    inBuffer.open(QIODevice::WriteOnly);
    QDataStream ds(&inBuffer);

    const QVector<double> &tileData = data();
    for (double item : tileData) {
        ds << item;
    }
    inBuffer.close();

    if (format == COMPRESSED_BYTES){
        blob = qCompress(blob);
    }

    return blob;
}
//TODO: wrong API: exposed the member type! What if we want to use different format in the future, e.g. raw pointer array for effectivness
QVector<double> Tile::deserializedData(const QByteArray &input, SerializeFormat format /*= Tile::RAW_BYTES*/)
{
    QByteArray uncompressedData;
    if (format == COMPRESSED_BYTES){
        uncompressedData = qUncompress(input);
    } else {
        uncompressedData = input;
    }

    QBuffer out(&uncompressedData);
    out.open(QIODevice::ReadOnly);

    QDataStream ds(&out);

    QVector<double> result;
    const quint32 n = Tile::WIDTH * Tile::HEIGHT;
    result.reserve(n);
    for (quint32 i = 0; i < n; ++i) {
        double item;
        ds >> item;
        if (ds.status() != QDataStream::Ok) {
            result.clear();
            break;
        }
        result.push_back(item);
    }

    return result;
}


bool Tile::operator!=(const Tile &rhs) const
{
    return !(*this == rhs);
}

double Tile::value(int x, int y) const
{
    Q_ASSERT_X(x >= 0 && x < Tile::WIDTH && y >= 0 && y < Tile::HEIGHT, "Tile::value", "outside tile range");
    Q_ASSERT_X(!isNull(), "Tile::value", "Tile is null, value is requested");
    return m_data.at(y * Tile::WIDTH + x);
}

Tile Tile::difference(const Tile &t) const
{
    TileInfo ti = t.tileInfo();
    QVector<double> diffData;
    for (int y = 0; y < Tile::HEIGHT; ++y) {
        for (int x = 0; x < Tile::WIDTH; ++x) {
            const int pos = y * Tile::WIDTH + x;
            double diff = data().at(pos) - t.data().at(pos);
            diffData << diff;
        }
    }

    return Tile(ti, diffData);

}

QVector<QPoint> Tile::differentValues(const Tile &t) const
{
    QVector<QPoint> result;

    for (int y = 0; y < Tile::HEIGHT; ++y) {
        for (int x = 0; x < Tile::WIDTH; ++x) {
            int pos = y * Tile::WIDTH + x;
            if (data().at(pos) != t.data().at(pos)) {
                result << QPoint(x, y);
            }
        }
    }

    return result;
}

bool Tile::operator==(const Tile & rhs) const
{
    return (m_tileInfo == rhs.tileInfo() && m_data == rhs.data());
}

_PMI_END

