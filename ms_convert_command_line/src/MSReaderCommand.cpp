/*
 *  Copyright (C) 2017 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author  : Yashpalsinh Gohil (yash.gohiil@gmail.com)
 */

#include "MSReaderCommand.h"
#include "MSReader.h"

MSReaderCommand::MSReaderCommand(PMCommandLineParser *parser)
    : m_parser(parser)
{
}

MSReaderCommand::~MSReaderCommand()
{
    pmi::MSReader::Instance()->closeFile();
}

pmi::Err MSReaderCommand::execute()
{
    qWarning() << "Base command invoked";
    return pmi::kError;
}
