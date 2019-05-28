#ifndef DEFAULT_PLOTS_COLORS_H
#define DEFAULT_PLOTS_COLORS_H

#include <pmi_common_core_mini_export.h>
#include <pmi_core_defs.h>
#include <QHash>

_PMI_BEGIN

class PMI_COMMON_CORE_MINI_EXPORT DefaultPlotsColors
{
public:
    static const QString defaultPlotsColors(const QString &appName);

private:
    static const QHash<QString, QString> m_defaultPlotsColors;
};
_PMI_END

#endif // DEFAULT_PLOTS_COLORS_H
