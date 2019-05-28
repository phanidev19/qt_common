#include "CheckerDataProvider.h"

#include "Tile.h"
#include "TileManager.h"

_PMI_BEGIN

CheckerDataProvider::CheckerDataProvider(const QSizeF &checkerSize, qreal offsetX, qreal offsetY)
    :   m_checkerSize(checkerSize),
        m_offsetX(offsetX),
        m_offsetY(offsetY)
{
}

void CheckerDataProvider::fetchTileData(const QRect& tileArea, QVector<double> *tileData)
{
    int tileColumn = tileArea.x() / Tile::WIDTH;
    int tileRow = tileArea.y() / Tile::HEIGHT;

    tileData->resize(Tile::WIDTH * Tile::HEIGHT);

    int xEnd = tileArea.x() + tileArea.width();
    int yEnd = tileArea.y() + tileArea.height();

    // TODO: abstract this iteration over tileData, it is copy-pasta in every TileDataProvider
    for (int y = tileArea.y(); y < yEnd; ++y) {
        for (int x = tileArea.x(); x < xEnd; ++x) {

            double result = value(x + m_offsetX, y + m_offsetY);

            int localX = xInTile(x, tileColumn);
            int localY = yInTile(y, tileRow);

            int bufferPos = localY * Tile::WIDTH + localX;
            (*tileData)[bufferPos] = result;
        }
    }
}

int CheckerDataProvider::xInTile(int x, int column)
{
    return x - column * Tile::WIDTH;
}


int CheckerDataProvider::yInTile(int y, int row)
{
    return y - row * Tile::HEIGHT;
}

double CheckerDataProvider::value(double wx, double wy) const
{
    int col = wx / m_checkerSize.width();
    int row = wy / m_checkerSize.height();

    bool columnIsEven = (col % 2 == 0);
    bool rowIsEven = (row % 2 == 0);

    const double HIGH = 1.0;
    const double LOW = 0.0;

    double result;
    if (columnIsEven)
    {
        if (rowIsEven) {
            result = HIGH;
        }
        else {
            result = LOW;
        }
    }
    else {
        if (rowIsEven) {
            result = LOW;
        }
        else {
            result = HIGH;
        }
    }

    return result;
}

_PMI_END


