/*
* Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef TILE_LEVEL_SELECTOR_H
#define TILE_LEVEL_SELECTOR_H

#include "pmi_core_defs.h"
#include "pmi_common_tiles_export.h"
#include <QPoint>
#include <QVector>

_PMI_BEGIN

class PMI_COMMON_TILES_EXPORT TileLevelSelector {

public:    
    TileLevelSelector(const QVector<QPoint> &levels);

    QPoint selectLevelByScaleFactor(qreal scaleFactorX, qreal scaleFactorY) const;

    int findMaxXLevel() const;
    int findMaxYLevel() const;

    qreal convertLevelToZoom(int level);
    int convertZoomToLevel(qreal zoom, const QVector<qreal> &lookup) const;

    QVector<QPointF> convertLevelsToZoomLevels(const QVector<QPoint> &levels);

private:
    QVector<qreal> computeLookup(int maxLevel);

private:
    QVector<QPoint> m_levels;
    QVector<QPointF> m_availableZoomLevels;

    int m_maxLevelX;
    int m_maxLevelY;

    QVector<qreal> m_levelXLookup;
    QVector<qreal> m_levelYLookup;

    friend class TileLevelSelectorTest;

};

_PMI_END

#endif // TILE_LEVEL_SELECTOR_H