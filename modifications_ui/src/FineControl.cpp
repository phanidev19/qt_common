/*
 *  Copyright (C) 2019 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#include "FineControl.h"

#include "pmi_modifications_ui_debug.h"

static const int GLYCAN_TYPE_INDEX = 0;
static const int GLYCAN_DB_INDEX = 1;
static const int GLYCAN_INDEX = 2;
static const int FINE_INDEX = 3;
static const int MOD_MAX_INDEX = 4;
static const int LAST_PARENT_INDEX = 4;

static QString getStaticText(bool proteomeDiscoverer_2_0)
{
    return proteomeDiscoverer_2_0 ? QStringLiteral("Static") : QStringLiteral("Fixed");
}

static QString getDynamicText(bool proteomeDiscoverer_2_0)
{
    return proteomeDiscoverer_2_0 ? QStringLiteral("Dynamic") : QStringLiteral("Variable");
}

bool FineControl::isValid(int intValue)
{

    return intValue >= static_cast<int>(FineType::Fixed)
        && intValue <= static_cast<int>(FineType::CommonGreaterThan2);
}

FineControl::FineType FineControl::fromInt(int intValue)
{
    if (isValid(intValue)) {
        return static_cast<FineType>(intValue);
    }
    return FineControl::defaultFine;
}

QString FineControl::toComboBoxDisplayString(FineType fineType, bool proteomeDiscoverer_2_0)
{
    switch (fineType) {
    case FineType::Fixed:
        return getStaticText(proteomeDiscoverer_2_0);
    case FineType::Rare1:
        return getDynamicText(proteomeDiscoverer_2_0) + QStringLiteral(" - rare 1");
    case FineType::Rare2:
        return getDynamicText(proteomeDiscoverer_2_0) + QStringLiteral(" - rare 2");
    case FineType::Common1:
        return getDynamicText(proteomeDiscoverer_2_0) + QStringLiteral(" - common 1");
    case FineType::Common2:
        return getDynamicText(proteomeDiscoverer_2_0) + QStringLiteral(" - common 2");
    case FineType::CommonGreaterThan2:
        return getDynamicText(proteomeDiscoverer_2_0) + QStringLiteral(" - common >2");
    default:; // do nothing
    }
    return QString();
}

QStringList FineControl::getValidByonicStrings(FineType fineType, int modMaxValue)
{
    switch (fineType) {
    case FineType::Fixed:
        return QStringList({ QStringLiteral("fixed") });
    case FineType::Rare1:
        return QStringList({ QStringLiteral("rare1"), QStringLiteral("rare") });
    case FineType::Rare2:
        return QStringList({ QStringLiteral("rare2") });
    case FineType::Common1:
        return QStringList({ QStringLiteral("common1"), QStringLiteral("common") });
    case FineType::Common2:
        return QStringList({ QStringLiteral("common2") });
    case FineType::CommonGreaterThan2:
        return QStringList(
            { (modMaxValue > 0 ? QStringLiteral("common%1").arg(modMaxValue) : "") });
    default:; // do nothing
    }
    return QStringList({ QString() });
}

QString FineControl::toByonicString(FineType fineType, int modMaxValue)
{
    return getValidByonicStrings(fineType, modMaxValue)[0];
}
