#ifndef TILE_DATA_PROVIDER_H
#define TILE_DATA_PROVIDER_H

#include "pmi_core_defs.h"

#include <QVector>
class QRect;

_PMI_BEGIN

class TileDataProvider {

public:    
    virtual void fetchTileData(const QRect& tileArea, QVector<double> *tileData) = 0;

};

_PMI_END

#endif // TILE_DATA_PROVIDER_H