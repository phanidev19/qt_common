/*
* Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef NON_UNIFORM_TILE_STORE_MEMORY_H
#define NON_UNIFORM_TILE_STORE_MEMORY_H

#include "pmi_core_defs.h"

#include "NonUniformTileStoreBase.h"
#include "NonUniformTileBase.h"

#include "pmi_common_tiles_export.h"

#include <QDebug>
#include <QHash>
#include <QBitArray>

inline uint qHash(const QPoint &pos, uint seed)
{
    QtPrivate::QHashCombine hash;
    seed = hash(seed, pos.x());
    seed = hash(seed, pos.y());
    return seed;
}

_PMI_BEGIN

template<class T>
class NonUniformTileStoreMemoryBase : public NonUniformTileStoreBase<T> {

public:
    NonUniformTileStoreMemoryBase() = default;
    ~NonUniformTileStoreMemoryBase() override { }


    bool saveTile(const NonUniformTileBase<T> &t, ContentType type) override {
        QHash <QPoint, NonUniformTileBase<T>> &backend = contentTypeToStorage(type);
        if (backend.contains(t.position())) {
            return false;
        }

        backend[t.position()] = t;
        return true;
    }

    const NonUniformTileBase<T> loadTile(const QPoint &pos, ContentType type) override {
        QHash <QPoint, NonUniformTileBase<T>> &backend = contentTypeToStorage(type);
        return backend.value(pos);
    }

    NonUniformTileBase<T> &tile(const QPoint &pos, ContentType type) {
        QHash <QPoint, NonUniformTileBase<T>> &backend = contentTypeToStorage(type);
        // mark the tile dirty
        return backend[pos];
    }

    bool contains(const QPoint &pos, ContentType type) override {
        QHash <QPoint, NonUniformTileBase<T>> &backend = contentTypeToStorage(type);
        return backend.contains(pos);
    }

    bool start() override { return true; } // this is useful for db
    bool end() override { return true; }
    
    //! \brief so far used only in sqlite, for testing we can implement this one too
    bool startPartial() override {
        throw std::logic_error("The method or operation is not implemented.");
    }
    bool endPartial() override {
        throw std::logic_error("The method or operation is not implemented.");
    }
    
    bool savePartialTile(const NonUniformTileBase<T> &t, ContentType contentType, quint32 writePass) override {
        throw std::logic_error("The method or operation is not implemented.");
    }

    bool initTilePartCache() override {
        throw std::logic_error("The method or operation is not implemented.");
    }
    
    bool dropTilePartCache() override {
        throw std::logic_error("The method or operation is not implemented.");
    }
    
    bool defragmentTiles(NonUniformTileStoreBase<T> * dstStore) override {
        throw std::logic_error("The method or operation is not implemented.");
    }

    void clear() override {
        m_ms1Raw.clear();
        m_ms1Centroided.clear();
    }

    //! \brief 
    // @param pos - tile index position
    void append(const QPoint &pos, ContentType type, const T &data) {
        QHash <QPoint, NonUniformTile> &backend = contentTypeToStorage(type);

        if (!backend.contains(pos)) {
            NonUniformTileBase<T> newTile;
            newTile.setPosition(pos);
            newTile.append(data);

            backend.insert(pos, newTile);
        } else {
            backend[pos].append(data);
        }
    }

    //! \brief stores the tiles to some bigger store, e.g. disk or database
    //! TODO: this can fail due to full disk etc., proper error handling
    // TODO: bad design with ContentType and two "caches"
    bool flush(NonUniformTileStoreBase<T> * store, ContentType type, quint32 writePass) {
        QHash <QPoint, NonUniformTileBase<T>> &backend = contentTypeToStorage(type);

        if (!store->startPartial()) {
            qWarning() << "Failed to start transaction!";
        }

        // store all the partial tiles to some other store
        bool failDuringSave = false;
        for (auto it = backend.begin(), end = backend.end(); it != end; ++it) {
            NonUniformTileBase<T>& tile = *it;
            bool saved = store->savePartialTile(tile, type, writePass);
            if (!saved) {
                qWarning() << "Check storage! Cannot save part tiles!";
                failDuringSave = true;
                break;
            }
        }

        if (!failDuringSave) {
            if (!store->endPartial()) {
                qWarning() << "Failed to finish transaction!";
            }
        } else {
            // todo rollback
        }

        // TODO : make it pool-like, we can keep the tiles to avoid re-location and just fix their positions instead!
        backend.clear();
        return true;
    }

    // return the number of tiles stored for given content type
    int count(ContentType type) const {
        const QHash <QPoint, NonUniformTileBase<T>> &backend = contentTypeToStorage(type);
        return backend.size();
    }

    //! \brief returns the number of points stored in store (for all tiles of given type)
    quint32 pointCount(ContentType type) const {
        const QHash <QPoint, NonUniformTileBase<T>> &backend = contentTypeToStorage(type);
        const QHash <QPoint, NonUniformTileBase<T>>::const_iterator i;
        quint32 sum = 0;
        for (const NonUniformTileBase<T> &tile : backend) {
            sum += tile.pointCount();
        }
        return sum;
    }

    //! \brief returns the number of tiles stored in store
    quint32 tileCount(ContentType type) const {
        const QHash <QPoint, NonUniformTileBase<T>> &backend = contentTypeToStorage(type);
        return backend.size();
    }

    NonUniformTileStoreBase<T> *clone() const override {
        return new NonUniformTileStoreMemoryBase(*this);
    }

private:
    QHash <QPoint, NonUniformTileBase<T>> & contentTypeToStorage(ContentType type) {
        return const_cast<QHash <QPoint, NonUniformTileBase<T>> &>(static_cast<const NonUniformTileStoreMemoryBase<T>*>(this)->contentTypeToStorage(type));
    }
    
    const QHash <QPoint, NonUniformTileBase<T>> & contentTypeToStorage(ContentType type) const {
        switch (type) {
        case ContentMS1Raw:
            return m_ms1Raw;
        case ContentMS1Centroided:
            return m_ms1Centroided;
        default:
            qWarning() << "Requested unknown content type:" << type << "Returning as if ContentMS1Raw was requested";
            return m_ms1Raw;
        }
    }

private:
    QHash<QPoint, NonUniformTileBase<T>> m_ms1Raw;
    QHash<QPoint, NonUniformTileBase<T>> m_ms1Centroided;
};

typedef NonUniformTileStoreMemoryBase<point2dList> NonUniformTileStoreMemory;

typedef NonUniformTileStoreMemoryBase<QBitArray> NonUniformTileSelectionStoreMemory;

typedef NonUniformTileStoreMemoryBase<QVector<int>> NonUniformTileHillIndexStoreMemory;

_PMI_END

#endif // NON_UNIFORM_TILE_STORE_MEMORY_H