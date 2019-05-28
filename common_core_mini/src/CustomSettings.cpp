/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Marcin Bartoszek mbartoszek@milosolutions.com
* Author: Andrzej Żuraw azuraw@milosolutions.com
*/

#include "CustomSettings.h"

#include "AdvancedSettings.h"
#include "RowNode.h"
#include "RowNodeOptionsCore.h"
#include "RowNodeUtils.h"
#include "PmiQtCommonConstants.h"
#include "pmi_common_core_mini_debug.h"

#include <QSettings>
#include <QTemporaryFile>

_PMI_BEGIN

static void setup(const QString &values, const QVector<QMap<QString, QVariant> *> &maps)
{
    if (maps.isEmpty()) {
        return;
    }

    // write content to temp file
    QTemporaryFile tempFile;
    if (!tempFile.open()) {
        warningCoreMini() << "Could not open temp file for writing" << tempFile.errorString();
        return;
    }

    tempFile.write(values.toUtf8());
    tempFile.close();

    // parse content from file with QSettings
    QSettings settings(tempFile.fileName(), QSettings::IniFormat);

    const QStringList &allKeys = settings.allKeys();
    for (const QString &key : allKeys) {
        for (QMap<QString, QVariant> *map : maps) {
            map->insert(key, settings.value(key));
        }
    }
}

static Err advancedOptions(const QString &settings, RowNode *rnOptions)
{
    if (!rn::setOption(*rnOptions, kAdvancedOptions, settings)) {
        warningCoreMini() << "Unable to set advanced filter options";
        return kError;
    }
    return kNoErr;
}

static QMap<QString, QVariant> mergeWithAS(const QMap<QString, QVariant> &settings)
{
    QMap<QString, QVariant> result = AdvancedSettings::Instance()->values();
    for (auto iter = settings.constBegin(); iter != settings.constEnd(); ++iter) {
        result.insert(iter.key(), iter.value());
    }
    return result;
}

bool CustomSettings::contains(const QString &key, SettingsScope scope) const
{
    switch (scope) {
    case SettingsScope::Project:
        return m_projectCreationSettingsMap.contains(key);
    case SettingsScope::User:
        return m_userSettingsMap.contains(key);
    case SettingsScope::Document:
        return m_allSettingsMap.contains(key);
    case SettingsScope::Advanced:
        return as::contains(key);
    case SettingsScope::All:
        return m_allSettingsMap.contains(key) || as::contains(key);
    }

    Q_UNREACHABLE();
    return false;
}

QVariant CustomSettings::value(const QString &key,
                               const QVariant &defaultValue, SettingsScope scope) const
{
    switch (scope) {
    case SettingsScope::Project:
        return m_projectCreationSettingsMap.value(key, defaultValue);
    case SettingsScope::User:
        return m_userSettingsMap.value(key, defaultValue);
    case SettingsScope::Document:
        return m_allSettingsMap.value(key, defaultValue);
    case SettingsScope::Advanced:
        return as::value(key, defaultValue);
    case SettingsScope::All:
        return m_allSettingsMap.value(key, as::value(key, defaultValue));
    }

    Q_UNREACHABLE();
    return QVariant();
}

void CustomSettings::setupValues(const QString &values, bool clearSettingsMaps)
{
    if (clearSettingsMaps) {
        clear();
    }

    setup(values, { &m_projectCreationSettingsMap, &m_allSettingsMap, &m_userSettingsMap });
}

void CustomSettings::setupValues(const QString &userAdvancedOptions,
                                 const QString &projectCreationAdvancedOptions, bool clearSettingsMaps)
{
    // m_allSettingsMap corresponds to the already deleted  userAdvancedSettings() function

    if (clearSettingsMaps) {
        clear();
    }

    setup(projectCreationAdvancedOptions, { &m_projectCreationSettingsMap, &m_allSettingsMap });
    setup(userAdvancedOptions, { &m_userSettingsMap, &m_allSettingsMap });
}

QString CustomSettings::toString(SettingsScope scope) const
{
    switch (scope) {
    case SettingsScope::Project:
        return toIniString(m_projectCreationSettingsMap);
    case SettingsScope::User:
        return toIniString(m_userSettingsMap);
    case SettingsScope::Document:
        return toIniString(m_allSettingsMap);
    case SettingsScope::Advanced:
        return AdvancedSettings::Instance()->toString();
    case SettingsScope::All:
        const QMap<QString, QVariant> settings = mergeWithAS(m_allSettingsMap);
        return toIniString(settings);
    }

    Q_UNREACHABLE();
    return QString();
}

void CustomSettings::setupValuesFromQMap(const QMap<QString, QVariant> &settings)
{
    for (auto it = settings.constBegin(); it != settings.constEnd(); ++it) {
        m_projectCreationSettingsMap.insert(it.key(), it.value());
        m_userSettingsMap.insert(it.key(), it.value());
        m_allSettingsMap.insert(it.key(), it.value());
    }
}

Err CustomSettings::updateUserSettings(const CustomSettings &settings, QSqlDatabase &db)
{
    RowNode rnOptions;
    Err e = advancedOptions(settings.toString(SettingsScope::User),
                            &rnOptions); ree;

    e = rn::createFilterOptionsTable(db, kFilterOptions); ree;
    e = rn::saveOptions(db, kFilterOptions, kAdvancedOptions, rnOptions); ree;

    updateSettingsMap(settings);

    return e;
}

Err CustomSettings::updateUserSettings(const CustomSettings &settings, sqlite3 *db)
{
    RowNode rnOptions;
    Err e = advancedOptions(settings.toString(SettingsScope::User),
                            &rnOptions); ree;

    e = rn::createFilterOptionsTable(db, kFilterOptions); ree;
    e = rn::saveOptions(db, kFilterOptions, kAdvancedOptions, rnOptions); ree;

    updateSettingsMap(settings);

    return e;
}

void CustomSettings::clear()
{
    m_projectCreationSettingsMap.clear();
    m_userSettingsMap.clear();
    m_allSettingsMap.clear();
}

void CustomSettings::updateSettingsMap(const CustomSettings &settings)
{
    m_userSettingsMap = settings.values(SettingsScope::User);
    m_allSettingsMap = m_projectCreationSettingsMap;

    for (auto it = m_userSettingsMap.constBegin(); it != m_userSettingsMap.constEnd(); ++it) {
        m_allSettingsMap.insert(it.key(), it.value());
    }
}

bool CustomSettings::isEmpty(SettingsScope scope) const
{
    switch (scope) {
    case SettingsScope::Project:
        return m_projectCreationSettingsMap.isEmpty();
    case SettingsScope::User:
        return m_userSettingsMap.isEmpty();
    case SettingsScope::Document:
        return m_allSettingsMap.isEmpty();
    case SettingsScope::Advanced:
        return AdvancedSettings::Instance()->values().isEmpty();
    case SettingsScope::All:
        return m_allSettingsMap.isEmpty() && AdvancedSettings::Instance()->values().isEmpty();
    }

    Q_UNREACHABLE();
    return true;
}

const QMap<QString, QVariant> CustomSettings::values(SettingsScope scope) const
{
    switch (scope) {
    case SettingsScope::Project:
        return m_projectCreationSettingsMap;
    case SettingsScope::User:
        return m_userSettingsMap;
    case SettingsScope::Document:
        return m_allSettingsMap;
    case SettingsScope::Advanced:
        return AdvancedSettings::Instance()->values();
    case SettingsScope::All:
        return mergeWithAS(m_allSettingsMap);
    }

    Q_UNREACHABLE();
    return {};
}

_PMI_END
