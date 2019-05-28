/*
 * Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Jarosław Staniek jstaniek@milosolutions.com
 */

#ifndef PMI_COMMON_CORE_MINI_DEBUG_H
#define PMI_COMMON_CORE_MINI_DEBUG_H

//! @file pmi_common_core_mini_debug.h
//! @short Module-level debug interfaces

#include <QLoggingCategory>
Q_DECLARE_LOGGING_CATEGORY(PMI_COMMON_CORE_MINI)

#define debugCoreMini(...) qCDebug(PMI_COMMON_CORE_MINI, __VA_ARGS__)
#define infoCoreMini(...) qCInfo(PMI_COMMON_CORE_MINI, __VA_ARGS__)
#define warningCoreMini(...) qCWarning(PMI_COMMON_CORE_MINI, __VA_ARGS__)
#define criticalCoreMini(...) qCCritical(PMI_COMMON_CORE_MINI, __VA_ARGS__)

#endif
