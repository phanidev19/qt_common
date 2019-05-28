/*
 * Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Alex Prikhodko alex.prikhodko@proteinmetrics.com
 */

#include "PathUtils.h"
#include "pmi_common_core_mini_debug.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>

_PMI_BEGIN

static bool tryCombinePath(const QString &rootFolder, const QString &relFolder, const QString &bin,
    QString *path)
{
    const QString cleanedPath
        = QDir::cleanPath(QString("%1/%2/%3").arg(rootFolder).arg(relFolder).arg(bin));

    const QFileInfo fileInfo(cleanedPath);
    *path = fileInfo.absoluteFilePath();

    return fileInfo.exists();
}

// Get a full (existent) path from pwiz-pmi folder for a given file? getFullPathForPwizApp()?
static QString getAppWithSubFolderIfExists(const QString &file, const QString & subFolder)
{
    const QString appFolder = QCoreApplication::applicationDirPath();
    QString path;

    // Try app executable path
    if (tryCombinePath(appFolder, subFolder, file, &path)) {
        return path;
    }

    // New installer path
    if (tryCombinePath(appFolder, "../Tools/" + subFolder, file, &path)) {
        return path;
    }

    // Try a directory above the app executable
    if (tryCombinePath(appFolder, "../" + subFolder, file, &path)) {
        return path;
    }

    // Try working directory path, just in case.
    if (tryCombinePath(".", subFolder, file, &path)) {
        return path;
    }

    // Try above the working directory.
    if (tryCombinePath(".", "../" + subFolder, file, &path)) {
        return path;
    }

    // Could not find the binary
    return QString();
}

static const QString g_pwizSubfolder = QStringLiteral("pwiz-pmi");
static const QString g_baseFolder = QStringLiteral(".");

static QString getSubFolderBinaryPath(const QString &bin, const QString & subfolder)
{
    const QString path = getAppWithSubFolderIfExists(bin, subfolder);
    if (QFile::exists(path)) {
        return path;
    }

    warningCoreMini() << "Could not find" << bin << ", expected location:" << path;
    return QString();
}

PMI_COMMON_CORE_MINI_EXPORT QString PathUtils::getMSConvertExecPath()
{
#ifdef Q_OS_WIN
    const QString msConvertExecName = "msconvert-pmi.exe";
#else
    const QString msConvertExecName = "msconvert-pmi";
#endif

    return getSubFolderBinaryPath(msConvertExecName, g_pwizSubfolder);
}

PMI_COMMON_CORE_MINI_EXPORT QString PathUtils::getPicoExecPath()
{
#ifdef Q_OS_WIN
    const QString picoExecName = "pmi_pico_console.exe";
#else
    const QString picoExecName = "pmi_pico_console";
#endif

    return getSubFolderBinaryPath(picoExecName, g_pwizSubfolder);
}

PMI_COMMON_CORE_MINI_EXPORT QString PathUtils::getPMIMSConvertPath()
{
#ifdef Q_OS_WIN
    const QString execName = "PMi-MSConvert.exe";
#else
    const QString execName = "PMi-MSConvert";
#endif

    return getSubFolderBinaryPath(execName, g_baseFolder);
}

PMI_COMMON_CORE_MINI_EXPORT QString PathUtils::getPwizReaderDLLManifestPath(const QString &manifestname)
{
    QString path = getAppWithSubFolderIfExists(manifestname, g_pwizSubfolder);

    // works without it in Win7_64, but just in case other versions need it.
    path.replace("/", "\\");
    return path;
}

_PMI_END
