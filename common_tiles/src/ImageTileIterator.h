#ifndef IMAGE_TILE_ITERATOR_H
#define IMAGE_TILE_ITERATOR_H

#include "pmi_core_defs.h"
#include "pmi_common_tiles_export.h"

#include <QImage>

_PMI_BEGIN

class PMI_COMMON_TILES_EXPORT ImageTileIterator {

public:    
    ImageTileIterator(const QImage &img);
    
    void fetchTileData(const QRect &rc, QVector<double> * data);

private:
    QImage m_image;
    
};

_PMI_END

#endif // IMAGE_TILE_ITERATOR_H
