#ifndef TILE_STORE_H
#define TILE_STORE_H

#include "pmi_core_defs.h"
#include "Tile.h"
#include "pmi_common_tiles_export.h"

#include <QPoint>

_PMI_BEGIN

class PMI_COMMON_TILES_EXPORT TileStore {
public:
    virtual bool saveTile(const Tile &t) = 0;
    virtual Tile loadTile(const QPoint &level, const QPoint &pos) = 0;
    
    // returns true if the store contains the tile at the position, false otherwise
    virtual bool contains(const QPoint &level, const QPoint &pos) = 0;

    // returns the rect that tiles cover with data
    virtual QRect boundary(const QPoint &level) = 0;

    // return vector of available levels of detail in tile store
    virtual QVector<QPoint> availableLevels() = 0;

    // start transaction
    virtual bool start() = 0;
    
    // end transaction
    virtual bool end() = 0;

    // clear cache
    virtual void clear() = 0;

};

_PMI_END

#endif
