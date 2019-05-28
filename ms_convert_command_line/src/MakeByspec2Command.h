/*
 *  Copyright (C) 2017 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author  : Yashpalsinh Gohil (yash.gohiil@gmail.com)
 */

#ifndef MAKEBYSPEC2COMMAND_H
#define MAKEBYSPEC2COMMAND_H

#include "MSReaderCommand.h"

class MakeByspec2Command : public MSReaderCommand
{
public:
    explicit MakeByspec2Command(PMCommandLineParser *parser);
    pmi::Err execute() override;
};

#endif // MAKEBYSPEC2COMMAND_H
