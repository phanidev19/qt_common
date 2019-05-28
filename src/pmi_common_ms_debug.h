/*
 * Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 */

#ifndef PMI_COMMON_MS_DEBUG_H
#define PMI_COMMON_MS_DEBUG_H

//! @file pmi_common_ms_debug.h
//! @short Module-level debug interfaces

#include <QLoggingCategory>
Q_DECLARE_LOGGING_CATEGORY(PMI_COMMON_MS)

#define debugMs(...) qCDebug(PMI_COMMON_MS, __VA_ARGS__)
#define infoMs(...) qCInfo(PMI_COMMON_MS, __VA_ARGS__)
#define warningMs(...) qCWarning(PMI_COMMON_MS, __VA_ARGS__)
#define criticalMs(...) qCCritical(PMI_COMMON_MS, __VA_ARGS__)

#endif
