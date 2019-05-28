/*
 * Copyright (C) 2017-2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author  : Sam Bogar (sbogar@proteinmetrics.com)
 */
#ifndef MSCONVERTCOMPARE_H
#define MSCONVERTCOMPARE_H

#include "MSReaderCommand.h"
#include "PMCommandLineParser.h"

class ComparisonCommand : public MSReaderCommand
{
public:
    explicit ComparisonCommand(PMCommandLineParser *parser);
    ~ComparisonCommand();

    pmi::Err execute() override;
};

#endif // MSCONVERTCOMPARE_H