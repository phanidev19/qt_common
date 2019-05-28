/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef MZ_SCANINDEX_RECT_H
#define MZ_SCANINDEX_RECT_H

#include "pmi_core_defs.h"

#include "MzInterval.h"
#include "ScanIndexInterval.h"
#include "pmi_common_tiles_export.h"

#include <QRectF>

_PMI_BEGIN

/**
* @brief Defines rectangular region defined by mz and scan index interval
*
* Combines double and integer precision in single entity
*/
class PMI_COMMON_TILES_EXPORT MzScanIndexRect
{
public:
    MzInterval mz;
    ScanIndexInterval scanIndex;

    bool isNull() const { return mz.isNull() && scanIndex.isNull(); }

    bool contains(const MzScanIndexRect &other) const
    {
        return mz.contains(other.mz) && scanIndex.contains(other.scanIndex);
    }

    MzScanIndexRect intersected(const MzScanIndexRect &other) const {
        MzScanIndexRect result;
        result.mz = mz.intersected(other.mz);
        result.scanIndex = scanIndex.intersected(other.scanIndex);
        return result;
    }

    QRectF toQRectF() const
    {
        QRectF result;

        result.setLeft(mz.start());
        result.setRight(mz.end());

        result.setTop(scanIndex.start());
        result.setBottom(scanIndex.end());

        return result;
    }

    static MzScanIndexRect fromQRectF(const QRectF &mzScanIndexRect)
    {
        MzScanIndexRect result;

        result.mz.rstart() = mzScanIndexRect.left();
        result.mz.rend() = mzScanIndexRect.right();

        result.scanIndex.rstart() = static_cast<int>(mzScanIndexRect.top());
        result.scanIndex.rend() = static_cast<int>(mzScanIndexRect.bottom());

        return result;
    }
};

_PMI_END

#endif // MZ_SCANINDEX_RECT_H