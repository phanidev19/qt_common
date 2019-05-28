#include "DefaultPlotsColors.h"

#include "PmiQtCommonConstants.h"

_PMI_BEGIN

const QString genericDefaultColors
    = "red, green, blue, orange, darkCyan, magenta, limeGreen, darkRed, darkGreen, darkBlue";

const QHash<QString, QString> defaultValues()
{
    QHash<QString, QString> values;
    values.insert(kintact, genericDefaultColors);
    values.insert(kbyomap, genericDefaultColors);
    values.insert(koxi, "red, green, blue, orange, darkCyan, magenta, limeGreen, darkRed, darkGreen, darkBlue");
    return values;
}

const QHash<QString, QString> DefaultPlotsColors::m_defaultPlotsColors = defaultValues();

const QString DefaultPlotsColors::defaultPlotsColors(const QString &appName)
{
    return m_defaultPlotsColors.value(appName, genericDefaultColors);
}

_PMI_END
