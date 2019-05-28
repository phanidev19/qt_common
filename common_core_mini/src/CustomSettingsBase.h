/*
 * Copyright (C) 2019 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Andrzej Żuraw azuraw@milosolutions.com
 */

#ifndef CUSTOMSETTINGSBASE_H
#define CUSTOMSETTINGSBASE_H

#include <pmi_common_core_mini_export.h>
#include <pmi_core_defs.h>

#include <QMap>
#include <QString>
#include <QVariant>

_PMI_BEGIN

/*!
    \brief Base class for storing advanced settings
*/
class PMI_COMMON_CORE_MINI_EXPORT CustomSettingsBase
{
public:
    CustomSettingsBase() = default;
    virtual ~CustomSettingsBase() = default;
    CustomSettingsBase(const CustomSettingsBase &) = default;
    CustomSettingsBase &operator=(const CustomSettingsBase &) = default;

    /**
     * \brief Clears all values from the settings
     */
    virtual void clear();

    /**
     * \brief Returns true, if the settings contains the value with the provided \a key, false
     * otherwise.
     */
    Q_INVOKABLE bool contains(const QString &key) const;

    /**
     * \brief Returns the value associated with the \a key.
     *
     * If the settings do not contain the value associated with the \a key, the function returns the
     * \a defaultValue.
     */
    Q_INVOKABLE const QVariant value(const QString &key, const QVariant &defaultValue) const;

    /**
     * \brief Returns settings as QMap
     */
    Q_INVOKABLE QMap<QString, QVariant> values() const;

    /**
     * \brief Returns the string that this Settings represents
     *
     * The ini format is used to present the settings as a string.
     */
    QString toString() const;

protected:
    /**
     * \brief Returns the string for a given \a settingsMap
     *
     * The ini format is used to present the \a settingsMap as a string.
     */
    QString toIniString(const QMap<QString, QVariant> &settingsMap) const;

protected:
    QMap<QString, QVariant> m_allSettingsMap;
};

_PMI_END

#endif // CUSTOMSETTINGSBASE_H
