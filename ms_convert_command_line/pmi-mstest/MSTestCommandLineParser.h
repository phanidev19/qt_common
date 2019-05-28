/*
 *  Copyright (C) 2018 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author  : Dmitry Pischenko (dmitry.pischenko@toplab.io)
 */

#ifndef MSTESTCOMMANDLINEPARSER_H
#define MSTESTCOMMANDLINEPARSER_H

#include "PMCommandLineParser.h"
#include "MSReaderTypes.h"

class MSTestCommandLineParser : public PMCommandLineParser
{
    Q_DECLARE_TR_FUNCTIONS(MSTestCommandLineParser)
public:
    MSTestCommandLineParser();
};

#endif // MSTESTCOMMANDLINEPARSER_H
