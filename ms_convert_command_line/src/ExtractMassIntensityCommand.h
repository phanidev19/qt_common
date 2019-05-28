/*
 *  Copyright (C) 2017 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author  : Yashpalsinh Gohil (yash.gohiil@gmail.com)
 */

#ifndef EXTRACTMASSINTENSITYCOMMAND_H
#define EXTRACTMASSINTENSITYCOMMAND_H

#include "MSReaderCommand.h"

class ExtractMassIntensityCommand : public MSReaderCommand
{
public:
    explicit ExtractMassIntensityCommand(PMCommandLineParser *parser);
    ~ExtractMassIntensityCommand();

    pmi::Err execute() override;
};

#endif // EXTRACTMASSINTENSITYCOMMAND_H
