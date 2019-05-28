/*
 *  Copyright (C) 2019 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#ifndef PMI_MODIFICATIONS_UI_DEBUG_H
#define PMI_MODIFICATIONS_UI_DEBUG_H

//! @file pmi_modifications_ui_debug.h
//! @short Module-level debug interfaces

#include <QLoggingCategory>
Q_DECLARE_LOGGING_CATEGORY(PMI_MODIFICATIONS_UI)

#define debugModUi(...) qCDebug(PMI_MODIFICATIONS_UI, __VA_ARGS__)
#define infoModUi(...) qCInfo(PMI_MODIFICATIONS_UI, __VA_ARGS__)
#define warningModUi(...) qCWarning(PMI_MODIFICATIONS_UI, __VA_ARGS__)
#define criticalModUi(...) qCCritical(PMI_MODIFICATIONS_UI, __VA_ARGS__)

#endif
