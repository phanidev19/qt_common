/*
 *  Copyright (C) 2017 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author  : Yashpalsinh Gohil (yash.gohiil@gmail.com)
 */

#ifndef MAKEBYSPEC2PMICOMMAND_H
#define MAKEBYSPEC2PMICOMMAND_H

#include "MSReaderCommand.h"

class MakeByspec2PmiCommand : public MSReaderCommand
{
public:
    explicit MakeByspec2PmiCommand(PMCommandLineParser *parser);
    pmi::Err execute() override;
};

#endif // MAKEBYSPEC2PMICOMMAND_H
