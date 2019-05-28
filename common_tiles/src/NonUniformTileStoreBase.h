/*
* Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef NON_UNIFORM_TILE_STORE_BASE_H
#define NON_UNIFORM_TILE_STORE_BASE_H

#include "pmi_core_defs.h"
#include "NonUniformTile.h"
#include "pmi_common_tiles_export.h"

class QPoint;

_PMI_BEGIN

class NonUniformTileStoreType {
public:
    //! @note ContentMS1Raw is profile data 
    enum ContentType { ContentMS1Raw, ContentMS1Centroided };
};

template <class T>
class NonUniformTileStoreBase : public NonUniformTileStoreType {
public:
    virtual ~NonUniformTileStoreBase() {}
    
    virtual bool saveTile(const NonUniformTileBase<T> &t, ContentType type) = 0;
    virtual const NonUniformTileBase<T> loadTile(const QPoint &pos, ContentType type) = 0;

    // returns true if the store contains the tile at the position, false otherwise
    virtual bool contains(const QPoint &pos, ContentType type) = 0;

    // start transaction
    virtual bool start() = 0;

    // end transaction
    virtual bool end() = 0;

    // clear cache
    virtual void clear() = 0;

    // support for partial tiles storage
    
    // start transaction for partial tiles
    virtual bool startPartial() = 0;
    // end transaction for partial tiles
    virtual bool endPartial() = 0;

    virtual bool savePartialTile(const NonUniformTileBase<T> &t, ContentType contentType, quint32 writePass) = 0;

    virtual bool initTilePartCache() = 0;
    virtual bool dropTilePartCache() = 0;

    virtual bool defragmentTiles(NonUniformTileStoreBase<T> *dstStore) = 0;

    virtual NonUniformTileStoreBase<T> *clone() const = 0;
};

_PMI_END

#endif // NON_UNIFORM_TILE_STORE_BASE_H
