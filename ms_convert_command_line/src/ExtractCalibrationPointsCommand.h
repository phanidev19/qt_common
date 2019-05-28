/*
 *  Copyright (C) 2017 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author  : Yashpalsinh Gohil (yash.gohiil@gmail.com)
 */

#ifndef EXTRACTCALIBRATIONPOINTSCOMMAND_H
#define EXTRACTCALIBRATIONPOINTSCOMMAND_H

#include "MSReaderCommand.h"

class ExtractCalibrationPointsCommand : public MSReaderCommand
{
public:
    explicit ExtractCalibrationPointsCommand(PMCommandLineParser *parser);
    pmi::Err execute() override;
};

#endif // EXTRACTCALIBRATIONPOINTSCOMMAND_H
