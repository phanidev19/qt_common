#ifndef TILE_TEST_UTILS_H
#define TILE_TEST_UTILS_H

#include "pmi_core_defs.h"

_PMI_BEGIN

namespace TestUtils {

    
    // value(x,y) is the respective index of the item, e.g. (0,0) has value 0; (0,1) has value 1; (63,63) has value 4095
    Tile createIndexedTile(const QPoint &level, const QPoint &pos) {
        // create tile with values 0..4095
        QVector<double> data;
        for (int y = 0; y < Tile::HEIGHT; y++) {
            for (int x = 0; x < Tile::WIDTH; x++) {
                data.push_back(y * Tile::WIDTH + x); // value computed from position - index
            }
        }

        return Tile(TileInfo(level, pos), data);
    }

    Tile createIndexedTile(const TileInfo& tileInfo) {
        return createIndexedTile(tileInfo.level(), tileInfo.pos());
    }


};

_PMI_END

#endif // TILE_TEST_UTILS_H