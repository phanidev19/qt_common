#ifndef RANDOM_TILE_ITERATOR
#define RANDOM_TILE_ITERATOR

#include "pmi_core_defs.h"
#include "pmi_common_tiles_export.h"
#include "Tile.h"

_PMI_BEGIN

class TileManager;

class PMI_COMMON_TILES_EXPORT RandomTileIterator {

public:
    //! \brief Provide a way to access the elements of an aggregate object (TileManager's tiles)
    //!  without exposing its underlying representation.
    //! @param tileManager - manager encapsulating particular tile store @see TileStore interface
    RandomTileIterator(TileManager *tileManager);

    /*
    * @param posX - global tile position X
    * @param posY - global tile position Y
    * @param level - mip-map level
    */
    void moveTo(const QPoint &level, int posX, int posY);
    
    //! \brief return the last value of the call @see moveTo(..)
    double value() const;

    //! \brief Provides information about how many points are available for given position til the
    //! boundary of the tiles. Used for optimization purposes.
    int numContiguousCols(int posX) const;
    int numContiguousRows(int posY) const;

    //! Helper function for the caching strategy
    void setMainTile(const TileInfo &tileInfo);

    // TODO: better design would be to have various caching strategies implemented as external class
    void clearCache() { m_cache.clear(); }

    //! \brief provides information about last position that moveTo moved to
    int x() const { return m_pos.x(); };
    int y() const { return m_pos.y(); };

private:
    //! Caching strategy: when the cache is full, it removes the tiles
    //! that are NOT closest neighboring tiles
    int clearDistantTiles();

private:
    TileManager * m_tileManager; //TODO: SharedPointer
    double m_lastValue;

    int m_maxCacheItems = 9; 
    QHash<TileInfo, Tile> m_cache;
    
    TileInfo m_mainTileInfo;
    QPoint m_pos;
};

_PMI_END

#endif // RANDOM_TILE_ITERATOR