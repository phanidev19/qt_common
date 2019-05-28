/*
 * Copyright (C) 2019 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Andrzej Żuraw azuraw@milosolutions.com
 */

#include "CustomSettingsBase.h"

#include "pmi_common_core_mini_debug.h"

#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QUuid>

_PMI_BEGIN

void CustomSettingsBase::clear()
{
    m_allSettingsMap.clear();
}

bool CustomSettingsBase::contains(const QString &key) const
{
    return m_allSettingsMap.contains(key);
}

const QVariant CustomSettingsBase::value(const QString &key, const QVariant &defaultValue) const
{
    return m_allSettingsMap.value(key, defaultValue);
}

QMap<QString, QVariant> CustomSettingsBase::values() const
{
    return m_allSettingsMap;
}

QString CustomSettingsBase::toString() const
{
    return toIniString(m_allSettingsMap);
}

QString CustomSettingsBase::toIniString(const QMap<QString, QVariant> &settingsMap) const
{
    if (settingsMap.isEmpty()) {
        return QString();
    }

    // open and close to create temp file. QSettings can't operate on QTemporaryFile
    const QString fileName = QStringLiteral("%1/%2").arg(
        QStandardPaths::writableLocation(QStandardPaths::TempLocation),
        QUuid::createUuid().toString());

    QSettings settings(fileName, QSettings::IniFormat);

    QMapIterator<QString, QVariant> iterator(settingsMap);
    while (iterator.hasNext()) {
        iterator.next();
        settings.setValue(iterator.key(), iterator.value());
    }
    settings.sync();

    QFile file(fileName);
    if (!file.open(QFile::ReadOnly)) {
        warningCoreMini() << "Could not open temp file for reading" << file.errorString();
        return QString();
    }
    const QByteArray bytes = file.readAll();
    file.close();
    if (!file.remove()) {
        warningCoreMini() << "Could not remove temp file";
    }

    return bytes;
}

_PMI_END
