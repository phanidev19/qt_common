/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "NonUniformTileIntensityIndex.h"

#include "CsvWriter.h"

#include <QHash>
#include <QMultiMap>
#include <QVector>

#include "pmi_common_tiles_debug.h"

inline uint qHash(const QPoint &pos, uint seed)
{
    QtPrivate::QHashCombine hash;
    seed = hash(static_cast<uint>(pos.x()), seed);
    seed = hash(static_cast<uint>(pos.y()), seed);
    return seed;
}

_PMI_BEGIN

class Q_DECL_HIDDEN NonUniformTileIntensityIndex::Private
{
public:
    Private() {}

    QVector<IntensityIndexEntry *> entries;

    QHash<QPoint, IntensityIndexEntry *> indexByTilePos;
    QMultiMap<double, IntensityIndexEntry *> indexByIntensity;
};

NonUniformTileIntensityIndex::NonUniformTileIntensityIndex()
    : d(new Private)
{
}

NonUniformTileIntensityIndex::~NonUniformTileIntensityIndex()
{
    clear();
}

void NonUniformTileIntensityIndex::insertIndexEntry(IntensityIndexEntry *entry)
{
    Q_ASSERT(entry);

    d->entries.push_back(entry);
    d->indexByTilePos.insert(entry->pos.tilePos, entry);
    d->indexByIntensity.insert(entry->intensity, entry);
}

QList<IntensityIndexEntry *> NonUniformTileIntensityIndex::topIntensityEntries()
{
    return d->indexByIntensity.values(topIntensity());
}

void NonUniformTileIntensityIndex::clear()
{
    d->indexByIntensity.clear();
    d->indexByTilePos.clear();

    qDeleteAll(d->entries);
    d->entries.clear();
}

double NonUniformTileIntensityIndex::topIntensity() const
{
    return d->indexByIntensity.lastKey();
}

void NonUniformTileIntensityIndex::dumpToCsvFile(const QString &filePath) const
{
    // dump the index 
    CsvWriter writer(filePath);
    if (!writer.open()) {
        warningTiles() << "Failed to open CSV writer!";
        return;
    }

    for (const IntensityIndexEntry *item : indexEntries()) {
        QString tileXStr = QString::number(item->pos.tilePos.x());
        QString tileYStr = QString::number(item->pos.tilePos.y());
        QString intensityStr = QString::number(item->intensity, 'f', 15);
        bool ok = writer.writeRow({ tileXStr, tileYStr, intensityStr });
        if (!ok) {
            return;
        }
    }
}

QVector<IntensityIndexEntry *> NonUniformTileIntensityIndex::indexEntries() const
{
    return d->entries;
}

void NonUniformTileIntensityIndex::updateIntensity(const IntensityIndexEntry &updatedEntryContent)
{
    IntensityIndexEntry *entry = d->indexByTilePos.value(updatedEntryContent.pos.tilePos);

    // store old intensity, we need to update index
    double oldIntensity = entry->intensity;
    entry->intensity = updatedEntryContent.intensity;

    // update info used to locate the maxima within tile
    entry->pos.scanIndex = updatedEntryContent.pos.scanIndex;
    entry->pos.internalIndex = updatedEntryContent.pos.internalIndex;

    // update index
    d->indexByIntensity.remove(oldIntensity, entry);
    d->indexByIntensity.insert(updatedEntryContent.intensity, entry);
}

_PMI_END
