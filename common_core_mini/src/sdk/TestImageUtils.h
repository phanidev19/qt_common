/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef TEST_IMAGE_UTILS_H
#define TEST_IMAGE_UTILS_H

#include "pmi_core_defs.h"

#include <QImage>
#include "pmi_common_core_mini_export.h"

_PMI_BEGIN

namespace TestImageUtils {
    //! Helper to generate diff between two images
    PMI_COMMON_CORE_MINI_EXPORT QImage generateDiff(const QImage &left, const QImage &right);
}

_PMI_END

#endif // TEST_IMAGE_UTILS_H