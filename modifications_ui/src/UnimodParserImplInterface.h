/*
 *  Copyright (C) 2019 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#ifndef UNIMODPARSERIMPLINTERFACE_H
#define UNIMODPARSERIMPLINTERFACE_H

#include <common_errors.h>

class QDomElement;
class UnimodModifications;

class UnimodParserImplInterface
{
public:
    virtual ~UnimodParserImplInterface() = default;

    virtual pmi::Err parse(const QDomElement& unimodElement, UnimodModifications* modifications) const = 0;
};

#endif // UNIMODPARSERIMPLINTERFACE_H
