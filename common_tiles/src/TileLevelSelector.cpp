/*
* Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/


#include "TileLevelSelector.h"
#include "pmi_common_tiles_debug.h"
#include <algorithm>

_PMI_BEGIN

const int INVALID_LEVEL = -1;


TileLevelSelector::TileLevelSelector(const QVector<QPoint> &levels) :m_levels(levels)
{
    m_maxLevelX = findMaxXLevel();
    m_maxLevelY = findMaxYLevel();
    
    m_levelXLookup = computeLookup(m_maxLevelX);
    m_levelYLookup = computeLookup(m_maxLevelY);

    m_availableZoomLevels = convertLevelsToZoomLevels(levels);
    
    debugTiles() << "zooms" << m_availableZoomLevels;
}

QPoint TileLevelSelector::selectLevelByScaleFactor(qreal scaleFactorX, qreal scaleFactorY) const
{
    int levelX = convertZoomToLevel(scaleFactorX, m_levelXLookup);
    int levelY = convertZoomToLevel(scaleFactorY, m_levelYLookup);
    //TODO: what if the combination is not available

    return QPoint(levelX, levelY);
}

bool compareY(const QPointF& left, const QPointF& right){
    return left.y() < right.y();
}

bool compareX(const QPointF& left, const QPointF& right){
    return left.x() < right.x();
}

int TileLevelSelector::findMaxXLevel() const
{
    if (m_levels.isEmpty()) {
        return INVALID_LEVEL;
    }

    auto result = std::max_element(m_levels.begin(), m_levels.end(), compareX);
    return (*result).x();
}

int TileLevelSelector::findMaxYLevel() const
{
    if (m_levels.isEmpty()) {
        return INVALID_LEVEL;
    }

    auto result = std::max_element(m_levels.begin(), m_levels.end(), compareY);
    return (*result).y();
}

qreal TileLevelSelector::convertLevelToZoom(int level)
{
    qreal zoomLevel = 1 / pow(2, level - 1);
    return zoomLevel;
}


int TileLevelSelector::convertZoomToLevel(qreal zoom, const QVector<qreal> &lookup) const
{
    // compute level from given scale factor
    int pos;
    auto lowerBoundIter = std::lower_bound(lookup.begin(), lookup.end(), zoom);
    if (lowerBoundIter == lookup.end()) {
        pos = lookup.size() - 1;
    } else {
        pos = (lowerBoundIter - lookup.begin());
    }

    debugTiles() << "Mip-map level position for scale factor" << zoom << "at" << pos << "Respective zoom:" << lookup.at(pos);

    int selectedLevel = -1;
    if (pos > 0) {
        qreal diffA = qAbs(zoom - lookup.at(pos));
        qreal diffB = qAbs(zoom - lookup.at(pos - 1));
        if (diffA < diffB) {
            selectedLevel = lookup.size() - 1 - pos;
        } else {
            selectedLevel = lookup.size() - 1 - (pos - 1);
        }
    } else {
        selectedLevel = lookup.size() - 1;
    }

    // vector index to level
    selectedLevel += 1;

    return selectedLevel;
}

QVector<QPointF> TileLevelSelector::convertLevelsToZoomLevels(const QVector<QPoint> &levels)
{
    QVector<QPointF> result;
    for (int i = levels.size() - 1; i >= 0; --i) {
        QPoint level = levels.at(i);
        qreal zoomLevelX = convertLevelToZoom(level.x());
        qreal zoomLevelY = convertLevelToZoom(level.y());
        result.push_back(QPointF(zoomLevelX, zoomLevelY));
    }

    return result;
}

QVector<qreal> TileLevelSelector::computeLookup(int maxLevel)
{
    QVector<qreal> result;
    for (int level = maxLevel; level >= 1; --level) {
        qreal zoom = convertLevelToZoom(level);
        result.push_back(zoom);
    }
    return result;
}


_PMI_END

