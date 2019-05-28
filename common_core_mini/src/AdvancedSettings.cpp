/*
 * Copyright (C) 2012-2014 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 */


#include "AdvancedSettings.h"

#include "DefaultPlotsColors.h"
#include "Global.h"
#include "PmiQtCommonConstants.h"
#include "pmi_common_core_mini_debug.h"

// pmi_common_stable
#include <qt_utils.h>

#include <QCoreApplication>
#include <QFont>
#include <QSettings>
#include <QStringList>

AdvancedSettings *AdvancedSettings::s_instance(nullptr);

AdvancedSettings::AdvancedSettings(QObject *parent)
    : QObject(parent)
    , PMI_ADVANCED_INI(QStringLiteral("pmi_advanced.ini"))
{
    if (!s_instance) {
        s_instance = this;

        m_colorSettingsLoaded = false;
        m_fontSettingsLoaded = false;
        m_plotSettingsLoaded = false;
    }
    m_help = "instructions:\n"
             "as.set(\"key\",value) -- to add/update advanced settings variables\n"
             "as.unset(\"key\") -- to remove advanced settings variables\n\n";
}

AdvancedSettings::~AdvancedSettings()
{
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

const QStringList AdvancedSettings::keys() const
{
    return m_allSettingsMap.keys();
}

int AdvancedSettings::size() const
{
    return m_allSettingsMap.size();
}

//Note: "data\ini\pmi_advanced.ini" keeps record of all known variables

// Current ini data flow can be viewed in help\data_flow\ini_data_flow.pptx
void AdvancedSettings::load(const QString &filename, bool clearSettingsMap,
                            const KeyProperty &keyProperty)
{
    QSettings settings(filename, QSettings::IniFormat);

    if (clearSettingsMap) {
        m_allSettingsMap.clear();
    }

    debugCoreMini() << "settings.childKeys()=" << settings.childKeys();

    for (const QString &key : settings.childKeys()) {
        m_allSettingsMap[key] = settings.value(key);
        setKeyProperty(key, keyProperty);
    }

    debugCoreMini() << "settings.childGroups()=" << settings.childGroups();

    //ML-2303: read also keys under groups
    for (const QString &group : settings.childGroups()) {
        settings.beginGroup(group);
        debugCoreMini() << "settings.childKeys() [" << group << "] =" << settings.childKeys();

        for (const QString &key : settings.childKeys()) {
            m_allSettingsMap[group + "/" + key] = settings.value(key);
            debugCoreMini() << "key=" << group + "/" + key << "value=" << settings.value(key);
            setKeyProperty(group + "/" + key, keyProperty);
        }
        settings.endGroup();
    }
}

void AdvancedSettings::set(const QString &key, const QVariant &value)
{
    m_allSettingsMap[key] = value;
}

void AdvancedSettings::unset(const QString &key)
{
    if (m_allSettingsMap.contains(key)) {
        m_allSettingsMap.remove(key);
    }
}

void AdvancedSettings::msg(const QString &value, OutputStreamType type) const
{
    if (type == OutputStreamType::Debug) {
        debugCoreMini() << value;
    } else if (type == OutputStreamType::Warning) {
        warningCoreMini() << value;
    } else if (type == OutputStreamType::Critical) {
        criticalCoreMini() << value;
    } else {
        debugCoreMini() << value << QStringLiteral(" type = ") << static_cast<int>(type);
    }
}

QString AdvancedSettings::show() const
{
    QString output;
    for (auto iter = m_allSettingsMap.constBegin(); iter != m_allSettingsMap.constEnd(); ++iter) {
        output += iter.key() + ": " + iter.value().toString() + '\n';
    }
    return output;
}

QString AdvancedSettings::help() const
{
    return m_help;
}

AdvancedSettings *AdvancedSettings::Instance() {
    //Normally we create an instance in the main.cpp,
    //but we can also avoid creating an instance and use
    //the static object in this function.  This avoids
    //possible return of null pointer.
    //Note: Yong doesn't like this class's way of providing
    //single pattern.  We should consider always creating a static object.
    //Double check on thread safety issue with static objects and shared DLLs
    //http://blogs.msdn.com/b/oldnewthing/archive/2004/03/08/85901.aspx
    static AdvancedSettings staticObj;
    if (s_instance == nullptr) {
        s_instance = &staticObj;
    }

    return s_instance;
}

bool AdvancedSettings::containsKeysFromIniFile() const
{
    for (const KeyProperty &kp : m_keyProperty.values()) {
        if (kp.fromIniFile) {
            return true;
        }
    }
    return false;
}

AdvancedSettings::KeyProperty AdvancedSettings::keyProperty(const QString &key) const
{
    return m_keyProperty.value(key);
}

void AdvancedSettings::setKeyProperty(const QString &key, const AdvancedSettings::KeyProperty &val)
{
    m_keyProperty[key] = val;
}

PMI_COMMON_CORE_MINI_EXPORT QString getApplicationName()
{
    return as::value(kAPPLICATION_NAME, QStringLiteral("UnknownAppName")).toString();
}

PMI_COMMON_CORE_MINI_EXPORT bool show_debug_level(int level)
{
    return as::value(kShowDebugLevel, level - 1).toInt() >= level;
}

/*arguments for QFonts:
 *QString QFont::toString() const
{
    const QChar comma( ',' );
    return family() + comma +
    QString::number(  pointSizeFloat() ) + comma +
    QString::number(       pixelSize() ) + comma +
    QString::number( (int) styleHint() ) + comma +
    QString::number(          weight() ) + comma +
    QString::number( (int)    italic() ) + comma +
    QString::number( (int) underline() ) + comma +
    QString::number( (int) strikeOut() ) + comma +
    QString::number( (int)fixedPitch() ) + comma +
    QString::number( (int)   rawMode() );
}
*/

static QFont getFont(const QString &fontTypeKey, const QString &fontSizeKey)
{
    QFont f;
    if (!AdvancedSettings::Instance()->fontSettingsWereLoaded()) {
        f = getDefaultFontSettings(fontTypeKey);
    } else {
        f.fromString(as::value(fontTypeKey, QString()).toString());
        f.setPointSize(as::value(fontSizeKey, QVariant()).toInt());
    }
    return f;
}

PMI_COMMON_CORE_MINI_EXPORT QFont get_default_font()
{
    return getDefaultFontSettings(QString());
}

PMI_COMMON_CORE_MINI_EXPORT QFont get_plot_title_font()
{
    return getFont(kplotTitleFont, kplotTitleFontSize);
}

PMI_COMMON_CORE_MINI_EXPORT QFont get_plot_axis_title_font()
{
    return getFont(kaxisTitleFont, kaxisTitleFontSize);
}

PMI_COMMON_CORE_MINI_EXPORT QFont get_plot_axis_font()
{
    return getFont(kaxisFont, kaxisFontSize);
}

PMI_COMMON_CORE_MINI_EXPORT QFont get_plot_labels_font()
{
    return getFont(klabelFont, klabelFontSize);
}

PMI_COMMON_CORE_MINI_EXPORT QFont get_plot_fragments_main_font()
{
    QFont f = getFont(kfragmentsMainFont, kfragmentsMainFontSize);
    f.setStyleHint(QFont::TypeWriter);
    return f;
}

PMI_COMMON_CORE_MINI_EXPORT QFont get_plot_fragments_sub_font()
{
    QFont f = getFont(kfragmentsSubFont, kfragmentsSubFontSize);
    f.setBold(false);  //note sure which of the string is bold; so just make it not bold here.
    return f;
}

PMI_COMMON_CORE_MINI_EXPORT QFont get_markers_tooltip_font()
{
    QFont f = getFont(kmarkersToolTipFont, kmarkersToolTipFontSize);
    f.setStyleHint(QFont::TypeWriter);
    return f;
}

inline void setFontSettingsToAS(const QSettings &settings, const QString &fontTypeKey, const QString &fontSizeKey)
{
    as::setValue(fontTypeKey, settings.value(fontTypeKey, getDefaultFontSettings(fontTypeKey).toString()));
    if (settings.contains(fontSizeKey)) {
        as::setValue(fontSizeKey, settings.value(fontSizeKey));
    } else {
        as::setValue(fontSizeKey, getDefaultFontSettings(fontTypeKey).pointSize());
    }
}

static void loadFontSettings(const QString &settingsFile)
{
    QSettings settings(settingsFile, QSettings::IniFormat);
    settings.beginGroup(kFontSettings);

    setFontSettingsToAS(settings, kplotTitleFont, kplotTitleFontSize);
    setFontSettingsToAS(settings, kaxisTitleFont, kaxisTitleFontSize);
    setFontSettingsToAS(settings, kaxisFont, kaxisFontSize);
    setFontSettingsToAS(settings, klabelFont, klabelFontSize);
    setFontSettingsToAS(settings, kfragmentsMainFont, kfragmentsMainFontSize);
    setFontSettingsToAS(settings, kfragmentsSubFont, kfragmentsSubFontSize);
    setFontSettingsToAS(settings, kmarkersToolTipFont, kmarkersToolTipFontSize);

    settings.endGroup();
}

static void saveFontSettings(const QString &settingsFile)
{
    QSettings settings(settingsFile, QSettings::IniFormat);
    settings.beginGroup(kFontSettings);

    settings.setValue(kplotTitleFont, as::value(kplotTitleFont, QVariant()));
    settings.setValue(kplotTitleFontSize, as::value(kplotTitleFontSize, QVariant()));

    settings.setValue(kaxisTitleFont, as::value(kaxisTitleFont, QVariant()));
    settings.setValue(kaxisTitleFontSize, as::value(kaxisTitleFontSize, QVariant()));

    settings.setValue(kaxisFont, as::value(kaxisFont, QVariant()));
    settings.setValue(kaxisFontSize, as::value(kaxisFontSize, QVariant()));

    settings.setValue(klabelFont, as::value(klabelFont, QVariant()));
    settings.setValue(klabelFontSize, as::value(klabelFontSize, QVariant()));

    settings.setValue(kfragmentsMainFont, as::value(kfragmentsMainFont, QVariant()));
    settings.setValue(kfragmentsMainFontSize, as::value(kfragmentsMainFontSize, QVariant()));

    settings.setValue(kfragmentsSubFont, as::value(kfragmentsSubFont, QVariant()));
    settings.setValue(kfragmentsSubFontSize, as::value(kfragmentsSubFontSize, QVariant()));

    settings.setValue(kmarkersToolTipFont, as::value(kmarkersToolTipFont, QVariant()));
    settings.setValue(kmarkersToolTipFontSize, as::value(kmarkersToolTipFontSize, QVariant()));

    settings.endGroup();
}

static void loadPlotSettings(const QString &settingsFile)
{
    QSettings settings(settingsFile, QSettings::IniFormat);
    settings.beginGroup(kPlotSettings);

    as::setValue(kplotWidthSize, settings.value(kplotWidthSize, getDefaultPlotSettings(kplotWidthSize)));
    as::setValue(kgridLinesSize, settings.value(kgridLinesSize, getDefaultPlotSettings(kgridLinesSize)));
    as::setValue(kcirclesSize, settings.value(kcirclesSize, getDefaultPlotSettings(kcirclesSize)));
    as::setValue(kcolorDotsSize, settings.value(kcolorDotsSize, getDefaultPlotSettings(kcolorDotsSize)));
    as::setValue(kverticalAxisLinesSize, settings.value(kverticalAxisLinesSize, getDefaultPlotSettings(kverticalAxisLinesSize)));
    as::setValue(khorizontalAxisLinesSize, settings.value(khorizontalAxisLinesSize, getDefaultPlotSettings(khorizontalAxisLinesSize)));

    settings.endGroup();
}

static void savePlotSettings(const QString &settingsFile)
{
    QSettings settings(settingsFile, QSettings::IniFormat);
    settings.beginGroup(kPlotSettings);

    settings.setValue(kplotWidthSize, as::value(kplotWidthSize, QVariant()));
    settings.setValue(kgridLinesSize, as::value(kgridLinesSize, QVariant()));
    settings.setValue(kcirclesSize, as::value(kcirclesSize, QVariant()));
    settings.setValue(kcolorDotsSize, as::value(kcolorDotsSize, QVariant()));
    settings.setValue(kverticalAxisLinesSize, as::value(kverticalAxisLinesSize, QVariant()));
    settings.setValue(khorizontalAxisLinesSize, as::value(khorizontalAxisLinesSize, QVariant()));

    settings.endGroup();
}

static void saveColorsSettings(const QString &settingsFile)
{
    QSettings settings(settingsFile, QSettings::IniFormat);
    settings.beginGroup(kColorsSettings);

    settings.setValue(kPlotsColorList, as::value(kPlotsColorList, QVariant()));
    settings.setValue(kPlotsColorCount, as::value(kPlotsColorCount, QVariant()));

    settings.endGroup();
}

static void loadColorsSettings(const QString &settingsFile)
{
    QSettings settings(settingsFile, QSettings::IniFormat);
    settings.beginGroup(kColorsSettings);

    const QString defaultPlotsColors
        = pmi::DefaultPlotsColors::defaultPlotsColors(getApplicationName());
    as::setValue(kPlotsColorList, settings.value(kPlotsColorList, defaultPlotsColors));
    as::setValue(kPlotsColorCount,
                 settings.value(kPlotsColorCount,
                                defaultPlotsColors.split(",", QString::SkipEmptyParts).size()));

    settings.endGroup();
}

// Current ini data flow can be viewed in help\data_flow\ini_data_flow.pptx
void AdvancedSettings::loadCustomSettings(const QString &settingsFile)
{
    loadFontSettings(settingsFile);
    m_fontSettingsLoaded = true;
    loadPlotSettings(settingsFile);
    m_plotSettingsLoaded = true;
    loadColorsSettings(settingsFile);
    m_colorSettingsLoaded = true;
}

// Current ini data flow can be viewed in help\data_flow\ini_data_flow.pptx
void AdvancedSettings::saveCustomSettings(const QString &settingsFile)
{
    saveFontSettings(settingsFile);
    savePlotSettings(settingsFile);
    saveColorsSettings(settingsFile);
}

void AdvancedSettings::loadAdvancedSettings()
{
    KeyProperty kp(true, true);
    //load pmi_advanced.ini from appData
    load(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/../" + PMI_ADVANCED_INI, false, kp);

    //load pmi_advanced.ini from appData/applicationDir
    load(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/" + PMI_ADVANCED_INI, false, kp);

    //load pmi_advanced.ini from .exe path
    load(qApp->applicationDirPath() + "/" + PMI_ADVANCED_INI, false, kp);
}

bool AdvancedSettings::colorSettingsWereLoaded() const
{
    return m_colorSettingsLoaded;
}

bool AdvancedSettings::fontSettingsWereLoaded() const
{
    return m_fontSettingsLoaded;
}

bool AdvancedSettings::plotSettingsWereLoaded() const
{
    return m_plotSettingsLoaded;
}

void AdvancedSettings::setHelp(const QString &helpStr, bool append)
{
    if (append) {
        m_help += helpStr;
    } else {
        m_help = helpStr;
    }
}

PMI_COMMON_CORE_MINI_EXPORT QFont getDefaultFontSettings(const QString &fontType)
{
    QFont f;
    f.fromString("MS Shell Dlg 2,8,-1,5,50,0,0,0,0,0");
    if (fontType.isEmpty()) {
        return f;
    }

    if (fontType == kplotTitleFont) {
        f.fromString("MS Shell Dlg 2,8,-1,5,50,0,0,0,0,0");
    } else if (fontType == kaxisTitleFont) {
        f.fromString("MS Shell Dlg 2,7,-1,5,50,0,0,0,0,0");
    } else if (fontType == kaxisFont) {
        f.fromString("MS Shell Dlg 2,7,-1,5,50,0,0,0,0,0");
    } else if (fontType == klabelFont) {
        f.fromString("MS Shell Dlg 2,8,-1,5,50,0,0,0,0,0");
    } else if (fontType == kfragmentsMainFont) {
        f.fromString("Monospaced,12,-1,5,75,0,0,0,0,0");
    } else if (fontType == kfragmentsSubFont) {
        f.fromString("Monospaced,7,-1,5,75,0,0,0,0,0");
    } else if (fontType == kmarkersToolTipFont) {
        f.fromString("MS Shell Dlg 2,7.25,-1,5,50,0,0,0,0,0");
    }

    return f;
}

PMI_COMMON_CORE_MINI_EXPORT QString get_plot_colors()
{
    if (!AdvancedSettings::Instance()->colorSettingsWereLoaded()) {
        return pmi::DefaultPlotsColors::defaultPlotsColors(getApplicationName());
    } else {
        return as::value(kPlotsColorList, QVariant()).toString();
    }
}

PMI_COMMON_CORE_MINI_EXPORT int get_plot_color_count()
{
    if (!AdvancedSettings::Instance()->colorSettingsWereLoaded()) {
        return pmi::DefaultPlotsColors::defaultPlotsColors(getApplicationName()).split(",", QString::SkipEmptyParts).size();
    } else {
        return as::value(kPlotsColorCount, QVariant()).toInt();
    }
}

bool show_debug_ui()
{
    return as::value(kShowDebugUI, false).toBool();
}

static int getPlotStyleSize(const QString &styleSizeKey)
{
    if (AdvancedSettings::Instance()->plotSettingsWereLoaded()) {
        return as::value(styleSizeKey, QVariant()).toInt();
    } else {
        return getDefaultPlotSettings(styleSizeKey);
    }
}

PMI_COMMON_CORE_MINI_EXPORT int get_plot_width()
{
    return getPlotStyleSize(kplotWidthSize);
}

PMI_COMMON_CORE_MINI_EXPORT int get_grid_lines_size()
{
    return getPlotStyleSize(kgridLinesSize);
}

PMI_COMMON_CORE_MINI_EXPORT int get_circles_xic_size()
{
    return getPlotStyleSize(kcirclesSize);
}

PMI_COMMON_CORE_MINI_EXPORT int get_color_dots_size()
{
    return getPlotStyleSize(kcolorDotsSize);
}

PMI_COMMON_CORE_MINI_EXPORT int get_size_of_lines_in_vertical_axis()
{
    return getPlotStyleSize(kverticalAxisLinesSize);
}

PMI_COMMON_CORE_MINI_EXPORT int get_size_of_lines_in_horizontal_axis()
{
    return getPlotStyleSize(khorizontalAxisLinesSize);
}

PMI_COMMON_CORE_MINI_EXPORT int getDefaultPlotSettings(const QString &settingsType)
{
    int res = 1;

    if (settingsType == kplotWidthSize) {
        res = 1;
    } else if (settingsType == kgridLinesSize) {
        res = 1;
    } else if (settingsType == kcirclesSize) {
        res = 9;
    } else if (settingsType == kcolorDotsSize) {
        res = 8;
    } else if (settingsType == kverticalAxisLinesSize) {
        res = 1;
    } else if (settingsType == khorizontalAxisLinesSize) {
        res = 1;
    }

    return res;
}

namespace as {

PMI_COMMON_CORE_MINI_EXPORT bool contains(const QString &key)
{
    return AdvancedSettings::Instance()->contains(key);
}

PMI_COMMON_CORE_MINI_EXPORT QVariant value(const QString &key, const QVariant &defaultValue)
{
    return AdvancedSettings::Instance()->value(key, defaultValue);
}

PMI_COMMON_CORE_MINI_EXPORT void setValue(const QString &key, const QVariant &value)
{
    AdvancedSettings::Instance()->set(key, value);
}


} //as

