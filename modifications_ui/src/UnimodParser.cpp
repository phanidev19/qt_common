/*
 *  Copyright (C) 2019 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#include "UnimodParser.h"

#include "UnimodModifications.h"
#include "UnimodParserImplV2_0.h"

#include <QDomDocument>
#include <QFile>

using namespace pmi;

UnimodParser::UnimodParser()
{
}

UnimodParser::~UnimodParser()
{
}

Err UnimodParser::parse(const QString &filename, UnimodModifications *modifications)
{
    Q_ASSERT(modifications != nullptr);

    static const QString unimodElementName = QStringLiteral("unimod");
    static const QString majorVersionAttributeName = QStringLiteral("majorVersion");
    static const QString minorVersionAttributeName = QStringLiteral("minorVersion");

    QDomDocument doc;
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        return kFileOpenError;
    }
    if (!doc.setContent(&file, true)) {
        file.close();
        return kFileIncorrectTypeError;
    }
    file.close();

    QDomElement rootElement = doc.documentElement();
    if (rootElement.localName() != unimodElementName) {
        return kFileIncorrectTypeError;
    }
    const int majorVersion = rootElement.attribute(majorVersionAttributeName).toInt();
    const int minorVersion = rootElement.attribute(minorVersionAttributeName).toInt();

    UnimodParserImplInterface *parser = createParser(majorVersion, minorVersion);
    if (parser == nullptr) {
        return kFileIncorrectTypeError;
    }

    return parser->parse(rootElement, modifications);
}

UnimodParserImplInterface *UnimodParser::createParser(int majorVersion, int minorVersion)
{
    if (majorVersion == 2 && minorVersion == 0) {
        return new UnimodParserImplV2_0();
    }
    return nullptr;
}
