/*
 *  Copyright (C) 2019 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#ifndef FINECONTROL_H
#define FINECONTROL_H

#include <QString>

class FineControl
{
public:
    enum class FineType { Fixed, Rare1, Rare2, Common1, Common2, CommonGreaterThan2 };

    static const FineType defaultFine = FineType::Fixed;

    static bool isValid(int intValue);
    static FineType fromInt(int intValue);

    static QString toComboBoxDisplayString(FineType fineType, bool proteomeDiscoverer_2_0);
    static QStringList getValidByonicStrings(FineType fineType, int modMaxValue = 0);
    static QString toByonicString(FineType fineType, int modMaxValue = 0);
};

#endif // FINECONTROL_H
