/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "FilePathUtils.h"

#include <QDir>
#include <QFileInfo>

_PMI_BEGIN

void FilePathUtils::splitToDirAndFileName(const QString &filePath, QString *dir, QString *fileName)
{
    // split to path and filename
    QFileInfo fi(filePath);
    Q_ASSERT(fi.isAbsolute());

    if (dir) {
        *dir = fi.absoluteDir().absolutePath();
    }

    if (fileName) {
        *fileName = fi.fileName();
    }
}

_PMI_END

