/*
 *  Copyright (C) 2019 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#ifndef UNIMODPARSERIMPLV2_0_H
#define UNIMODPARSERIMPLV2_0_H

#include "UnimodModifications.h"
#include "UnimodParserImplInterface.h"

class UnimodParserImplV2_0 : public UnimodParserImplInterface
{
public:
    UnimodParserImplV2_0();
    virtual ~UnimodParserImplV2_0();

    virtual pmi::Err parse(const QDomElement &unimodElement,
                           UnimodModifications *modifications) const override;

private:
    pmi::Err parseModifications(const QDomElement &element,
                                UnimodModifications *modifications) const;
    pmi::Err parseMod(const QDomElement &element, UnimodModifications::UmodMod *modification) const;
    pmi::Err parseSpecificity(const QDomElement &element,
                              UnimodModifications::UmodSpecificity *specificity) const;
    pmi::Err parseComposition(const QDomElement &element,
                              UnimodModifications::UmodComposition *composition) const;
    pmi::Err parseNeutralLoss(const QDomElement &element,
                              UnimodModifications::UmodNeutralLoss *neitralLoss) const;
    pmi::Err parsePepNeutralLoss(const QDomElement &element,
                                 UnimodModifications::UmodPepNeutralLoss *pepNeutralLoss) const;
    pmi::Err parseXref(const QDomElement &element, UnimodModifications::UmodXref *xref) const;
    pmi::Err parseElemRef(const QDomElement &element,
                          UnimodModifications::UmodElemRef *elemRef) const;
};

#endif // UNIMODPARSERIMPLV2_0_H
