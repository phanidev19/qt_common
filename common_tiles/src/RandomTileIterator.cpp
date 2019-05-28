
#include "RandomTileIterator.h"
#include "TileManager.h"
#include "pmi_common_tiles_debug.h"



_PMI_BEGIN

RandomTileIterator::RandomTileIterator(TileManager * tileManager) 
    : m_tileManager(tileManager),
      m_lastValue(Tile::DEFAULT_TILE_VALUE),
      m_pos(-1, -1)

{
}

void RandomTileIterator::moveTo(const QPoint &level, int posX, int posY)
{
    m_pos.setX(posX);
    m_pos.setY(posY);

    int tileY = TileManager::normalizeY(posY);
    int tileX = TileManager::normalizeX(posX);
    
    const TileInfo currentTi(level, QPoint(tileX, tileY));
    bool useCache = m_cache.contains(currentTi);
    
    Tile t = useCache ? m_cache.value(currentTi) : m_tileManager->fetchTile(currentTi);
    if (t.isNull()) {
        //qWarning() << "Tile is not present: " << TileInfo(level, QPoint(tileX, tileY)) << "pos:" << posX << posY;
        m_lastValue = Tile::DEFAULT_TILE_VALUE;
    } else {
        int offsetX = posX - tileX;
        int offsetY = posY - tileY;

        m_lastValue = t.value(offsetX, offsetY);
    }

    // cache fetched tile
    if (!useCache) {
        if (m_cache.size() >= m_maxCacheItems) {
            int removed = clearDistantTiles();
            
            if (removed == 0 || removed == -1) {
                m_cache.clear();
            }

            if (removed == 0) {
                warningTiles() << "Failed to clean-up cache: Removed" << removed
                    << "items! Wiping completely!";
            }
        }
        m_cache.insert(currentTi, t);
    }
}

double RandomTileIterator::value() const
{
    return m_lastValue;
}


int RandomTileIterator::numContiguousCols(int posX) const
{
    int tileX = TileManager::normalizeX(posX);
    return Tile::WIDTH - (posX - tileX);
}

int RandomTileIterator::numContiguousRows(int posY) const
{
    int tileY = TileManager::normalizeY(posY);
    return Tile::HEIGHT - (posY - tileY);
}


void RandomTileIterator::setMainTile(const TileInfo &tileInfo)
{
    m_mainTileInfo = tileInfo;
}

int RandomTileIterator::clearDistantTiles()
{
    int removed = 0;
    if (!m_mainTileInfo.isValid()) {
        return -1; // signal invalid settings with -1 for this strategy
    }

    int mainTileCol = TileManager::xToColumn(m_mainTileInfo.pos().x());
    int mainTileRow = TileManager::yToRow(m_mainTileInfo.pos().y());

    for (const TileInfo &key : m_cache.keys()) {
        Tile t = m_cache.value(key);
        const TileInfo &other = t.tileInfo();
        // check level, has to be same
        if (other.level() != m_mainTileInfo.level()) {
            warningTiles() << "in-effective for multi-level iteration!";
            removed += m_cache.remove(key);
        } else {
            // compute vertical and horizontal distance
            const QPoint &otherPos = other.pos();

            int columnDist = qAbs(TileManager::xToColumn(otherPos.x()) - mainTileCol);
            int rowDist = qAbs(TileManager::yToRow(otherPos.y()) - mainTileRow);
            if (columnDist > 1 || rowDist > 1) {
                // good bye
                removed += m_cache.remove(key);
            }
        }
    }

    return removed;
}

_PMI_END


