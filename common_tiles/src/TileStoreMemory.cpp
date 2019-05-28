#include "TileStoreMemory.h"
#include "Tile.h"

#include <QPoint>
#include <QRect>

_PMI_BEGIN

bool TileStoreMemory::saveTile(const Tile &t)
{
    
    if (m_backend.contains(t.tileInfo())) {
        // we do not store, because we are already there!
        return false;
    }
        
    m_backend[t.tileInfo()] = t;
    return true;
}



Tile TileStoreMemory::loadTile(const QPoint &level, const QPoint &pos)
{
    return m_backend.value(TileInfo(level, pos));
}

bool TileStoreMemory::contains(const QPoint &level, const QPoint &pos)
{
    return m_backend.contains(TileInfo(level, pos));
}

QRect TileStoreMemory::boundary(const QPoint &level)
{
    //TODO: not implemented!
    Q_UNUSED(level);
    return QRect();
}

QVector<QPoint> TileStoreMemory::availableLevels()
{
    return QVector<QPoint>();
}

void TileStoreMemory::clear()
{
    m_backend.clear();
}

_PMI_END


