/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef MSDATA_NON_UNIFORM_GENERATOR_H
#define MSDATA_NON_UNIFORM_GENERATOR_H

#include "pmi_common_ms_export.h"

#include <common_errors.h>
#include <pmi_core_defs.h>

#include "ProgressBarInterface.h"

#include <QScopedPointer>
#include <QString>

_PMI_BEGIN

class CacheFileManagerInterface;
class MSDataNonUniformGeneratorPrivate;

//! \brief generates the NonUniform cache for provided MS vendor files
class PMI_COMMON_MS_EXPORT MSDataNonUniformGenerator
{
public:
    MSDataNonUniformGenerator(const QString &vendorFilePath);

    ~MSDataNonUniformGenerator();

    //! \brief Set the cache file path
    void setOutputFilePath(const QString &filePath);

    //! \brief file path for generated cache
    QString outputFilePath() const;

    //! \brief Set to true if the destination cache should be created from centroided data
    // By default profile cache is created
    void setDoCentroiding(bool on);

    //! \brief executes the generation of the NonUniform cache
    //
    // After you provide particular settings of the generator, you can execute
    // the generation of the cache
    //
    // @param progressBar - currently unused
    Err generate(QSharedPointer<ProgressBarInterface> progressBar = NoProgress);

    //! \brief provides default cache file path using @see CacheFileManager
    static Err defaultCacheLocation(const QString &vendorFilePath, QString *cacheFilePath);

private:
    Q_DISABLE_COPY(MSDataNonUniformGenerator)
    QScopedPointer<MSDataNonUniformGeneratorPrivate> d;
};

_PMI_END

#endif // MSDATA_NON_UNIFORM_GENERATOR_H
