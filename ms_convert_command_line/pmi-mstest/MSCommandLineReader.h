/*
 *  Copyright (C) 2017 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author  : Yashpalsinh Gohil (yash.gohiil@gmail.com)
 */

#ifndef MSCOMMANDLINEREADER_H
#define MSCOMMANDLINEREADER_H

#include "MSTestCommandLineParser.h"
#include "common_errors.h"

class MSCommandLineReader
{
public:
    explicit MSCommandLineReader(MSTestCommandLineParser *parser);
    ~MSCommandLineReader();
    pmi::Err process();

private:
    MSTestCommandLineParser *m_parser;
};

#endif // MSCOMMANDLINEREADER_H
