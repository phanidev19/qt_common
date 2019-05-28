#include "ImageTileIterator.h"

#include "Tile.h"

_PMI_BEGIN

ImageTileIterator::ImageTileIterator(const QImage &img)
{
    m_image = img.convertToFormat(QImage::Format_ARGB32);
}

void ImageTileIterator::fetchTileData(const QRect &rc, QVector<double> *data)
{
    // if we are outside of qimage dimension, return default tile
    bool isIntersected = m_image.rect().intersects(rc);
    if (!isIntersected) {
        for (int i=0; i < Tile::WIDTH * Tile::HEIGHT;++i) {
            data->push_back(Tile::DEFAULT_TILE_VALUE);
        }
        return;
    }

    QRect intersected = m_image.rect().intersected(rc);
    int xEnd = intersected.x() + intersected.width();
    int yEnd = intersected.y() + intersected.height();

    Q_ASSERT(m_image.hasAlphaChannel());
    int numOfChannels = 4; // let's assume rgba image

    for (int y = rc.y(); y < yEnd; ++y) {
        uchar * scanLine = m_image.scanLine(y);
        for (int x = rc.x(); x < xEnd; ++x) {
            QRgb * pixel = reinterpret_cast<QRgb*>(scanLine + x * numOfChannels);
            qreal intensity = qGray(*pixel) / 255.0;
            data->push_back(intensity);
        }

        int missingCols = Tile::WIDTH - (xEnd - rc.x());
        if (missingCols > 0) {
            for (int x = xEnd; x < xEnd + missingCols; ++x) {
                data->push_back(Tile::DEFAULT_TILE_VALUE);
            }
        }
    }

    int missingRows = Tile::HEIGHT - (yEnd - rc.y());
    if (missingRows > 0){
        for (int y = yEnd; y < yEnd + missingRows; ++y){
            for (int x = 0; x < Tile::WIDTH; ++x) {
                data->push_back(Tile::DEFAULT_TILE_VALUE);
            }
        }
    }
}


_PMI_END

