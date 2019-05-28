/*
 *  Copyright (C) 2017 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author  : Yashpalsinh Gohil (yash.gohiil@gmail.com)
 */

#ifndef MS_READER_COMMAND_H
#define MS_READER_COMMAND_H

#include "PMCommandLineParser.h"
#include "common_errors.h"

class MSReaderCommand
{
    Q_DISABLE_COPY(MSReaderCommand)
public:
    explicit MSReaderCommand(PMCommandLineParser *parser);
    virtual ~MSReaderCommand();
    virtual pmi::Err execute();

protected:
    PMCommandLineParser *m_parser;
};

#endif // MS_READER_COMMAND_H