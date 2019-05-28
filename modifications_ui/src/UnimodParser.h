/*
 *  Copyright (C) 2019 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#ifndef UNIMODPARSER_H
#define UNIMODPARSER_H

#include <common_errors.h>

#include <QString>

class UnimodModifications;
class UnimodParserImplInterface;

class UnimodParser
{
public:
    UnimodParser();
    ~UnimodParser();

    pmi::Err parse(const QString& filename, UnimodModifications* modifications);

private:
    UnimodParserImplInterface* createParser(int majorVersion, int minorVersion);
};

#endif // UNIMODPARSER_H
