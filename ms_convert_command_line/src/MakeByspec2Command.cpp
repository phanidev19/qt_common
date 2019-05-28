/*
 *  Copyright (C) 2017 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author  : Yashpalsinh Gohil (yash.gohiil@gmail.com)
 */

#include "MakeByspec2Command.h"
#include "MSReaderByspec.h"

MakeByspec2Command::MakeByspec2Command(PMCommandLineParser *parser)
    : MSReaderCommand(parser)
{
}

pmi::Err MakeByspec2Command::execute()
{
    if (!m_parser->validateAndParseForMakeByspec2Command()) {
        return pmi::kBadParameterError;
    }

    return pmi::msreader::makeByspec(m_parser->sourceFile(), m_parser->outputPath(),
                                     m_parser->msConvertOptions(), pmi::CentroidOptions(),
                                     pmi::msreader::IonMobilityOptions(), nullptr);
}
