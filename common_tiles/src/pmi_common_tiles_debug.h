/*
 * Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 */

#ifndef PMI_COMMON_DEBUG_H
#define PMI_COMMON_DEBUG_H

//! @file pmi_common_debug.h
//! @short Module-level debug interfaces

#include <QLoggingCategory>
Q_DECLARE_LOGGING_CATEGORY(PMI_COMMON_TILES)

#define debugTiles(...) qCDebug(PMI_COMMON_TILES, __VA_ARGS__)
#define infoTiles(...) qCInfo(PMI_COMMON_TILES __VA_ARGS__)
#define warningTiles(...) qCWarning(PMI_COMMON_TILES, __VA_ARGS__)
#define criticalTiles(...) qCCritical(PMI_COMMON_TILES, __VA_ARGS__)

#endif
