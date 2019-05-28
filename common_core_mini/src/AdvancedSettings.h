/*
 * Copyright (C) 2012-2014 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 */


#ifndef ADVANCEDSETTINGS_H
#define ADVANCEDSETTINGS_H

#include "CustomSettingsBase.h"

#include <pmi_common_core_mini_export.h>

#include <QMap>
#include <QString>
#include <QVariant>

/*!
 * @brief Advanced Settings
 *
 * Contains a list of advanced settings and tweaks used around the
 * application.
 *
 * This is read-only, please create a file named advanced.ini that needs to be
 * located in CWD at runtime.
 *
 * @example
 *
 * [General]
 * conversion_rate=100.0
 * random_seed=1
 * window_closable=true
 *
 */
class PMI_COMMON_CORE_MINI_EXPORT AdvancedSettings: public QObject, public pmi::CustomSettingsBase
{
    Q_OBJECT
public:
    struct KeyProperty {
        KeyProperty()
            : fromIniFile(false)
            , showInDebugUI(false)
        {
        }

        KeyProperty(bool fromIniFile, bool showInDebugUI)
            : fromIniFile(fromIniFile)
            , showInDebugUI(showInDebugUI)
        {
        }

        bool fromIniFile;
        bool showInDebugUI;
    };

    explicit AdvancedSettings(QObject *parent = nullptr);
    ~AdvancedSettings();

    /**
     * @brief Returns a list containing all the keys in the settings map
     */
    const QStringList keys() const;

    /**
     * @brief Returns the size of the settings (number of keys stored in the settings map)
     */
    int size() const;

    /**
     * @brief Loads the advanced settings from a \a filename
     *
     * @param filename file path with advanced settings
     * @param clearSettingsMap - if true then the settings will be cleared before loading
     * @param keyProperty key property
     */
    void load(const QString &filename, bool clearSettingsMap,
              const KeyProperty &keyProperty = KeyProperty());

    /**
     * @brief Loads the custom settings (the font settings, the plot settings and the color settings)
     *
     * @param settingsFile file path
     */
    void loadCustomSettings(const QString &settingsFile);

    /**
     * @brief Saves the custom settings (the font settings, the plot settings and the color settings)
     *
     * @param settingsFile file path
     */
    void saveCustomSettings(const QString &settingsFile);

    /**
     * @brief Loads the settings from pmi_advanced.ini files
     *
     * Settings are loaded from the following directories:
     * 1. Organization data directory (parent directory for Application data directory) - "C:/Users/<USER>/AppData/Local/<ORGANIZATION_NAME>"
     * 2. Application data directory - "C:/Users/<USER>/AppData/Local/<ORGANIZATION_NAME>/<APP_NAME>"
     * 3. Directory that contains the application executable
     */
    void loadAdvancedSettings();

    /**
     * @brief Returns true if the color settings have been loaded, false otherwise
     */
    bool colorSettingsWereLoaded() const;

    /**
     * @brief Returns true if the font settings have been loaded, false otherwise
     */
    bool fontSettingsWereLoaded() const;

    /**
     * @brief Returns true if the plot settings have been loaded, false otherwise
     */
    bool plotSettingsWereLoaded() const;

    /**
     * @brief Sets the help string
     *
     * @param helpStr help string
     * @param append if true then the \a helpStr will be appended to the current string, if false
     * then then the contents of the current string will be cleared
     */
    void setHelp(const QString &helpStr, bool append = true);

    /**
     * @brief Sets the \a value for a given \a key
     */
    Q_INVOKABLE void set(const QString &key, const QVariant &value);

    /**
     * @brief Removes the item associated with the given \a key from the settings map
     */
    Q_INVOKABLE void unset(const QString &key);

    /*! Type of output stream */
    enum class OutputStreamType {
        Debug, /*!< Output stream for debug messages */
        Warning, /*!< Output stream for warning messages */
        Critical /*!< Output stream for critical messages */
    };

    /**
     * @brief Writes the \a value to the output stream
     *
     * @param value Value
     * @param type Type of output stream
     */
    Q_INVOKABLE void msg(const QString &value, OutputStreamType type = OutputStreamType::Debug) const;

    /**
     * @brief Returns the settings as a string
     *
     * The following format is used:
     * key1: value1
     * key2: value2
     * key3: value3
     * ...
     * keyN: valueN
     */
    Q_INVOKABLE QString show() const;

