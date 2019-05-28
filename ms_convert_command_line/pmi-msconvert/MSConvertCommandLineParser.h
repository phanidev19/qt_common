/*
 *  Copyright (C) 2018 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author  : Dmitry Pischenko (dmitry.pischenko@toplab.io)
 */

#ifndef MSCONVERTCOMMANDLINEPARSER_H
#define MSCONVERTCOMMANDLINEPARSER_H

#include "PMCommandLineParser.h"
#include "MSReaderTypes.h"

class MSConvertCommandLineParser : public PMCommandLineParser
{
    Q_DECLARE_TR_FUNCTIONS(MSConvertCommandLineParser)
public:
    MSConvertCommandLineParser();
};

#endif // MSCONVERTCOMMANDLINEPARSER_H
