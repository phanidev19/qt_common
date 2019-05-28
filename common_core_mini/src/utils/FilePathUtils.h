/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef FILE_PATH_UTILS_H
#define FILE_PATH_UTILS_H

#include <pmi_core_defs.h>

#include "pmi_common_core_mini_export.h"

#include <QString>

_PMI_BEGIN

/**
 * @brief FilePathUtils provides general functions for manipulating file paths
 *
 */
class PMI_COMMON_CORE_MINI_EXPORT FilePathUtils
{
public:
    //! \brief Takes absolute path @filePath and splits it into absolute path to directory and
    //! filename
    //
    // Example: C:\foo\bar\bas.txt -> dir: C:\foo\bar  fileName: bas.txt
    //
    // @param filePath absolute file path
    // @param dir absolute dir from filePath
    // @param fileName file name part from the file path provided
    static void splitToDirAndFileName(const QString &filePath, QString *dir, QString *fileName);
};

_PMI_END

#endif // FILE_PATH_UTILS_H