    /**
     * @brief Returns the help string
     *
     * \see setHelp
     */
    Q_INVOKABLE QString help() const;

public:
    static AdvancedSettings *Instance();

    /**
     * @brief if not found, this returns default init values
     *
     * @param key - property name
     *
     * @return property value
     */
    KeyProperty keyProperty(const QString &key) const;

    /**
     * @brief Sets the key property value for a given \a key
     *
     * @param key key
     * @param val value
     */
    void setKeyProperty(const QString &key, const KeyProperty &val);

    /**
     * @brief if function returns true, show dialog with settings from ini files.
     *
     * @return True: if any key have fromIniFile true. False otherwise.
     */
    bool containsKeysFromIniFile() const;

private:
    static AdvancedSettings *s_instance;

    QMap<QString, KeyProperty> m_keyProperty;  //populate this while reading from ini file.

    QString m_help;

    bool m_colorSettingsLoaded;
    bool m_fontSettingsLoaded;
    bool m_plotSettingsLoaded;

    const QString PMI_ADVANCED_INI;
};

/*!
 * @brief show debug user interface
 */
PMI_COMMON_CORE_MINI_EXPORT bool show_debug_ui();

/*!
 * @brief searches for show_debug_print=0 in advanced.ini
 */
PMI_COMMON_CORE_MINI_EXPORT bool show_debug_level(int level);

/*!
 * @brief get default font size
 */
PMI_COMMON_CORE_MINI_EXPORT QFont get_default_font();

/*!
 * @brief get default font size for plot
 */
PMI_COMMON_CORE_MINI_EXPORT QFont get_plot_title_font();

/*!
 * @brief get default font size for plot axis title
 */
PMI_COMMON_CORE_MINI_EXPORT QFont get_plot_axis_title_font();

/*!
 * @brief get default font size for plot axis
 */
PMI_COMMON_CORE_MINI_EXPORT QFont get_plot_axis_font();

/*!
 * @brief get main font for histogram fragments
 */
PMI_COMMON_CORE_MINI_EXPORT QFont get_plot_fragments_main_font();

/*!
 * @brief get sub font for histogram fragments
 */
PMI_COMMON_CORE_MINI_EXPORT QFont get_plot_fragments_sub_font();

/*!
 * @brief get default font for markers tooltip
 */
PMI_COMMON_CORE_MINI_EXPORT QFont get_markers_tooltip_font();

/*!
 * @brief get default colors for plots
 */
PMI_COMMON_CORE_MINI_EXPORT QString get_plot_colors();

/*!
 * @brief get default count of colors for plots
 */
PMI_COMMON_CORE_MINI_EXPORT int get_plot_color_count();

/*!
 * @brief get default font for plot labels
 */
PMI_COMMON_CORE_MINI_EXPORT QFont get_plot_labels_font();

PMI_COMMON_CORE_MINI_EXPORT QFont getDefaultFontSettings(const QString &fontType);

/*!
 * @brief Get application name. E.g. "ISSA" or "Byonic"
 */
PMI_COMMON_CORE_MINI_EXPORT QString getApplicationName();

/*!
 * @brief get default plot width
 */
PMI_COMMON_CORE_MINI_EXPORT int get_plot_width();

/*!
 * @brief get default grid lines size
 */
PMI_COMMON_CORE_MINI_EXPORT int get_grid_lines_size();

/*!
 * @brief get default size of circles in XIC plot
 */
PMI_COMMON_CORE_MINI_EXPORT int get_circles_xic_size();

/*!
 * @brief get default size of color dots in MS1 plot
 */
PMI_COMMON_CORE_MINI_EXPORT int get_color_dots_size();

/*!
 * @brief get default size of lines in vertical axis
 */
PMI_COMMON_CORE_MINI_EXPORT int get_size_of_lines_in_vertical_axis();

/*!
 * @brief get default size of lines in horizontal axis
 */
PMI_COMMON_CORE_MINI_EXPORT int get_size_of_lines_in_horizontal_axis();

PMI_COMMON_CORE_MINI_EXPORT int getDefaultPlotSettings(const QString &settingsType);

namespace as {
    PMI_COMMON_CORE_MINI_EXPORT QVariant value(const QString &key, const QVariant &defaultValue);
    PMI_COMMON_CORE_MINI_EXPORT void setValue(const QString &key, const QVariant &value);
    PMI_COMMON_CORE_MINI_EXPORT bool contains(const QString &key);
}

#endif
