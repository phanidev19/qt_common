/*
 * Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Alex Prikhodko alex.prikhodko@proteinmetrics.com
 */
#ifndef __PATH_UTILS_H__
#define __PATH_UTILS_H__

#include <QString>

#include <pmi_common_core_mini_export.h>
#include <pmi_core_defs.h>

_PMI_BEGIN

namespace PathUtils
{

PMI_COMMON_CORE_MINI_EXPORT QString getMSConvertExecPath();
PMI_COMMON_CORE_MINI_EXPORT QString getPicoExecPath();
PMI_COMMON_CORE_MINI_EXPORT QString getPMIMSConvertPath();
PMI_COMMON_CORE_MINI_EXPORT QString getPwizReaderDLLManifestPath(const QString &manifestname);
}
_PMI_END

#endif
