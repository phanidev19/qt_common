#ifndef TILE_STORE_MEMORY_H
#define TILE_STORE_MEMORY_H

#include "pmi_core_defs.h"

#include <QHash>
#include "Tile.h"
#include "TileStore.h"

_PMI_BEGIN

class PMI_COMMON_TILES_EXPORT TileStoreMemory : public TileStore {

public:    
    TileStoreMemory(){}

    virtual bool saveTile(const Tile &t) override;
    virtual Tile loadTile(const QPoint &level,const QPoint &pos) override;

    virtual bool contains(const QPoint &level, const QPoint &pos) override;

    // TileStore interface
    QRect boundary(const QPoint &level);
    QVector<QPoint> availableLevels();
    virtual bool start() { return true; } //TODO: naive
    virtual bool end() { return true; } //TODO: naive

    virtual void clear() override;

private:
    QHash<TileInfo, Tile> m_backend;
};

_PMI_END

#endif // TILE_STORE_MEMORY_H
