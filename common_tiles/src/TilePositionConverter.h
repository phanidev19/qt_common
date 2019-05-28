#ifndef TILE_POSITION_CONVERTER_H
#define TILE_POSITION_CONVERTER_H

#include "pmi_core_defs.h"
#include "pmi_common_tiles_export.h"

#include "TileRange.h"

_PMI_BEGIN

class PMI_COMMON_TILES_EXPORT TilePositionConverter {

public:    
    TilePositionConverter(const TileRange& tileRange);

    void setCurrentLevel(const QPoint &level) { m_currentLevel = level; }
    QPoint currentLevel() const { return m_currentLevel; }

    // wx - world coordinate within tile range
    // level - specified level of tiles
    double worldToLevelPositionX(double wx, const QPoint &level) const;
    double worldToLevelPositionY(double wy, const QPoint &level) const;

    double levelToWorldPositionX(int x, const QPoint &level) const;
    double levelToWorldPositionY(int y, const QPoint &level) const;

    //! \brief Translates area specified in mz, scan index to tile coordinates
    QRectF worldToLevelRect(const QRectF &area, const QPoint &level) const;

    // new APIs are using setCurrentLevel, so set it properly before usage!!

    // converts mz value to global tile position x
    int globalTileX(double mz) const;
    // converts scan index to global tile position y
    int globalTileY(int scanIndex);
    
    // converts global tile position x to mz value
    double mzAt(int globalTileX) const;
    
    // converts global tile position y to scan index
    int scanIndexAt(int globalTileY) const;

    // range which this converter operates on
    TileRange range() const { return m_range; }

private:
    TileRange m_range;
    QPoint m_currentLevel;
};

_PMI_END

#endif // TILE_POSITION_CONVERTER_H