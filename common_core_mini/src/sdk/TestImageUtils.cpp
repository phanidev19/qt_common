/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "TestImageUtils.h"

#include <QPainter>

_PMI_BEGIN

PMI_COMMON_CORE_MINI_EXPORT QImage TestImageUtils::generateDiff(const QImage &left, const QImage &right) {
    QImage result = left;
    QPainter painter(&result);
    painter.setCompositionMode(QPainter::CompositionMode_Difference);
    painter.drawImage(QPoint(0, 0), right);
    return result;
}

_PMI_END

