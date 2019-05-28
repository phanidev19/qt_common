/*
 *  Copyright (C) 2017 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author  : Yashpalsinh Gohil (yash.gohiil@gmail.com)
 */

#include "MakeByspec2PmiCommand.h"
#include "MSWriterByspec2.h"

MakeByspec2PmiCommand::MakeByspec2PmiCommand(PMCommandLineParser *parser)
    : MSReaderCommand(parser)
{
}

pmi::Err MakeByspec2PmiCommand::execute()
{
    if (!m_parser->validateAndParseForMakeByspec2Command()) {
        return pmi::kBadParameterError;
    }

    pmi::MSWriterByspec2 msWriter;
    return msWriter.makeByspec2(m_parser->sourceFile(), m_parser->outputPath(),
                                m_parser->msConvertOptions(), m_parser->doCentroiding(),
                                m_parser->ionMobilityOptions(), qApp->arguments());
}
