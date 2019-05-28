/*
 *  Copyright (C) 2017 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author  : Yashpalsinh Gohil yash.gohiil@gmail.com
 */

#include "NonUniformCacheCommand.h"

#include <MSDataNonUniformGenerator.h>

#include <QElapsedTimer>
#include <QFileInfo>

NonUniformCacheCommand::NonUniformCacheCommand(PMCommandLineParser *parser)
    : MSReaderCommand(parser)
{
}

pmi::Err NonUniformCacheCommand::execute()
{
    if (!m_parser->validateAndParseForNonUniformCacheCommand()) {
        return pmi::kBadParameterError;
    }

    pmi::Err e = pmi::kNoErr;

    QElapsedTimer et;
    et.start();

    QString dstFilePath;
    e = pmi::MSDataNonUniformGenerator::defaultCacheLocation(m_parser->sourceFile(), &dstFilePath); ree;

    pmi::MSDataNonUniformGenerator generator(m_parser->sourceFile());
    generator.setOutputFilePath(dstFilePath);

    if (QFileInfo(dstFilePath).exists()) {
        qWarning() << "Destination file" << dstFilePath << "exists! Cache generation stopped!";
        e = pmi::kError; ree;
    }

    generator.setDoCentroiding(m_parser->doCentroiding());
    e = generator.generate(); ree;

    qInfo() << "NonUniform cache created in" << et.elapsed() << "ms";

    return e;
}
