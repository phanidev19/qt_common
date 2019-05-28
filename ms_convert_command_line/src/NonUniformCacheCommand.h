/*
 *  Copyright (C) 2017 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author  : Yashpalsinh Gohil yash.gohiil@gmail.com
 */

#ifndef NONUNIFORMCACHECOMMAND_H
#define NONUNIFORMCACHECOMMAND_H

#include "MSReaderCommand.h"

class NonUniformCacheCommand : public MSReaderCommand
{
public:
    NonUniformCacheCommand(PMCommandLineParser *parser);
    pmi::Err execute();
};

#endif // NONUNIFORMCACHECOMMAND_H
