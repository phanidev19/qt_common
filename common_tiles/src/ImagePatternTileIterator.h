#ifndef IMAGE_PATTERN_TILE_ITERATOR_H
#define IMAGE_PATTERN_TILE_ITERATOR_H

#include "pmi_core_defs.h"
#include "pmi_common_tiles_export.h"

class QImage;
class QRect;

#include <QVector>

_PMI_BEGIN

class PMI_COMMON_TILES_EXPORT ImagePatternTileIterator {

public:    
    // pass two pattern, one is on even tile positions, the other one on odd, dimension is 64x64
    ImagePatternTileIterator(const QImage& even, const QImage& odd);

    void fetchTileData(const QRect &rc, QVector<double> *data);

private:
    QVector<double> fromImage(const QImage &in);

private:
    QVector<double> m_even;
    QVector<double> m_odd;
};

_PMI_END

#endif // IMAGE_PATTERN_TILE_ITERATOR_H
