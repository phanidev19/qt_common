#ifndef RANDOM_BILINEAR_TILE_ITERATOR
#define RANDOM_BILINEAR_TILE_ITERATOR

#include "pmi_core_defs.h"
#include <QScopedPointer>
#include <QPoint>
#include "pmi_common_tiles_export.h"

_PMI_BEGIN

class RandomTileIterator;
class TileManager;

class PMI_COMMON_TILES_EXPORT RandomBilinearTileIterator {

public:    
    RandomBilinearTileIterator(TileManager * manager);
    ~RandomBilinearTileIterator();

    void moveTo(const QPoint &level, qreal x, qreal y);

    double value() const;

private:
    TileManager * m_tileManager;
    QScopedPointer<RandomTileIterator>  m_randomIterator;
    double m_lastValue;


};

_PMI_END

#endif // RANDOM_BILINEAR_TILE_ITERATOR