/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Marcin Bartoszek mbartoszek@milosolutions.com
* Author: Andrzej Żuraw azuraw@milosolutions.com
*/

#ifndef CUSTOM_SETTINGS_H
#define CUSTOM_SETTINGS_H

#include "CustomSettingsBase.h"

#include <pmi_common_core_mini_export.h>
#include <pmi_core_defs.h>

#include <common_errors.h>

#include <QObject>
#include <QScopedPointer>
#include <QVariant>

class QSqlDatabase;
struct sqlite3;

_PMI_BEGIN

/*!
    \brief The CustomSettings class is responsible for storing the advanced configuration
*/
class PMI_COMMON_CORE_MINI_EXPORT CustomSettings : public CustomSettingsBase
{
public:
    /*! Scope of settings */
    enum class SettingsScope {
        Project, /*!< Refers to the advanced configuration from project
                                                 creation settings */
        User, /*!< Refers to the user advanced configuration */
        Document, /*!< Refers to the document advanced configuration (Project Creation Advanced
                     Configuration merged with User Advanced Configuration) */
        Advanced, /*!< Refers to the advanced configuration from pmi_advanced.ini file */
        All /*!< Refers to all possible advanced configurations (The document advanced configuration
               and the configuration from pmi_advanced.ini file) */
    };

    /**
     * \brief Returns true, if the settings contains the value with the provided \a key for a given
     * \a scope, false otherwise.
     */
    bool contains(const QString &key, SettingsScope scope = SettingsScope::All) const;

    /**
     * \brief Returns the value associated with the \a key for a given \a scope.
     *
     * If the settings do not contain the value associated with the \a key, the function returns the
     * \a defaultValue. If scope is \a SettingsScope::All and if the key exists in the document
     * settings and in the AdvancedSettings singleton then the value from the document settings is
     * returned.
     */
    QVariant value(const QString &key, const QVariant &defaultValue,
                   SettingsScope scope = SettingsScope::All) const;

    /**
     * \brief Sets the values.
     *
     * The settings are filled on the basis of \a values. \a values refer to \a
     * SettingsScope::Project and \a SettingsScope::User.
     *
     * \param values - advanced settings in the form of a string
     * \param clearSettingsMaps - if true then the settings will be cleared before setup
     */
    void setupValues(const QString &values, bool clearSettingsMaps = true);

    /**
     * \brief Sets the values.
     *
     * The settings are filled on the basis of \a userAdvancedOptions and \a
     * projectCreationAdvancedOptions. \a userAdvancedOptions refer to \a SettingsScope::User.
     * \a projectCreationAdvancedOptions refer to \a SettingsScope::Project.
     *
     * \param userAdvancedOptions - user advanced settings in the form of a string
     * \param projectCreationAdvancedOptions - project creation advanced settings in the form of a
     * string
     * \param clearSettingsMaps - if true then the settings will be cleared before setup
     */
    void setupValues(const QString &userAdvancedOptions,
                     const QString &projectCreationAdvancedOptions, bool clearSettingsMaps = true);

    /**
     * \brief Sets the values
     *
     * The settings are filled on the basis of \a settings.
     * \a settings refer to \a SettingsScope::Project and \a SettingsScope::User.
     *
     * \param settings - advanced settings in the form of a map
     */
    void setupValuesFromQMap(const QMap<QString, QVariant> &settings);

    /**
     * \brief Returns the string that this CustomSettings represents for a given \a scope.
     *
     * The ini format is used to present the settings as a string.
     */
    QString toString(SettingsScope scope) const;

    /**
     * \brief Returns true if the settings for a given \a scope contains no values, otherwise
     * returns false.
     */
    bool isEmpty(SettingsScope scope) const;

    /**
     * \brief Returns a map with values for a given \a scope.
     */
    const QMap<QString, QVariant> values(SettingsScope scope) const;

    /**
     * \brief Updates the user advanced settings based on \a settings
     *
     * Saves the user settings to the database and updates settings maps, except project creation settings map.
     */
    Err updateUserSettings(const CustomSettings &settings, QSqlDatabase &db);

    /**
     * \brief Updates the user advanced settings based on \a settings
     *
     * Saves the user settings to the database and updates settings maps, except project creation settings map.
     */
    Err updateUserSettings(const CustomSettings &settings, sqlite3 *db);

    /**
     * \reimp
     */
    void clear() override;

private:
    void updateSettingsMap(const CustomSettings &settings);

private:
    QMap<QString, QVariant> m_userSettingsMap;
    QMap<QString, QVariant> m_projectCreationSettingsMap;
};

_PMI_END

#endif // CUSTOM_SETTINGS_H
