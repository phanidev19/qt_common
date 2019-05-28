/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "NonUniformTileDevice.h"

_PMI_BEGIN

class Q_DECL_HIDDEN NonUniformTileDevice::Private
{
public:
    Private(NonUniformTileManager *tm, const NonUniformTileRange &r,
            NonUniformTileStore::ContentType cnt)
        : range(r)
        , tileManager(tm)
        , content(cnt)
    {
    }

    NonUniformTileRange range;
    NonUniformTileManager *tileManager = nullptr;
    NonUniformTileStore::ContentType content = NonUniformTileStore::ContentMS1Centroided;
};

NonUniformTileDevice::NonUniformTileDevice(NonUniformTileManager *tileManager,
                                           const NonUniformTileRange &range,
                                           NonUniformTileStore::ContentType content)
    : d(new Private(tileManager, range, content))
{
}

NonUniformTileDevice::NonUniformTileDevice()
    : d(new Private(nullptr, NonUniformTileRange(), NonUniformTileStore::ContentMS1Centroided))
{
}

NonUniformTileDevice::~NonUniformTileDevice()
{
}

NonUniformTileRange NonUniformTileDevice::range() const
{
    return d->range;
}

NonUniformTileStore::ContentType NonUniformTileDevice::content() const
{
    return d->content;
}

NonUniformTileManager *NonUniformTileDevice::tileManager()
{
    return d->tileManager;
}

bool NonUniformTileDevice::doCentroiding() const
{
    return d->content == NonUniformTileStore::ContentMS1Centroided;
}

bool NonUniformTileDevice::isNull() const
{
    return d->tileManager == nullptr;
}

RandomNonUniformTileIterator NonUniformTileDevice::createRandomIterator()
{
    return RandomNonUniformTileIterator(d->tileManager, d->range, doCentroiding());
}

_PMI_END
