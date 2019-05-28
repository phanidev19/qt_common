#ifndef TILE_MANAGER_H
#define TILE_MANAGER_H

#include "pmi_common_tiles_export.h"
#include "pmi_core_defs.h"

#include "Tile.h"

#include <QHash>
#include <QVector>

//#define DEV_LOG_TILE_ACCESS

class QRect;

_PMI_BEGIN

class TileStore;
class TileRaster;

class PMI_COMMON_TILES_EXPORT TileManager {
public:
    explicit TileManager(TileStore * store);
    ~TileManager();

    static int tileColumns(int x, int width);
    static int tileRows(int y, int height);

    QVector<Tile> fetchTiles(const QPoint &level, const QRect &area);
    Tile fetchTile(const QPoint &level, int x, int y);
    Tile fetchTile(const TileInfo &ti);

    void insertTiles(const QVector<Tile> &tiles);
    void clearTiles();

    //! \brief Writes tile data from tile area to linear buffer
    // @param level - level of details from tiles 
    // @param area - in tile point coordinates
    // @param buffer - output buffer properly resized for area
    // caller is responsible to provide buffer of the given size
    void write(const QPoint &level, const QRect &area, QVector<double> * buffer);

    //! \brief overload for @see TileManager::write. MemoryRaster has to be pre-allocated correctly
    void write(const QPoint &level, TileRaster *raster);

    static int normalizeX(int posX);
    static int normalizeY(int posY);

    // converts global tile position to column index of the tile
    static int xToColumn(int posX) { return posX / Tile::WIDTH; }
    // converts global tile position to column index of the tile
    static int yToRow(int posY) { return posY / Tile::HEIGHT; }

    // returns the number of tiles for given levels on x-axis
    int columnCount(const QPoint &level) const;
    // returns the number of tiles for given levels on y-axis
    int rowCount(const QPoint &level) const;

    static QRect normalizeRect(const QRect &rc);

#ifdef DEV_LOG_TILE_ACCESS    
    void dumpLog(const QString &filePath);
    QHash<TileInfo, int> m_accessLogger;
#endif

private:
    TileStore * m_store;

};

_PMI_END

#endif