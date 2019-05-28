#include "TileManager.h"

#include "RandomTileIterator.h"
#include "TileRaster.h"
#include "TileStore.h"

#include <QRect>

#ifdef DEV_LOG_TILE_ACCESS
#include "CsvWriter.h"
#endif

_PMI_BEGIN

TileManager::TileManager(TileStore * store) : m_store(store)
{

}

TileManager::~TileManager()
{

}


int TileManager::tileColumns(int x, int width)
{
    int firstColumn = xToColumn(x);
    int lastColumn = xToColumn(x + width);
    return lastColumn - firstColumn + 1;
}


int TileManager::tileRows(int y, int height)
{
    int firstRow = yToRow(y);
    int lastRow = yToRow(y + height);
    return lastRow - firstRow + 1;
}

QVector<Tile> TileManager::fetchTiles(const QPoint &level, const QRect &area)
{
    QVector<Tile> tiles;
    
    int firstColumn = xToColumn(area.left());
    int lastColumn = xToColumn(area.x() + area.width());
    
    int firstRow = yToRow(area.top());
    int lastRow = yToRow(area.y() + area.height());

    for (int row = firstRow; row <= lastRow; ++row) {
        for (int column = firstColumn; column <= lastColumn; ++column){
            QPoint pos(column * Tile::WIDTH, row * Tile::HEIGHT);
            Tile t = m_store->loadTile(level, pos);
            tiles.push_back(t);
        }
    }

    return tiles;
}


Tile TileManager::fetchTile(const TileInfo &ti)
{
#ifdef DEV_LOG_TILE_ACCESS
    // log requests to tiles
    if (m_accessLogger.contains(ti)) {
        m_accessLogger[ti]++;
    } else {
        m_accessLogger[ti] = 1;
    }
#endif

    return m_store->loadTile(ti.level(), ti.pos());
}

Tile TileManager::fetchTile(const QPoint &level, int x, int y)
{
    QPoint pos(normalizeX(x), normalizeY(y));
    return m_store->loadTile(level, pos);
}

void pmi::TileManager::insertTiles(const QVector<Tile> &tiles)
{
    for (const Tile& t : tiles){
        if (!t.isNull()) {
            m_store->saveTile(t);
        }
    }
}


void TileManager::clearTiles()
{
    m_store->clear();
}


void TileManager::write(const QPoint &level, const QRect &tileArea, QVector<double> * buffer)
{
    Q_ASSERT(buffer != nullptr);
    Q_ASSERT(buffer->size() == tileArea.width() * tileArea.height());
    RandomTileIterator iterator(this);

    int rowsRemaining = tileArea.height();
    int bufferY = 0;
    int tileY = tileArea.y();
    while (rowsRemaining > 0) {

        qint32 bufferX = 0;
        int tileX = tileArea.x();

        qint32 columnsRemaining = tileArea.width();
        qint32 numContiguousTileRows = iterator.numContiguousRows(tileY);

        qint32 rowsToWork = qMin(numContiguousTileRows, rowsRemaining);

        while (columnsRemaining > 0) {
            qint32 numContiguousImageColumns = iterator.numContiguousCols(tileX);
            qint32 columnsToWork = qMin(numContiguousImageColumns, columnsRemaining);

            for (int rows = 0; rows < rowsToWork; ++rows){
                for (int cols = 0; cols < columnsToWork; ++cols){
                    // tile coordinate 
                    iterator.moveTo(level, tileX + cols, tileY + rows);

                    int bfx = bufferX + cols;
                    int bfy = bufferY + rows;

                    (*buffer)[bfy * tileArea.width() + bfx] = iterator.value();
                }
            }

            tileX += columnsToWork;
            bufferX += columnsToWork;
            columnsRemaining -= columnsToWork;
        }

        tileY += rowsToWork;
        bufferY += rowsToWork;
        rowsRemaining -= rowsToWork;
    }
}

void TileManager::write(const QPoint &level, TileRaster *raster)
{
    write(level, raster->tileArea(), raster->data());
}

int TileManager::normalizeX(int posX)
{
    return xToColumn(posX) * Tile::WIDTH;
}


int TileManager::normalizeY(int posY)
{
    return yToRow(posY) * Tile::HEIGHT;
}


int TileManager::columnCount(const QPoint &level) const
{
    return m_store->boundary(level).width() / Tile::WIDTH;
}


int TileManager::rowCount(const QPoint &level) const
{
    return m_store->boundary(level).height() / Tile::HEIGHT;
}

QRect TileManager::normalizeRect(const QRect &rc)
{
    // topleft of the area
    int left = normalizeX(rc.x());
    int top = normalizeY(rc.y());

    // bottom right of the area
    int right = normalizeX(rc.right()) + Tile::WIDTH - 1;
    int bottom = normalizeY(rc.bottom()) + Tile::HEIGHT - 1;

    return QRect(QPoint(left, top), QPoint(right, bottom));
}

#ifdef DEV_LOG_TILE_ACCESS
void TileManager::dumpLog(const QString &filePath)
{
    CsvWriter writer(filePath, "\r\n", ',');
    if (!writer.open()) {
        qWarning() << "Failed to open writer";
        return;
    }

    QStringList header = { "levelX", "levelY", "posX", "posY", "AccessCount" };
    writer.writeRow(header);

    for (const TileInfo &key : m_accessLogger.keys()) {
        QStringList row = {
            QString::number(key.level().x()),
            QString::number(key.level().y()),
            QString::number(key.pos().x()),
            QString::number(key.pos().y()),
            QString::number(m_accessLogger.value(key))
        };

        writer.writeRow(row);
    }
}
#endif

_PMI_END


