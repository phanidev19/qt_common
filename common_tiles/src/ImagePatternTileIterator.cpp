#include "ImagePatternTileIterator.h"

#include <QImage>

#include "Tile.h"

_PMI_BEGIN

ImagePatternTileIterator::ImagePatternTileIterator(const QImage &even, const QImage &odd)
{
    Q_ASSERT(even.width() == Tile::WIDTH);
    Q_ASSERT(even.height() == Tile::HEIGHT);

    even.convertToFormat(QImage::Format_ARGB32);
    odd.convertToFormat(QImage::Format_ARGB32);

    m_even = fromImage(even);
    m_odd = fromImage(odd);
}

void ImagePatternTileIterator::fetchTileData(const QRect &rc, QVector<double> *data)
{
    int tileColumn = rc.x() / Tile::WIDTH;
    int tileRow = rc.y() / Tile::HEIGHT;

    bool columnIsEven = (tileColumn % 2 == 0);
    bool rowIsEven = (tileRow % 2 == 0);

    data->resize(Tile::WIDTH * Tile::HEIGHT);

    if (columnIsEven)
    {
        if (rowIsEven){
            std::copy(m_even.begin(), m_even.end(), data->begin());
        } else {
            std::copy(m_odd.begin(), m_odd.end(), data->begin());
        }
    } else {
        if (rowIsEven){
            std::copy(m_odd.begin(), m_odd.end(), data->begin());
        } else {
            std::copy(m_even.begin(), m_even.end(), data->begin());
        }
    }
}

QVector<double> ImagePatternTileIterator::fromImage(const QImage &in)
{
    QVector<double> tileData;
    int numOfChannels = 4;
    for (int y = 0; y < Tile::HEIGHT; ++y){
        const uchar * scanLine = in.scanLine(y);
        for (int x = 0; x < Tile::WIDTH; ++x) {
            const QRgb * pixel = reinterpret_cast<const QRgb*>(scanLine + x * numOfChannels);
            qreal intensity = qGray(*pixel) / 255.0;
            tileData.push_back(intensity);
        }
    }

    return tileData;
}





_PMI_END

