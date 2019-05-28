#include "TilePositionConverter.h"

#include "TileRange.h"

#include <QRectF>
#include <QtMath>

_PMI_BEGIN

TilePositionConverter::TilePositionConverter(const TileRange& tileRange) 
    : m_range(tileRange)
{
    m_currentLevel = QPoint(1, 1);
}

double TilePositionConverter::worldToLevelPositionX(double wx, const QPoint &level) const
{
    double stepX = m_range.levelStepX(level);
    double levelMinOffset = stepX * 0.5;
    double levelPositionX = (wx - (m_range.minX() + levelMinOffset )) / stepX;
    return levelPositionX;
}

double TilePositionConverter::worldToLevelPositionY(double wy, const QPoint &level) const
{
    double stepY = m_range.levelStepY(level);
    double levelMinOffset = stepY * 0.5;
    double levelPositionY = (wy - (m_range.minY() + levelMinOffset)) / stepY;
    return levelPositionY;
}

double TilePositionConverter::levelToWorldPositionX(int x, const QPoint &level) const
{
    double stepX = m_range.levelStepX(level);
    double levelMinOffset = stepX * 0.5;
    double worldPositionX = x * stepX + (m_range.minX() + levelMinOffset);
    return worldPositionX;
}

double TilePositionConverter::levelToWorldPositionY(int y, const QPoint &level) const
{
    double stepY = m_range.levelStepY(level);
    double levelMinOffset = stepY * 0.5;
    double worldPositionY = y * stepY + (m_range.minY() + levelMinOffset);
    return worldPositionY;

}

QRectF TilePositionConverter::worldToLevelRect(const QRectF &area, const QPoint &level) const
{
    qreal x1 = worldToLevelPositionX(area.left(), level);
    qreal x2 = worldToLevelPositionX(area.x() + area.width(), level);

    qreal y1 = worldToLevelPositionY(area.top(), level);
    qreal y2 = worldToLevelPositionY(area.y() + area.height(), level);

    return QRectF(x1, y1, x2 - x1, y2 - y1);
}

int TilePositionConverter::globalTileX(double mz) const
{
    const double mzStart = m_range.minX();
    const double mzStep = m_range.levelStepX(m_currentLevel); 
    const int tileX = qFloor((mz - mzStart) / mzStep);

    const double mzMin = mzAt(tileX);
    const double mzMax = mzAt(tileX + 1);

    
    if (mzMin <= mz && mz < mzMax) {
        return tileX;
    } else if (mz < mzMin) {
        return tileX - 1;
    } else if (mz >= mzMax) {
        return tileX + 1;
    }

    qWarning() << "Unexpected state for mz" << mz;
    return tileX;

}

int TilePositionConverter::globalTileY(int scanIndex)
{
    const int scanIndexStart = m_range.minY();
    const double scanIndexStep = m_range.levelStepY(m_currentLevel);
    const int tileY = (scanIndex - scanIndexStart) / scanIndexStep;

    //TODO: think if we need to do the correction

    return tileY;
}

double TilePositionConverter::mzAt(int globalTileX) const
{
    const double stepX = m_range.levelStepX(m_currentLevel);
    return m_range.minX() + globalTileX * stepX;
}

int TilePositionConverter::scanIndexAt(int globalTileY) const
{
    const double stepY = m_range.levelStepY(m_currentLevel);
    return m_range.minY() + globalTileY * stepY;
}

_PMI_END


