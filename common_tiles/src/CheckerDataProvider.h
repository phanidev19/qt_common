#ifndef CHECKER_DATA_PROVIDER_H
#define CHECKER_DATA_PROVIDER_H

#include "pmi_core_defs.h"
#include "TileDataProvider.h"

#include <QSize>
#include <QRect>
#include "pmi_common_tiles_export.h"

_PMI_BEGIN

class PMI_COMMON_TILES_EXPORT CheckerDataProvider : public TileDataProvider {

public:    
    CheckerDataProvider(const QSizeF &checkerSize, qreal offsetX, qreal offsetY);

    virtual void fetchTileData(const QRect& tileArea, QVector<double> *tileData);

    double value(double wx, double wy) const;

private:
    //TODO: find better place 
    // x position
    // column tile column
    int xInTile(int x, int column);
    int yInTile(int y, int row);

    

private:
    QSizeF m_checkerSize;
    qreal m_offsetX;
    qreal m_offsetY;
};

_PMI_END

#endif // CHECKER_DATA_PROVIDER_H