/*
 *  Copyright (C) 2019 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#include "UnimodParserImplV2_0.h"
#include "pmi_modifications_ui_debug.h"

#include <QDomElement>

using namespace pmi;

#define CHECK_ATTR(parent, attrName)                                                               \
    {                                                                                              \
        const bool hasAttribute = parent.hasAttribute(attrName);                                   \
        if (!hasAttribute) {                                                                       \
            warningModUi() << parent.tagName() << "does not contain attirbute" << attrName;        \
            rrr(kFileIncorrectTypeError);                                                          \
        }                                                                                          \
}

UnimodParserImplV2_0::UnimodParserImplV2_0()
{
}

UnimodParserImplV2_0::~UnimodParserImplV2_0()
{
}

Err UnimodParserImplV2_0::parse(const QDomElement &element,
                                UnimodModifications *modifications) const
{
    Q_ASSERT(modifications != nullptr);

    static const QString modificationsElementName = QStringLiteral("modifications");

    for (QDomNode node = element.firstChild(); !node.isNull(); node = node.nextSibling()) {
        QDomElement el = node.toElement();
        if (!el.isNull()) {
            if (el.localName() == modificationsElementName) {
                return parseModifications(el, modifications);
            }
        }
    }

    return kFileIncorrectTypeError;
}

pmi::Err UnimodParserImplV2_0::parseModifications(const QDomElement &element,
                                                  UnimodModifications *modifications) const
{
    static const QString modElementName = QStringLiteral("mod");

    QVector<UnimodModifications::UmodMod> mods;
    for (QDomNode node = element.firstChild(); !node.isNull(); node = node.nextSibling()) {
        QDomElement el = node.toElement();
        if (el.isNull()) {
            continue;
        }
        if (el.localName() == modElementName) {
            mods.push_back(UnimodModifications::UmodMod());
            const Err e = parseMod(el, &mods.last());
            if (e != kNoErr) {
                mods.removeLast();
            }
        }
    }
    modifications->setModifications(mods);

    return kNoErr;
}

pmi::Err UnimodParserImplV2_0::parseMod(const QDomElement &element,
                                        UnimodModifications::UmodMod *modification) const
{
    // attributes
    {
        static const QString titleAttributeName = QStringLiteral("title");
        static const QString fullNameAttributeName = QStringLiteral("full_name");
        static const QString usernameOfPosterAttributeName = QStringLiteral("username_of_poster");
        static const QString groupOfPosterAttributeName = QStringLiteral("group_of_poster");
        static const QString dateTimePostedAttributeName = QStringLiteral("date_time_posted");
        static const QString dateTimeModifiedAttributeName = QStringLiteral("date_time_modified");
        static const QString exCodeNameAttributeName = QStringLiteral("ex_code_name");
        static const QString approvedAttributeName = QStringLiteral("approved");
        static const QString recordIdAttributeName = QStringLiteral("record_id");

        // required attributes
        CHECK_ATTR(element, titleAttributeName);
        CHECK_ATTR(element, fullNameAttributeName);
        CHECK_ATTR(element, usernameOfPosterAttributeName);
        CHECK_ATTR(element, dateTimePostedAttributeName);
        CHECK_ATTR(element, dateTimeModifiedAttributeName);
        modification->title = element.attribute(titleAttributeName);
        modification->fullName = element.attribute(fullNameAttributeName);
        modification->usernameOfPoster = element.attribute(usernameOfPosterAttributeName);
        modification->dateTimePosted = element.attribute(dateTimePostedAttributeName);
        modification->dateTimeModified = element.attribute(dateTimeModifiedAttributeName);

        // optional attributes
        modification->groupOfPoster = element.attribute(groupOfPosterAttributeName);
        modification->exCodeName = element.attribute(exCodeNameAttributeName);
        if (element.hasAttribute(approvedAttributeName)) {
            modification->approvedSpecified = true;
            modification->approved = element.attribute(exCodeNameAttributeName).toInt();
        }
        if (element.hasAttribute(recordIdAttributeName)) {
            modification->recordIdSpecified = true;
            modification->recordId = element.attribute(exCodeNameAttributeName).toLong();
        }
    }

    // elements
    {
        static const QString specificityElementName = QStringLiteral("specificity");
        static const QString deltaElementName = QStringLiteral("delta");
        static const QString ignoreElementName = QStringLiteral("Ignore");
        static const QString xrefElementName = QStringLiteral("xref");
        static const QString altNameElementName = QStringLiteral("alt_name");
        static const QString miscNotesElementName = QStringLiteral("misc_notes");

        for (QDomNode node = element.firstChild(); !node.isNull(); node = node.nextSibling()) {
            QDomElement el = node.toElement();
            if (el.isNull()) {
                continue;
            }
            if (el.localName() == specificityElementName) {
                modification->specificity.push_back(UnimodModifications::UmodSpecificity());
                const Err e = parseSpecificity(el, &modification->specificity.last());
                ree;
                continue;
            }
            if (el.localName() == deltaElementName) {
                Q_ASSERT(modification->delta.composition.isEmpty());
                const Err e = parseComposition(el, &modification->delta);
                ree;
                continue;
            }
            if (el.localName() == ignoreElementName) {
                modification->ignore.push_back(UnimodModifications::UmodComposition());
                const Err e = parseComposition(el, &modification->ignore.last());
                ree;
                continue;
            }
            if (el.localName() == xrefElementName) {
                modification->xref.push_back(UnimodModifications::UmodXref());
                const Err e = parseXref(el, &modification->xref.last());
                ree;
                continue;
            }
            if (el.localName() == altNameElementName) {
                modification->altName.push_back(el.text());
                continue;
            }
            if (el.localName() == miscNotesElementName) {
                Q_ASSERT(modification->miscNotes.isEmpty());
                modification->miscNotes = el.text();
                continue;
            }
        }
    }

    return kNoErr;
}

pmi::Err
UnimodParserImplV2_0::parseSpecificity(const QDomElement &element,
                                       UnimodModifications::UmodSpecificity *specificity) const
{
    // attributes
    {
        static const QString hiddenAttributeName = QStringLiteral("hidden");
        static const QString siteAttributeName = QStringLiteral("site");
        static const QString positionAttributeName = QStringLiteral("position");
        static const QString classificationAttributeName = QStringLiteral("classification");
        //static const QString specGroupAttributeName = QStringLiteral("spec_group");

        // required attributes
        CHECK_ATTR(element, siteAttributeName);
        specificity->site = element.attribute(siteAttributeName);
        CHECK_ATTR(element, positionAttributeName);
        specificity->position
            = UnimodModifications::toPosition(element.attribute(positionAttributeName));
        CHECK_ATTR(element, classificationAttributeName);
        specificity->classification
            = UnimodModifications::toClassification(element.attribute(classificationAttributeName));

        // optional attributes
        specificity->hidden
            = element.attribute(hiddenAttributeName, "false") == "true" ? true : false;
        specificity->specGroup = element.attribute(hiddenAttributeName, "1").toInt();
    }

    // elements
    {
        static const QString neutralLossElementName = QStringLiteral("NeutralLoss");
        static const QString pepNeutralLossElementName = QStringLiteral("PepNeutralLoss");

        for (QDomNode node = element.firstChild(); !node.isNull(); node = node.nextSibling()) {
            QDomElement el = node.toElement();
            if (el.isNull()) {
                continue;
            }
            if (el.localName() == neutralLossElementName) {
                specificity->neutralLoss.push_back(UnimodModifications::UmodNeutralLoss());
                const Err e = parseNeutralLoss(el, &specificity->neutralLoss.last());
                ree;
                continue;
            }
            if (el.localName() == pepNeutralLossElementName) {
                specificity->pepNeutralLoss.push_back(UnimodModifications::UmodPepNeutralLoss());
                const Err e = parsePepNeutralLoss(el, &specificity->pepNeutralLoss.last());
                ree;
                continue;
            }
        }
    }

    return kNoErr;
}

pmi::Err
UnimodParserImplV2_0::parseComposition(const QDomElement &element,
                                       UnimodModifications::UmodComposition *composition) const
{
    // attributes
    {
        static const QString compositionAttributeName = QStringLiteral("composition");
        static const QString monoMassAttributeName = QStringLiteral("mono_mass");
        static const QString avgeMassAttributeName = QStringLiteral("avge_mass");

        // required attributes
        CHECK_ATTR(element, compositionAttributeName);
        composition->composition = element.attribute(compositionAttributeName);

        // optional attributes
        if (element.hasAttribute(monoMassAttributeName)) {
            composition->monoMassSpecified = true;
            composition->monoMass = element.attribute(monoMassAttributeName).toDouble();
        }
        if (element.hasAttribute(avgeMassAttributeName)) {
            composition->avgeMassSpecified = true;
            composition->avgeMass = element.attribute(avgeMassAttributeName).toDouble();
        }
    }

    // elements
    {
        static const QString elementElementName = QStringLiteral("element");

        for (QDomNode node = element.firstChild(); !node.isNull(); node = node.nextSibling()) {
            QDomElement el = node.toElement();
            if (el.isNull()) {
                continue;
            }
            if (el.localName() == elementElementName) {
                composition->element.push_back(UnimodModifications::UmodElemRef());
                const Err e = parseElemRef(el, &composition->element.last());
                ree;
                continue;
            }
        }
    }

    return kNoErr;
}

pmi::Err
UnimodParserImplV2_0::parseNeutralLoss(const QDomElement &element,
                                       UnimodModifications::UmodNeutralLoss *neitralLoss) const
{
    static const QString flagAttributeName = QStringLiteral("flag");

    // optional attributes
    neitralLoss->flag = element.attribute(flagAttributeName, "false") == "true" ? true : false;

    return parseComposition(element, neitralLoss);
}

pmi::Err UnimodParserImplV2_0::parsePepNeutralLoss(
    const QDomElement &element, UnimodModifications::UmodPepNeutralLoss *pepNeutralLoss) const
{
    static const QString requiredAttributeName = QStringLiteral("required");

    // optional attributes
    pepNeutralLoss->required
        = element.attribute(requiredAttributeName, "false") == "true" ? true : false;

    return parseComposition(element, pepNeutralLoss);
}

pmi::Err UnimodParserImplV2_0::parseXref(const QDomElement &element,
                                         UnimodModifications::UmodXref *xref) const
{
    static const QString textElementName = QStringLiteral("text");
    static const QString sourceElementName = QStringLiteral("source");
    static const QString urlElementName = QStringLiteral("url");

    for (QDomNode node = element.firstChild(); !node.isNull(); node = node.nextSibling()) {
        QDomElement el = node.toElement();
        if (el.isNull()) {
            continue;
        }
        if (el.localName() == textElementName) {
            Q_ASSERT(xref->text.isEmpty());
            xref->text = el.text();
            continue;
        }
        if (el.localName() == sourceElementName) {
            xref->source = UnimodModifications::toXrefSource(el.text());
            continue;
        }
        if (el.localName() == urlElementName) {
            Q_ASSERT(xref->url.isEmpty());
            xref->url = el.text();
            continue;
        }
    }

    return kNoErr;
}

pmi::Err UnimodParserImplV2_0::parseElemRef(const QDomElement &element,
                                            UnimodModifications::UmodElemRef *elemRef) const
{
    static const QString symbolAttributeName = QStringLiteral("symbol");
    static const QString numberAttributeName = QStringLiteral("number");

    // required attributes
    CHECK_ATTR(element, symbolAttributeName);
    elemRef->symbol = element.attribute(symbolAttributeName);

    // optional attributes
    elemRef->number = element.attribute(numberAttributeName, "1").toInt();

    return kNoErr;
}
