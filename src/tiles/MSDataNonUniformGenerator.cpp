/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "MSDataNonUniformGenerator.h"

#include "MSReader.h"
#include "pmi_common_ms_debug.h"

#include "CacheFileManager.h"
#include "NonUniformTileStore.h"

#include <QDir>
#include <QElapsedTimer>

_PMI_BEGIN

using namespace msreader;

class MSDataNonUniformGeneratorPrivate
{
public:
    MSDataNonUniformGeneratorPrivate(const QString &venFilePath)
        : vendorFilePath(venFilePath)
    {
    }

    bool doCentroiding = false;
    QString vendorFilePath;

    QString outputFilePath;
};

MSDataNonUniformGenerator::MSDataNonUniformGenerator(const QString &vendorFilePath)
    : d(new MSDataNonUniformGeneratorPrivate(vendorFilePath))
{
    Q_ASSERT(QFileInfo(vendorFilePath).exists());
}

MSDataNonUniformGenerator::~MSDataNonUniformGenerator()
{
}

void MSDataNonUniformGenerator::setOutputFilePath(const QString &filePath)
{
    d->outputFilePath = filePath;
}

QString MSDataNonUniformGenerator::outputFilePath() const
{
    return d->outputFilePath;
}

Err MSDataNonUniformGenerator::generate(QSharedPointer<ProgressBarInterface> progressBar)
{
    Err e = kNoErr;
    if (d->outputFilePath.isEmpty()) {
        warningMs() << "Destination file path for cache not set, using default location!";
        e = MSDataNonUniformGenerator::defaultCacheLocation(d->vendorFilePath, &d->outputFilePath); ree;
    }

    MSReader *reader = MSReader::Instance();
    e = reader->openFile(d->vendorFilePath); ree;

    QList<ScanInfoWrapper> scanInfo;
    e = reader->getScanInfoListAtLevel(1, &scanInfo); ree;
    
    MSDataNonUniformAdapter adapter(d->outputFilePath);

    NonUniformTileStore::ContentType content = d->doCentroiding
        ? NonUniformTileStore::ContentMS1Centroided
        : NonUniformTileStore::ContentMS1Raw; // Profile

    adapter.setContentType(content);

    bool ok;
    NonUniformTileRange range = adapter.createRange(reader, d->doCentroiding, scanInfo.size(), &ok);

    if (!ok) {
        warningMs() << "Error retrieving range";
        e = kError; ree;
    }

    e = adapter.createNonUniformTiles(reader, range, scanInfo, progressBar); ree;

    return e;
}

Err MSDataNonUniformGenerator::defaultCacheLocation(const QString &vendorFilePath,
                                                    QString *cacheFilePath)
{
    Q_ASSERT(cacheFilePath);
    CacheFileManager manager;
    manager.setSourcePath(vendorFilePath);
    Err e = manager.findOrCreateCachePath(MSDataNonUniformAdapter::formatSuffix(),
                                          cacheFilePath); ree;
    return e;
}

void MSDataNonUniformGenerator::setDoCentroiding(bool on)
{
    d->doCentroiding = on;
}

_PMI_END
