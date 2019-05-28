/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef NONUNIFORM_TILE_MANAGER_H
#define NONUNIFORM_TILE_MANAGER_H

#include "NonUniformTileStoreBase.h"
#include "NonUniformTileStoreMemory.h"

#include "pmi_common_tiles_export.h"

#include <pmi_core_defs.h>

#include <QScopedPointer>

_PMI_BEGIN

#define PROFILE_TILE_FETCHING
#ifdef PROFILE_TILE_FETCHING

class TileFetchCounter
{
public:
    void increase() { m_count++; }
    void reset() { m_count = 0; }
    int value() const { return m_count; }

private:
    int m_count = 0;
};

#endif

template <class T>
class PMI_COMMON_TILES_EXPORT NonUniformTileManagerBase
{

public:
    explicit NonUniformTileManagerBase(NonUniformTileStoreBase<T> *store, bool ownStore = false)
        : m_store(store)
        , m_ownStore(ownStore)
    {
    }

    virtual ~NonUniformTileManagerBase()
    {
        if (m_ownStore) {
            delete m_store;
        }
    }

    NonUniformTileBase<T> loadTile(const QPoint &tilePos, NonUniformTileStoreType::ContentType type)
    {
        bool useCache = m_cache.contains(tilePos, type);

#ifdef PROFILE_TILE_FETCHING
        if (useCache) {
            m_cacheCounter.increase();
        } else {
            m_DbCounter.increase();
        }
#endif

        NonUniformTileBase<T> tile
            = useCache ? m_cache.loadTile(tilePos, type) : m_store->loadTile(tilePos, type);

        if (!useCache && (m_maxCacheItems > 0)) {
            if (m_cache.count(type) >= m_maxCacheItems) {
                m_cache.clear();
            }
            m_cache.saveTile(tile, type);
        }

        return tile;
    }

    void setCacheSize(int cacheSize) {
        m_maxCacheItems = cacheSize;
        if (cacheSize == 0) {
            m_cache.clear();
        }
    }

    int cacheSize() const { return m_maxCacheItems; }

#ifdef PROFILE_TILE_FETCHING
    void resetFetchCounters()
    {
        m_DbCounter.reset();
        m_cacheCounter.reset();
    }

    void fetchCounts(int *dbTileCount, int *cacheTileCount)
    {
        if (dbTileCount) {
            *dbTileCount = m_DbCounter.value();
        }

        if (cacheTileCount) {
            *cacheTileCount = m_cacheCounter.value();
        }
    }
#endif

    virtual NonUniformTileManagerBase<T> *clone() const
    {
        NonUniformTileStoreBase<T> *store = m_store->clone();

        NonUniformTileManagerBase<T> *clonedManager = new NonUniformTileManagerBase<T>(store, true);
        clonedManager->m_maxCacheItems = m_maxCacheItems;
        clonedManager->m_cache = m_cache;
        return clonedManager;
    }

    NonUniformTileStoreBase<T> *store() const { return m_store; }

private:
    Q_DISABLE_COPY(NonUniformTileManagerBase)
    NonUniformTileStoreBase<T> *m_store;
    bool m_ownStore;

    int m_maxCacheItems = 9;
    NonUniformTileStoreMemoryBase<T> m_cache;

#ifdef PROFILE_TILE_FETCHING
    TileFetchCounter m_cacheCounter;
    TileFetchCounter m_DbCounter;
#endif
};

typedef NonUniformTileManagerBase<point2dList> NonUniformTileManager;

typedef NonUniformTileManagerBase<QBitArray> NonUniformTileSelectionManager;

typedef NonUniformTileManagerBase<QVector<int>> NonUniformTileHillIndexManager;

template class PMI_COMMON_TILES_EXPORT NonUniformTileManagerBase<point2dList>;

template class PMI_COMMON_TILES_EXPORT NonUniformTileManagerBase<QBitArray>;

template class PMI_COMMON_TILES_EXPORT NonUniformTileManagerBase<QVector<int>>;

_PMI_END

#endif // NONUNIFORM_TILE_MANAGER_H